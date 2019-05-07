
inline v4
Unpack4x8(uint32 Packed)
{
	v4 Result =
	{
		(real32)((Packed >> 16) & 0xFF),
		(real32)((Packed >> 8) & 0xFF),
		(real32)((Packed >> 0) & 0xFF),
		(real32)((Packed >> 24) & 0xFF)
	};

	return(Result);
}

inline v4
UscaleAndBiasNormal(v4 Normal)
{
	v4 Result;

	real32 Inv255 = 1.0f / 255.0f;

	Result.x = -1.0f + 2.0f*(Inv255*Normal.x);
	Result.y = -1.0f + 2.0f*(Inv255*Normal.y);
	Result.z = -1.0f + 2.0f*(Inv255*Normal.z);

	Result.w = Inv255*Normal.w;

	return(Result);
}

internal void
DrawRectangle(loaded_bitmap *Buffer, v2 vMin,v2 vMax, v4 Color, rectangle2i ClipRect, bool Even)
{
	real32 R = Color.r;
	real32 G = Color.g;
	real32 B = Color.b;
	real32 A = Color.a;

	rectangle2i FillRect;
	FillRect.MinX = RoundReal32ToInt32(vMin.x);
	FillRect.MinY = RoundReal32ToInt32(vMin.y);
	FillRect.MaxX = RoundReal32ToInt32(vMax.x);
	FillRect.MaxY = RoundReal32ToInt32(vMax.y);

	FillRect = Intersect(FillRect, ClipRect);

	if (!Even == (FillRect.MinY & 1))
	{
		FillRect.MinY += 1;
	}

	uint32 Color32 = 
		(RoundReal32ToUInt32(A * 255.0f) << 24) |
		(RoundReal32ToUInt32(R * 255.0f) << 16) |
		(RoundReal32ToUInt32(G * 255.0f) << 8) |
		(RoundReal32ToUInt32(B * 255.0f));
	
	uint8 *Row = 
			((uint8 *)Buffer->Memory + 
			FillRect.MinX*BITMAP_BYTES_PER_PIXEL +
			FillRect.MinY*Buffer->Pitch);
	
	for (int Y = FillRect.MinY;
		Y < FillRect.MaxY;
		Y += 2)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = FillRect.MinX;
			X < FillRect.MaxX;
			++X)
		{
			*Pixel++ = Color32;
		}

		Row += 2*Buffer->Pitch;
	}
}


struct bilinear_sample
{
	uint32 A, B, C, D;
};

inline bilinear_sample
BilinearSample(loaded_bitmap *Texture, int32 X, int32 Y)
{
	bilinear_sample Result;

	uint8 *TexelPtr = (uint8 *)Texture->Memory + Y*Texture->Pitch + X*BITMAP_BYTES_PER_PIXEL;
	Result.A = *(uint32 *)(TexelPtr);
	Result.B = *(uint32 *)(TexelPtr + BITMAP_BYTES_PER_PIXEL);
	Result.C = *(uint32 *)(TexelPtr + Texture->Pitch);
	Result.D = *(uint32 *)(TexelPtr + Texture->Pitch + BITMAP_BYTES_PER_PIXEL);

	return(Result);
}

inline v4
SRGBBilinearBlend(bilinear_sample TexelSample, real32 fX, real32 fY)
{
	v4 TexelA = Unpack4x8(TexelSample.A);
	v4 TexelB = Unpack4x8(TexelSample.B);
	v4 TexelC = Unpack4x8(TexelSample.C);
	v4 TexelD = Unpack4x8(TexelSample.D);

	// Go from sRGB to "linear" brightness space
	TexelA = SRGB255ToLinear1(TexelA);
	TexelB = SRGB255ToLinear1(TexelB);
	TexelC = SRGB255ToLinear1(TexelC);
	TexelD = SRGB255ToLinear1(TexelD);

	v4 Result = Lerp(Lerp(TexelA, fX, TexelB), fY, Lerp(TexelC, fX, TexelD));
	
	return(Result);
}

inline v3
SampleEnvironmentMap(v2 ScreenSpaceUV, v3 SampleDirection, real32 Roughness, environment_map *Map,
	real32 DistanceFromMapInZ)
{
	/* NOTE:
		ScreenSpaceUV tells where the ray is being cast _from_ in
		normalized screen coordinates.

		SampleDirection tells us that direction the cast is going
		it does not gave to ve normalized.

		Roughess says which LODs of Map we sample from
	*/

	// NOTE: Pick which LOD to sample from
	uint32 LODIndex = (uint32)(Roughness*(real32)(ArrayCount(Map->LOD) - 1) + 0.5f);
	Assert(LODIndex < ArrayCount(Map->LOD));

	loaded_bitmap *LOD = &Map->LOD[LODIndex];

	// NOTE: Compute the distance to the map and the scaling
	// factor for meters-to-UVs
	// TODO: Paramaterize this and should be different for X and Y based on map 
	real32 UVsPerMeter = 0.01f;
	real32 C = (UVsPerMeter*DistanceFromMapInZ) / SampleDirection.y;
	v2 Offset = C * V2(SampleDirection.x, SampleDirection.z);
	
	// NOTE: Find the intersection point
	v2 UV = ScreenSpaceUV + Offset;

	// NOTE: Clamp to the valid range
	UV.x = Clamp01(UV.x);
	UV.y = Clamp01(UV.y);

	// NOTE: Bilinear sample
	real32 tX = (UV.x*(real32)(LOD->Width - 2));
	real32 tY = (UV.y*(real32)(LOD->Height - 2));

	int32 X = (int32)tX;
	int32 Y = (int32)tY;

	real32 fX = tX - (real32)X;
	real32 fY = tY - (real32)Y;

	Assert((X >= 0) && (X < LOD->Width));
	Assert((Y >= 0) && (Y < LOD->Height));

#if DEBUGUI_ShowLightingSamples
	uint8 *TexelPtr = (uint8 *)LOD->Memory + Y*LOD->Pitch + X*BITMAP_BYTES_PER_PIXEL;
	*(uint32 *)TexelPtr = 0xFFFFFFFF;
#endif
	bilinear_sample Sample = BilinearSample(LOD, X, Y);
	v3 Result = SRGBBilinearBlend(Sample, fX, fY).xyz;

	return(Result);
}

internal void
DrawRectangleSlowly(loaded_bitmap *Buffer,
	v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
	loaded_bitmap *Texture, loaded_bitmap *NormalMap,
	environment_map *Top, environment_map *Middle, environment_map *Bottom,
	real32 PixelsToMeters)
{
	TIMED_FUNCTION();
	
	// NOTE: Premultiply color up front
	Color.rgb *= Color.a;

	// TODO: This will need to be specified separateley

	real32 XAxisLength = Length(XAxis);
	real32 YAxisLength = Length(YAxis);

	v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
	v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;

	// NOTE: NzScale could be a parameter if we want people to
	// have control over the amount of scaling in the Z direction
	// that the normals appear to have
	real32 NzScale = 0.5f*(XAxisLength + YAxisLength);

	real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
	real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

	uint32 Color32 = 
		(RoundReal32ToUInt32(Color.a * 255.0f) << 24) |
		(RoundReal32ToUInt32(Color.r * 255.0f) << 16) |
		(RoundReal32ToUInt32(Color.g * 255.0f) << 8) |
		(RoundReal32ToUInt32(Color.b * 255.0f));
		
	v2 P[4] = {Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis};

	int WidthMax = Buffer->Width - 1;
	int HeightMax = Buffer->Height - 1;

	real32 InvWidthMax = 1.0f / (real32)WidthMax;
	real32 InvHeightMax = 1.0f / (real32)HeightMax;
	
	real32 OriginZ = 0.5f;
	real32 OriginY = (Origin + 0.5f*XAxis + 0.5f*YAxis).y;
	real32 FixedCastY = InvHeightMax*OriginY;

	int YMin = HeightMax;
	int YMax = 0;
	int XMin = WidthMax;
	int XMax = 0;

	for (int PIndex = 0;
		PIndex < ArrayCount(P);
		++PIndex)
	{
		v2 TestP = P[PIndex];
		int FloorX = FloorReal32ToInt32(TestP.x);
		int CeilX = CeilReal32ToInt32(TestP.x);
		int FloorY = FloorReal32ToInt32(TestP.y);
		int CeilY = CeilReal32ToInt32(TestP.y);

		if (XMin > FloorX) {XMin = FloorX;}
		if (YMin > FloorY) {YMin = FloorY;}
		if (XMax < CeilX) {XMax = CeilX;}
		if (YMax < CeilY) {YMax = CeilY;}
	}

	if (XMin < 0) {XMin = 0;}
	if (YMin < 0) {YMin = 0;}
	if (XMax > WidthMax) {XMax = WidthMax;}
	if (YMax > HeightMax) {YMax = HeightMax;}

	uint8 *Row = 
		((uint8 *)Buffer->Memory + 
		XMin*BITMAP_BYTES_PER_PIXEL +
		YMin*Buffer->Pitch);

	TIMED_BLOCK(PixelFill, (XMax - XMin + 1)*(YMax - YMin + 1));
	for (int Y = YMin;
		Y < YMax;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = XMin;
			X < XMax;
			++X)
		{
			v2 PixelP = V2i(X, Y);
			v2 d = PixelP - Origin;
			// TODO: PerpInner
			// TODO: Simple origin
			real32 Edge0 = Inner(d, -Perp(XAxis));
			real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
			real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
			real32 Edge3 = Inner(d - YAxis, Perp(YAxis));
			if ((Edge0 < 0) &&
				(Edge1 < 0) && 
				(Edge2 < 0) &&
				(Edge3 < 0))
			{
				v2 ScreenSpaceUV = {InvWidthMax*(real32)X, FixedCastY};

				real32 ZDiff = PixelsToMeters*((real32)Y - OriginY);

				real32 U = InvXAxisLengthSq*Inner(d, XAxis);
				real32 V = InvYAxisLengthSq*Inner(d, YAxis);

#if 0
				// TODO: SSE clamping
				Assert((U >= 0.0f) && (U <= 1.0f));
				Assert((V >= 0.0f) && (V <= 1.0f));
#endif
				// TODO: Formalize texture boundaries!
				real32 tX = (U*(real32)(Texture->Width - 2));
				real32 tY = (V*(real32)(Texture->Height - 2));

				int32 X = (int32)tX;
				int32 Y = (int32)tY;

				real32 fX = tX - (real32)X;
				real32 fY = tY - (real32)Y;

				Assert((X >= 0) && (X < Texture->Width));
				Assert((Y >= 0) && (Y < Texture->Height));

				bilinear_sample TexelSample = BilinearSample(Texture, X, Y);

				v4 Texel = SRGBBilinearBlend(TexelSample, fX, fY);
				
#if 0	
				if (NormalMap)
				{
					bilinear_sample NormalSample = BilinearSample(NormalMap, X, Y);
					
					v4 NormalA = Unpack4x8(NormalSample.A);
					v4 NormalB = Unpack4x8(NormalSample.B);
					v4 NormalC = Unpack4x8(NormalSample.C);
					v4 NormalD = Unpack4x8(NormalSample.D);

					v4 Normal = Lerp(
						Lerp(NormalA, fX, NormalB),
						fY,
						Lerp(NormalC, fX, NormalD));

					Normal = UscaleAndBiasNormal(Normal);
					// TODO: Do we really need to do this
					Normal.xy = Normal.x * NxAxis + Normal.y * NyAxis;
					Normal.z *= NzScale;
					Normal.xyz = Normalize(Normal.xyz);
					
					// NOTE: The eye vector is always assumed to be [0, 0, 1]
					// This is just the simplified version of reflection -e + 2e^T N N
					v3 BounceDirection = 2.0f*Normal.z*Normal.xyz;
					BounceDirection.z -= 1.0f;

					// TODO: Eventually we need to  upport two mappings,
					// one for top-down view (which we don't do now) and one
					// for sideways, which is what's happaning here.
					BounceDirection.z = -BounceDirection.z;

					environment_map *FarMap = 0;
					real32 Pz = OriginZ + ZDiff;
					real32 MapZ = 2.0f;
					real32 tEnvMap = BounceDirection.y;
					real32 tFarMap = 0.0f;
					if (tEnvMap < -0.5f)
					{
						FarMap = Bottom;
						tFarMap = -1.0f - 2.0f*tEnvMap;
					}
					else if (tEnvMap > 0.5f)
					{
						FarMap = Top;
						tFarMap = 2.0f*(tEnvMap - 0.5f);
					}

					v3 LightColor = {0 ,0, 0};// How do we sapmle from the middle map
					if (FarMap)
					{
						real32 DistanceFromMapInZ = FarMap->Pz - Pz;
						v3 FarMapColor = SampleEnvironmentMap(ScreenSpaceUV, BounceDirection, Normal.w, FarMap,
							DistanceFromMapInZ);
						LightColor = Lerp(LightColor, tFarMap, FarMapColor);
					}
					Texel.rgb = Texel.rgb + Texel.a*LightColor;
				
#if 0
					// NOTE: Draws the bounce direction
					Texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f*BounceDirection;
					Texel.rgb *= Texel.a;
#endif
				}
#endif

				Texel = Hadamard(Texel, Color);
				Texel.r = Clamp01(Texel.r);
				Texel.g = Clamp01(Texel.g);
				Texel.b = Clamp01(Texel.b);

				v4 Dest = 
					{(real32)((*Pixel >> 16) & 0xFF),
					(real32)((*Pixel >> 8) & 0xFF),
					(real32)((*Pixel >> 0) & 0xFF),
					(real32)((*Pixel >> 24) & 0xFF)};

				Dest = SRGB255ToLinear1(Dest);

				v4 Blended = (1.0f-Texel.a)*Dest + Texel;

				// Go from "linear" brightness space to sRGB
				v4 Blended255 = Linear1ToSRGB255(Blended);

 				*Pixel = 
					(((uint32)(Blended255.a + 0.5f) << 24)|
					((uint32)(Blended255.r + 0.5f) << 16) |
					((uint32)(Blended255.g + 0.5f) << 8) |
					((uint32)(Blended255.b + 0.5f)) << 0);

			}
			++Pixel;
		}

		Row += Buffer->Pitch;
	}
}


internal void
DrawBitmap(
	loaded_bitmap *Buffer, loaded_bitmap *Bitmap,
	real32 RealX, real32 RealY, real32 CAlpha = 1.0f)
{
	int MinX = RoundReal32ToInt32(RealX);
	int MinY = RoundReal32ToInt32(RealY);
	int MaxX = MinX + (real32)Bitmap->Width;
	int MaxY = MinY + (real32)Bitmap->Height;

	int32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	int32 SourceOffsetY = 0;
	if (MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}

	if (MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	uint8 *SourceRow = (uint8 *)Bitmap->Memory + SourceOffsetY*Bitmap->Pitch + BITMAP_BYTES_PER_PIXEL*SourceOffsetX;
	uint8 *DestRow = 
			((uint8 *)Buffer->Memory + 
			MinX*BITMAP_BYTES_PER_PIXEL +
			MinY*Buffer->Pitch);

	for (int32 Y = MinY;
		Y < MaxY;
		++Y)
	{
		uint32 *Dest = (uint32 *)DestRow;
		uint32 *Source = (uint32 *)SourceRow;
		for (int32 X = MinX;
			X < MaxX;
			++X)
		{
			v4 Texel = 
			{
				(real32)((*Source >> 16) & 0xFF),
				(real32)((*Source >> 8) & 0xFF),
				(real32)((*Source >> 0) & 0xFF),
				(real32)((*Source >> 24) & 0xFF)
			};
			Texel = SRGB255ToLinear1(Texel);
			Texel *= CAlpha;

			v4 D = 
			{
				(real32)((*Dest >> 16) & 0xFF),
				(real32)((*Dest >> 8) & 0xFF),
				(real32)((*Dest >> 0) & 0xFF),
				(real32)((*Dest >> 24) & 0xFF)
			};
			D = SRGB255ToLinear1(D);

			real32 InvRSA = (1.0f-Texel.a);

			v4 Result = (1.0f-Texel.a)*D + Texel;
			
			Result = Linear1ToSRGB255(Result);

			*Dest = 
				(((uint32)(Result.a + 0.5f) << 24)|
				((uint32)(Result.r + 0.5f) << 16) |
				((uint32)(Result.g + 0.5f) << 8) |
				((uint32)(Result.b + 0.5f)) << 0);

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
		SourceRow += Bitmap->Pitch;
	};
}

internal void
RenderGroupToOutput(render_group *RenderGroup, loaded_bitmap *OutputTarget,
	rectangle2i ClipRect, bool Even)
{
	TIMED_FUNCTION();

	real32 NullPixelsToMeters = 1.0f;

	for (uint32 BaseAddress = 0;
		BaseAddress < RenderGroup->PushBufferSize;
		)
	{
		render_group_entry_header *Header = (render_group_entry_header *)
            (RenderGroup->PushBufferBase + BaseAddress);
		BaseAddress += sizeof(*Header);
		
		void *Data = (uint8 *)Header + sizeof(*Header);
        switch (Header->Type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear *Entry = (render_entry_clear *)Data;

				DrawRectangle(OutputTarget, V2(0.0f, 0.0f),
					V2((real32)OutputTarget->Width, (real32)OutputTarget->Height),
					Entry->Color, ClipRect, Even);
                
				BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
				Assert(Entry->Bitmap);
#if 0
				DrawRectangleSlowly(OutputTarget, Entry->P,
					V2(Entry->Size.x, 0),
					V2(0, Entry->Size.y),
					Entry->Color, Entry->Bitmap,
					0, 0, 0, 0, NullPixelsToMeters)
#else
				DrawRectangleQuickly(OutputTarget, Entry->P,
					V2(Entry->Size.x, 0),
					V2(0, Entry->Size.y),
					Entry->Color, Entry->Bitmap,
					NullPixelsToMeters, ClipRect, Even);
#endif
	            BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
            	DrawRectangle(OutputTarget, Entry->P, Entry->P + Entry->Dim, Entry->Color, ClipRect, Even);

                BaseAddress += sizeof(*Entry);
            } break;
			
            case RenderGroupEntryType_render_entry_cordinate_system:
            {
                render_entry_cordinate_system *Entry = (render_entry_cordinate_system *)Data;
#if 0
				DrawRectangleSlowly(
					OutputTarget,
					Entry->Origin, Entry->XAxis, Entry->YAxis, Entry->Color,
					Entry->Texture, Entry->NormalMap,
					Entry->Top, Entry->Middle, Entry->Bottom,
					PixelsToMeters);
				
				v2 Dim = {2, 2};
				v4 Color = {1, 1, 0, 1};
				v2 P = Entry->Origin;
				DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

				P = Entry->Origin + Entry->XAxis;
				DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

				P = Entry->Origin + Entry->YAxis;
				DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

				//DrawRectangle(OutputTarget, vMax - Dim, vMax + Dim, Color.r, Color.g, Color.b);
#endif
                BaseAddress += sizeof(*Entry);
            } break;
            InvalidDefaultCase;
        }
	}
}

struct tile_render_work
{
	render_group *RenderGroup;
	loaded_bitmap *OutputTarget;
	rectangle2i ClipRect;
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(DoTileRenderWork)
{
	TIMED_FUNCTION();
	
	tile_render_work *Work = (tile_render_work *)Data;

	RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, false);
	RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, true);
}

internal void
RenderGroupToOutput(render_group *RenderGroup, loaded_bitmap *OutputTarget)
{
	TIMED_FUNCTION();

	Assert(RenderGroup->InsideRender);

	Assert(((uintptr)OutputTarget->Memory & 15) == 0);

	rectangle2i ClipRect;
	ClipRect.MinX = 0;
	ClipRect.MaxX = OutputTarget->Width;
	ClipRect.MinY = 0;
	ClipRect.MaxY = OutputTarget->Height;

	tile_render_work Work;
	Work.RenderGroup = RenderGroup;
	Work.OutputTarget = OutputTarget;
	Work.ClipRect = ClipRect;

	DoTileRenderWork(0, &Work);
}

internal void
TileRenderGroupToOutput(platform_work_queue *RenderQueue,
	render_group *RenderGroup, loaded_bitmap *OutputTarget)
{
	Assert(RenderGroup->InsideRender);

	int const TileCountX = 4;
	int const TileCountY = 4;
	tile_render_work WorkArray[TileCountX*TileCountY];

	Assert(((uintptr)OutputTarget->Memory & 15) == 0);
	int TileWidth = OutputTarget->Width / TileCountX;
	int TileHeight = OutputTarget->Height / TileCountY;

	TileWidth = ((TileWidth + 3) / 4 ) * 4;

	int WorkCount = 0;
	for (int TileY = 0;
		TileY < TileCountY;
		++TileY)
	{
		for (int TileX = 0;
			TileX < TileCountX;
			++TileX)
		{
			tile_render_work *Work = WorkArray + WorkCount++;

			// TODO: Buffers with overflow!!
			rectangle2i ClipRect;
			ClipRect.MinX = TileX*TileWidth;
			ClipRect.MaxX = ClipRect.MinX + TileWidth;
			ClipRect.MinY = TileY*TileHeight;
			ClipRect.MaxY = ClipRect.MinY + TileHeight;

			if (TileX == (TileCountX - 1))
			{
				ClipRect.MaxX = OutputTarget->Width;
			}

			if (TileY == (TileCountY - 1))
			{
				ClipRect.MaxY = OutputTarget->Height;
			}

			Work->RenderGroup = RenderGroup;
			Work->OutputTarget = OutputTarget;
			Work->ClipRect = ClipRect;

#if 1
			Platform.AddEntry(RenderQueue, DoTileRenderWork, Work);
#else
			DoTileRenderWork(RenderQueue, Work);
#endif
		}
	}

	Platform.CompleteAllWork(RenderQueue);
}

internal render_group *
AllocateRenderGroup(game_assets *Assets, memory_arena *Arena, uint32 MaxPushBufferSize,
	b32 RendersInBackground)
{
    render_group *Result = PushStruct(Arena, render_group);
	
	if (MaxPushBufferSize == 0)
	{
		MaxPushBufferSize = (uint32)GetArenaSizeRemaining(Arena);
	}
	
	Result->Assets = Assets;
	Result->PushBufferBase = (uint8 *)PushSize(Arena, MaxPushBufferSize);

    Result->MaxPushBufferSize = MaxPushBufferSize;
    Result->PushBufferSize = 0;

	Result->GlobalAlpha = 1.0f;

    // NOTE: Default transform
	Result->Transform.OffsetP = V3(0.0f, 0.0f, 0.0f);
	Result->Transform.Scale = 1.0f;

	Result->MissingResourceCount = 0;
	Result->RendersInBackground = RendersInBackground;

	Result->InsideRender = false;

	return(Result);
}

internal void
BeginRender(render_group *Group)
{
	if (Group)
	{
		Assert(!Group->InsideRender);
		Group->InsideRender = true;
		Group->GenerationID = BeginGeneration(Group->Assets);
	}
}

internal void
EndRenderGroup(render_group *Group)
{
	if (Group)
	{
		Assert(Group->InsideRender);
		Group->InsideRender = false;
		
		EndGeneration(Group->Assets, Group->GenerationID);
		Group->GenerationID = 0;
		Group->PushBufferSize = 0;
	}
}

inline void
Perspective(render_group *RenderGroup, int32 PixelWidth, int32 PixelHeight, real32 MetersToPixels,
	real32 FocalLength, real32 DistanceAboveTarget)
{
	// TODO: Need to adjust this based on buffer size
	real32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);
	RenderGroup->MonitroHalfDimInMeters = 
		{0.5f*(real32)PixelWidth*PixelsToMeters, 0.5f*(real32)PixelHeight*PixelsToMeters};

	RenderGroup->Transform.MetersToPixels = MetersToPixels;
	RenderGroup->Transform.FocalLength = FocalLength; // NOTE: Meters the person is sitting from the monitor
	RenderGroup->Transform.DistanceAboveTarget = DistanceAboveTarget;
	RenderGroup->Transform.ScreenCenter = {0.5f*(real32)PixelWidth, 0.5f*(real32)PixelHeight};

	RenderGroup->Transform.Orthographic = false;
}

inline void
Orthographic(render_group *RenderGroup, int32 PixelWidth, int32 PixelHeight, real32 MetersToPixels)
{
	// TODO: Need to adjust this based on buffer size
	real32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);
	RenderGroup->MonitroHalfDimInMeters = 
		{0.5f*(real32)PixelWidth*PixelsToMeters, 0.5f*(real32)PixelHeight*PixelsToMeters};

	RenderGroup->Transform.MetersToPixels = MetersToPixels;
	RenderGroup->Transform.FocalLength = 1.0f; // NOTE: Meters the person is sitting from the monitor
	RenderGroup->Transform.DistanceAboveTarget = 1.0f;
	RenderGroup->Transform.ScreenCenter = {0.5f*(real32)PixelWidth, 0.5f*(real32)PixelHeight};

	RenderGroup->Transform.Orthographic = true;
	RenderGroup->Transform.OffsetP = V3(0, 0, 0);
	RenderGroup->Transform.Scale = 1.0f;
}

inline entity_basis_p_result
GetRenderEntityBasisP(render_transform *Transform, v3 OriginalP)
{
	entity_basis_p_result Result = {};

	v3 P = V3(OriginalP.xy, 0.0f) + Transform->OffsetP;

	if (Transform->Orthographic)
	{
		Result.P = Transform->ScreenCenter + Transform->MetersToPixels*P.xy;
		Result.Scale = Transform->MetersToPixels;
		Result.Valid = true;
	}
	else
	{
		real32 OffsetZ = 0.0f;

		real32 DistanceAboveTarget = Transform->DistanceAboveTarget;
#if DEBUGUI_UsedDebugCamera
		// TODO: How do we want to control the debug camera?
		if (1)
		{
			DistanceAboveTarget += DEBUGUI_DebugCameraDistance;
		}
#endif
		real32 DistanceToPZ = (DistanceAboveTarget - P.z);
		real32 NearClipPlane = 0.2f;

		v3 RawXY = V3(P.xy, 1.0f);
		
		if (DistanceToPZ > NearClipPlane)
		{
			v3 ProjectedXY = (1.0f / DistanceToPZ) * Transform->FocalLength*RawXY;
			Result.P = Transform->ScreenCenter + Transform->MetersToPixels*ProjectedXY.xy + V2(0.0f, Result.Scale*OffsetZ);
			Result.Scale = Transform->MetersToPixels*ProjectedXY.z;
			Result.Valid = true;
		}
	}

    return(Result);
}

#define PushRenderElement(Group, type) (type *)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type)
inline void *
PushRenderElement_(render_group *Group, uint32 Size, render_group_entry_type Type)
{
	Assert(Group->InsideRender);

    void *Result = 0;

	Size += sizeof(render_group_entry_header);

    if ((Group->PushBufferSize + Size) < Group->MaxPushBufferSize)
    {
        render_group_entry_header *Header = (render_group_entry_header *)(Group->PushBufferBase + Group->PushBufferSize);
        Header->Type = Type;
		Result = (Header + 1);
		Group->PushBufferSize += Size;
    }
    else
    {
        InvalidCodePath;
    }

    return(Result);
}

inline used_bitmap_dim
GetBitmapDim(render_group *Group, loaded_bitmap *Bitmap, real32 Height, v3 Offset, r32 CAlign)
{
	used_bitmap_dim Dim;

	Dim.Size = V2(Height*Bitmap->WidthOverHeight, Height);
	Dim.Align = CAlign*Hadamard(Bitmap->AlignPercentage, Dim.Size);
	Dim.P = Offset - V3(Dim.Align, 0);
	Dim.Basis = GetRenderEntityBasisP(&Group->Transform, Dim.P);

	return(Dim);
}

internal inline void
PushBitmap(render_group *Group, loaded_bitmap *Bitmap, real32 Height, v3 Offset, v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f)
{
	used_bitmap_dim Dim = GetBitmapDim(Group, Bitmap, Height, Offset, CAlign);

	if (Dim.Basis.Valid)
	{
		render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_bitmap);
		if (Entry)
		{
			Entry->Bitmap = Bitmap;
			Entry->P = Dim.Basis.P;
			Entry->Color = Group->GlobalAlpha*Color;
			Entry->Size = Dim.Basis.Scale*Dim.Size;
		}
	}
}

internal inline void
PushBitmap(render_group *Group, bitmap_id ID, real32 Height, v3 Offset, v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f)
{
	loaded_bitmap *Bitmap = GetBitmap(Group->Assets, ID, Group->GenerationID);

	if (Group->RendersInBackground && !Bitmap)
	{
		LoadBitmap(Group->Assets, ID, true);
		Bitmap = GetBitmap(Group->Assets, ID, Group->GenerationID);
	}
	
	if (Bitmap && Bitmap->Memory)
	{
		PushBitmap(Group, Bitmap, Height, Offset, Color, CAlign);
	}
	else
	{
		Assert(!Group->RendersInBackground);
		LoadBitmap(Group->Assets, ID, false);
		++Group->MissingResourceCount;
	}
}

inline loaded_font *
PushFont(render_group *Group, font_id ID)
{
	loaded_font *Font = GetFont(Group->Assets, ID, Group->GenerationID);
	if (Font)
	{
		// NOTE: Nothing to do
	}
	else
	{
		Assert(!Group->RendersInBackground);
		LoadFont(Group->Assets, ID, false);
		++Group->MissingResourceCount;
	}

	return(Font);
}

inline void
PushRect(render_group *Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
	v3 P = Offset - V3(0.5f*Dim, 0);
	entity_basis_p_result Basis = GetRenderEntityBasisP(&Group->Transform, P);
	if (Basis.Valid)
	{
		render_entry_rectangle *Rect = PushRenderElement(Group, render_entry_rectangle);
		if (Rect)
		{
			Rect->P = Basis.P;
			Rect->Color = Color;
			Rect->Dim = Basis.Scale*Dim;
		}
	}
}

inline void
PushRect(render_group *Group, rectangle2 Rectangle, r32 Z, v4 Color = V4(1, 1, 1, 1))
{
	PushRect(Group, V3(GetCenter(Rectangle), Z), GetDim(Rectangle), Color);
}

internal inline void
PushRectOutline(render_group *Group, v3 Offset,	v2 Dim, v4 Color = V4(1, 1, 1, 1), r32 Thickness = 0.1f)
{
	// NOTE: Top and bottom
	PushRect(Group, Offset - V3(0, 0.5f*Dim.y, 0), V2(Dim.x, Thickness), Color);
	PushRect(Group, Offset + V3(0, 0.5f*Dim.y, 0), V2(Dim.x, Thickness), Color);
	
    // NOTE: Left and right
	PushRect(Group, Offset - V3(0.5f*Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
	PushRect(Group, Offset + V3(0.5f*Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
}

inline void
Clear(render_group *Group, v4 Color)
{
	render_entry_clear *Entry = PushRenderElement(Group, render_entry_clear);
	if (Entry)
	{
		Entry->Color = Color;
	}
}

inline void
CoordinateSystem(render_group *Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
	loaded_bitmap *Texture, loaded_bitmap *NormalMap,
	environment_map *Top, environment_map *Middle, environment_map *Bottom)
{
#if 0
	entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenDim);
	if (Basis.Valid)
	{
		render_entry_cordinate_system *Entry = PushRenderElement(Group, render_entry_cordinate_system);
		if (Entry)
		{
			Entry->Origin = Origin;
			Entry->XAxis = XAxis;
			Entry->YAxis = YAxis;
			Entry->Color = Color;
			Entry->Texture = Texture;
			Entry->NormalMap = NormalMap;
			Entry->Top = Top;
			Entry->Middle = Middle;
			Entry->Bottom = Bottom;
		}
	}
#endif 
}

inline v3
Unproject(render_group *Group, v2 PixelsXY)
{
	render_transform *Transform = &Group->Transform;

	v2 UnprojectedXY;
	if (Transform->Orthographic)
	{
		UnprojectedXY = (1.0f / Transform->MetersToPixels)*(PixelsXY - Transform->ScreenCenter);
		//NewInput->MouseX = (-0.5f*GlobalBackBaffer.Width + 0.5f) + (r32)MouseP.x;
		//NewInput->MouseY = (0.5f*GlobalBackBaffer.Height - 0.5f) - (r32)MouseP.y;
	}
	else
	{
		v2 A = (PixelsXY - Transform->ScreenCenter) * (1.0f / Transform->MetersToPixels);
		UnprojectedXY = ((Transform->DistanceAboveTarget - Transform->OffsetP.z) / Transform->FocalLength) * A;
	}

	v3 Result = V3(UnprojectedXY, Transform->OffsetP.z);
	Result -= Transform->OffsetP;

	return(Result);
}

inline v2
UnprojectOld(render_group *Group, v2 ProjectedXY, real32 AtDistanceFromCamera)
{
	v2 WorldXY = (AtDistanceFromCamera / Group->Transform.FocalLength)*ProjectedXY;
	return(WorldXY);
}

inline rectangle2
GetCameraRectangleAtDistance(render_group *Group, real32 DistanceFromCamera)
{
	v2 RawXY = UnprojectOld(Group, Group->MonitroHalfDimInMeters, DistanceFromCamera);

	rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);

	return(Result);
}

inline rectangle2
GetCameraRectangleAtTarget(render_group *Group)
{
	rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->Transform.DistanceAboveTarget);
	return(Result);
}

inline bool32
AllResourcesPresent(render_group *Group)
{
	bool32 Result = (Group->MissingResourceCount == 0);
	return(Result);
}