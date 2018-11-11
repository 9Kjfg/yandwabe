#include "handmade.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
	int16 ToneValue = 1000;
	int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0;
		SampleIndex < SoundBuffer->SampleCount;
		++SampleIndex)
	{
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneValue);
#else
		int16 SampleValue = 0;
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
#if 0
		GameState->tSine += 2.0f*Pi32*1.0f / (real32)WavePeriod;
		if (GameState->tSine > 2.0f*Pi32)
		{
			GameState->tSine -= 2.0f*Pi32;
		}
#endif
	}
}

internal void
DrawRectangle(
	game_offscreen_buffer *Buffer, v2 vMin,v2 vMax,
	real32 R, real32 G, real32 B)
{
	int MinX = RoundReal32ToInt32(vMin.X);
	int MinY = RoundReal32ToInt32(vMin.Y);
	int MaxX = RoundReal32ToInt32(vMax.X);
	int MaxY = RoundReal32ToInt32(vMax.Y);

	if (MinX < 0)
	{
		MinX = 0;
	}

	if (MinY < 0)
	{
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

	uint32 Color = 
		(RoundReal32ToUInt32(R * 255.0f) << 16) |
		(RoundReal32ToUInt32(G * 255.0f) << 8) |
		(RoundReal32ToUInt32(B * 255.0f));
	
	uint8 *Row = 
			((uint8 *)Buffer->Memory + 
			MinX*Buffer->BytesPerPixel +
			MinY*Buffer->Pitch);
	
	for (int Y = MinY;
		Y < MaxY;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = MinX;
			X < MaxX;
			++X)
		{
			*Pixel++ = Color;
		}

		Row += Buffer->Pitch;
	}
}

internal void
DrawBitmap(
	game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap,
	real32 RealX, real32 RealY,
	int32 AlignX = 0, int32 AlignY = 0)
{
	RealX -= (real32)AlignX;
	RealY -= (real32)AlignY;

	int MinX = RoundReal32ToInt32(RealX);
	int MinY = RoundReal32ToInt32(RealY);
	int MaxX = RoundReal32ToInt32(RealX + (real32)Bitmap->Width);
	int MaxY = RoundReal32ToInt32(RealY + (real32)Bitmap->Height);

	int32 SourceOffsetX = 0;
	if (MinX < 0)
	{
		SourceOffsetX= -MinX;
		MinX = 0;
	}

	int32 SourceOffsetY = 0;
	if (MinY < 0)
	{
		SourceOffsetY= -MinY;
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

	// TODO: SourceRow needs to be changed based on clipping
	uint32 *SourceRow = Bitmap->Pixel + Bitmap->Width*(Bitmap->Height - 1);
	SourceRow += -SourceOffsetY*Bitmap->Width + SourceOffsetX;
	uint8 *DestRow = 
			((uint8 *)Buffer->Memory + 
			MinX*Buffer->BytesPerPixel +
			MinY*Buffer->Pitch);

	for (int32 Y = MinY;
		Y < MaxY;
		++Y)
	{
		uint32 *Dest = (uint32 *)DestRow;
		uint32 *Source = SourceRow;
		for (int32 X = MinX;
			X < MaxX;
			++X)
		{
			real32 A = (real32)((*Source >> 24) & 0xFF) / 255.0f;
			real32 SR = (real32)((*Source >> 16) & 0xFF);
			real32 SG = (real32)((*Source >> 8) & 0xFF);
			real32 SB = (real32)((*Source >> 0) & 0xFF);

			real32 DR = (real32)((*Dest >> 16) & 0xFF);
			real32 DG = (real32)((*Dest >> 8) & 0xFF);
			real32 DB = (real32)((*Dest >> 0) & 0xFF);

			// TODO: Someday, we need to talk about premultiplied alpha
			// This is not premultiplied alpha
			real32 R = (1.0f-A)*DR + A*SR;
			real32 G = (1.0f-A)*DG + A*SG;
			real32 B = (1.0f-A)*DB + A*SB;

			*Dest = 
				(((uint32)(R + 0.5f) << 16) |
				((uint32)(G + 0.5f) << 8) |
				((uint32)(B + 0.5f)) << 0);

			++Dest;
			++Source;
		}
		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;
	};
}

#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;        /* File type, always 4D42h ("BM") */
    uint32 FileSize;        /* Size of the file in bytes */
    uint16 Reserved1;       /* Always 0 */
    uint16 Reserved2;       /* Always 0 */
    uint32 BitmapOffset;    /* Starting position of image data in bytes */
    uint32 Size;            /* Size of this header in bytes */
    int32 Width;            /* Image width in pixels */
    int32 Height;           /* Image height in pixels */
    uint16 Planes;          /* Number of color planes */
    uint16 BitsPerPixel;    /* Number of bits per pixel */
	int32 Compression;
    int32 SizeOfBitmap;		/* Size of bitmap in bytes */
    int32 HorzResolution;	/* Horizontal resolution in pixels per meter */
    int32 VertResolution;	/* Vertical resolution in pixels per meter */
    int32 ColorsUsed;       /* Number of colors in the image */
    int32 ColorsImportant;  /* Minimum number of important colors */

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(
	thread_context *Thread,
	debug_platform_read_entire_file *ReadEntireFile,
	char *FileName)
{
	loaded_bitmap Result = {};

	debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);
	if (ReadResult.ContentsSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixel = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Pixel = Pixel;
		Result.Width = Header->Width;
		Result.Height = Header->Height;

		Assert(Header->Compression == 3);

		// NOTE; Byte order in memory is determined by the Header itself,
		// os we have to read out the masks and convert hte pixels ourselves
		uint32 RedMask = Header->RedMask;
    	uint32 GreenMask = Header->GreenMask;
    	uint32 BlueMask = Header->BlueMask;
		uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		int32 RedShift = 16 - (int32)RedScan.Index;
		int32 GreenShift = 8 - (int32)GreenScan.Index;
		int32 BlueShift = 0 - (int32)BlueScan.Index;
		int32 AlphaShift = 24 - (int32)AlphaScan.Index;

		uint32 *SourceDest = Pixel;
		for (int32 Y = 0;
			Y < Header->Width;
			++Y)
		{
			for (int32 X = 0;
				X < Header->Height;
				++X)
			{
				uint32 C = *SourceDest;

				*SourceDest++ = 
					(RotateLeft(C & RedMask, RedShift) | 
					RotateLeft(C & GreenMask, GreenShift) |
					RotateLeft(C & BlueMask, BlueShift) |
					RotateLeft(C & AlphaMask, AlphaShift));
			}
		}
	}

	return(Result);
}

inline entity
GetEntity(game_state *GameState, entity_residence Residence, uint32 Index)
{
	entity Entity = {};
	
	if ((Index > 0) && (Index < GameState->EntityCount))
	{
		Entity.Residence = Residence;
		Entity.Dormant = &GameState->DormantEntities[Index];
		Entity.High = &GameState->HighEntities[Index];
		Entity.Low = &GameState->LowEntities[Index];
	}

	return(Entity);	
}

internal void
ChangeEntityResidence(game_state *GameState, entity *Entity, entity_residence Residence)
{
	// TODO: Implement this
}

internal void
InitializePlayer(game_state *GameState, uint32 EntityIndex)
{
	entity Entity = GetEntity(GameState, EntityResidence_Dormant, EntityIndex);
	
	Entity.Dormant->P.AbsTileX = 1;
	Entity.Dormant->P.AbsTileY = 3;
	Entity.Dormant->P.Offset_.X = 0.0f;
	Entity.Dormant->P.Offset_.Y = 0.0f;
	Entity.Dormant->Height = 0.5f;//1.4f;
	Entity.Dormant->Width = 1.0f;

	ChangeEntityResidence(GameState, &Entity, EntityResidence_High);

	if (!GetEntity(GameState, EntityResidence_Dormant, GameState->CameraFollowingEntityIndex).Residence == 
		EntityResidence_Nonexistent)
	{
		GameState->CameraFollowingEntityIndex = EntityIndex;
	}
}

internal uint32
AddEntity(game_state *GameState)
{
	uint32 EntityIndex = GameState->EntityCount++;
	
	Assert(GameState->EntityCount < ArrayCount(GameState->EntityResidence));
	Assert(GameState->EntityCount < ArrayCount(GameState->DormantEntities));
	Assert(GameState->EntityCount < ArrayCount(GameState->HighEntities));
	Assert(GameState->EntityCount < ArrayCount(GameState->LowEntities));
	
	GameState->EntityResidence[EntityIndex] = EntityResidence_Dormant;
	GameState->DormantEntities[EntityIndex] = {};
	GameState->HighEntities[EntityIndex] = {};
	GameState->LowEntities[EntityIndex] = {};

	return(EntityIndex);
}


internal bool32
TestWall(
	real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY,
	real32 *tMin, real32 MinY, real32 MaxY)
{
	bool32 Hit = false;

	real32 tEpsilon = 0.0001f;
	if (PlayerDeltaX != 0.0f)
	{
		real32 tResult = (WallX - RelX) / PlayerDeltaX;
		real32 Y = RelY + tResult*PlayerDeltaY;
		if ((tResult > 0.0f) && (*tMin > tResult))
		{
			if ((Y >= MinY) && (Y <= MaxY))
			{
				*tMin = Maximum(0.0f, tResult- tEpsilon);
				Hit = true;
			}
		}
	}

	return(Hit);
}

internal void
MovePlayer(game_state *GameState, entity Entity, real32 dt, v2 ddP)
{
	tile_map *TileMap = GameState->World->TileMap;

	real32 ddPLength = LengthSq(ddP);
	if (ddPLength > 1.0f)
	{
		ddP *= (1.0f / SquareRoot(ddPLength));
	}

	real32 PlayerSpeed = 50.0f; // m/s ^ 2
	ddP *= PlayerSpeed;

	// TODO: ODE here
	ddP += -8.0f * Entity.High->dP;

	v2 OldPlayerP = Entity.High->P;
	v2 PlayerDelta = (0.5f*ddP*Square(dt) + 
		Entity.High->dP*dt);
	Entity.High->dP = ddP*dt + Entity.High->dP;
	v2 NewPlayerP = OldPlayerP + PlayerDelta;
#if 0
	uint32 MinTileX = Minimum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
	uint32 MinTileY = Minimum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);
	uint32 MaxTileX = Maximum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX) + 1;
	uint32 MaxTileY = Maximum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY) + 1;

	uint32 EntityTileWidth = CeilReal32ToInt32(Entity->Width / TileMap->TileSideInMeters);
	uint32 EntityTileHeight = CeilReal32ToInt32(Entity->Height / TileMap->TileSideInMeters);

	MinTileX -= EntityTileWidth;
	MinTileY -= EntityTileHeight;
	MaxTileX += EntityTileWidth;
	MaxTileY += EntityTileHeight;

	uint32 AbsTileZ = Entity->P.AbsTileZ;
	
	real32 tRemaining = 1.0f;
	for (uint32 Iteration = 0;
		(Iteration < 4) && (tRemaining > 0.0f);
		++Iteration)
	{
		real32 tMin = 1.0f;
		v2 WallNormal = {};

		Assert((MaxTileX - MinTileX) < 32);
		Assert((MaxTileY - MinTileY) < 32);

		for (uint32 AbsTileY = MinTileY;
			AbsTileY <= MaxTileY;
			++AbsTileY)
		{
			for (uint32 AbsTileX = MinTileX;
				AbsTileX <= MaxTileX;
				++AbsTileX)
			{
				tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
				uint32 TileValue = GetTileValue(TileMap, TestTileP);
				if (!IsTileMapEmpty(TileValue))
				{
					real32 DiameterW = TileMap->TileSideInMeters + Entity->Width;
					real32 DiameterH = TileMap->TileSideInMeters + Entity->Height;
					v2 MinCorner = -0.5f*v2{DiameterW, DiameterH};
					v2 MaxCorner = 0.5f*v2{DiameterW, DiameterH};

					tile_map_difference RelOldPlayerP = Subtract(TileMap, &Entity.High->P, &TestTileP);
					v2 Rel = RelOldPlayerP.dXY;

					if(TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y,
						&tMin, MinCorner.Y, MaxCorner.Y))
					{
						WallNormal = v2{-1.0f, 0.0f};
					}
					if(TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y,
						&tMin, MinCorner.Y, MaxCorner.Y))
					{
						WallNormal = v2{1.0f, 0.0f};
					}
					if(TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X,
						&tMin, MinCorner.X, MaxCorner.X))
					{
						WallNormal = v2{0.0f, -1.0f};
					}
					if(TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X,
						&tMin, MinCorner.X, MaxCorner.X))
					{
						WallNormal = v2{0.0f, 1.0f};
					}
				}
			}
		}

		Entity.High->P = Offset(TileMap, Entity.High->P, tMin*PlayerDelta);
		Entity.High->dP = Entity.High->dP - 1.0f*Inner(Entity.High->dP, WallNormal) * WallNormal;
		PlayerDelta = PlayerDelta - 1.0f*Inner(PlayerDelta, WallNormal) * WallNormal;
		tRemaining -= tMin*tRemaining;
	}
	//
	// NOTE: Update camera/player Z based on last movement
	//
	if (!AreOnSameTile(&OldPlayerP, &Entity->P))
	{
		uint32 NewTileValue = GetTileValue(TileMap, Entity->P);

		if (NewTileValue == 3)
		{
			++Entity->P.AbsTileZ;
		}
		else if (NewTileValue == 4)
		{
			--Entity->P.AbsTileZ;
		}
	}

	if ((Entity->dP.X == 0.0f) && (Entity->dP.Y == 0.0f))
	{
		// NOTE: Leave FacingDirection whatever it was
	}
	else if (AbsoluteValue(Entity->dP.X) > AbsoluteValue(Entity->dP.Y))
	{
		if (Entity->dP.X > 0)
		{
			Entity->FacingDirection = 0;
		}
		else
		{
			Entity->FacingDirection = 2;
		}
	}
	else if (AbsoluteValue(Entity->dP.X) < AbsoluteValue(Entity->dP.Y))
	{
		if (Entity->dP.Y > 0)
		{
			Entity->FacingDirection = 1;
		}
		else
		{
			Entity->FacingDirection = 3;
		}
	}
#endif
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
		ArrayCount(Input->Controllers[0].Buttons));
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		// NOTE: Reserve entity slot 0 for the null entity
		AddEntity(GameState);
		GameState->Backdrop = 
			DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
		
		hero_bitmaps *Bitmap;

		Bitmap = GameState->HeroBitmaps;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
		Bitmap->Torso =	DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		++Bitmap;

		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		++Bitmap;

		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;
		++Bitmap;

		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
		Bitmap->AlignX = 72;
		Bitmap->AlignY = 182;

		GameState->CameraP.AbsTileX = 17/2;
		GameState->CameraP.AbsTileY = 9/2;

		InitializeArena(
			&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
			(uint8 *)Memory->PermanentStorage + sizeof(game_state));

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world *World = GameState->World;
		World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

		tile_map *TileMap = World->TileMap;
		// NOTE: This is set to using 256x256 tile chunks
		TileMap->ChunkShift = 4;
		TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
		TileMap->ChunkDim = (1 << TileMap->ChunkShift);

		TileMap->TileChunkCountX = 128;
		TileMap->TileChunkCountY = 128;
		TileMap->TileChunkCountZ = 2;
		TileMap->TileChunks = PushArray(
			&GameState->WorldArena,
			TileMap->TileChunkCountX*
			TileMap->TileChunkCountY*
			TileMap->TileChunkCountY,
			tile_chunk);
		
		TileMap->TileSideInMeters = 1.4f;

		uint32 RandomNumberIndex = 0;
		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;
#if 0
		uint32 ScreenX = INT32_MAX / 2;
		uint32 ScreenY = INT32_MAX / 2;
#else
		uint32 ScreenX = 0;
		uint32 ScreenY = 0;
#endif
		uint32 AbsTileZ = 0;

		// TODO: Replace with real world generation
		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;
		for (uint32 ScreenIndex = 0;
			ScreenIndex < 100;
			++ScreenIndex)
		{
			Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));

			uint32 RandomChoice;
			if (DoorUp || DoorDown)
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
			}
			else
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
			}

			bool32 CreatedZDoor = false;
			if (RandomChoice == 2)
			{
				CreatedZDoor = true;
				if (AbsTileZ == 0)
				{
					DoorUp = true;
				}
				else
				{
					DoorDown = true;
				}
			}
			else if (RandomChoice == 1)
			{
				DoorRight = true;
			}
			else
			{
				DoorTop = true;
			}

			for (uint32 TileY = 0;
				TileY < TilesPerHeight;
				++TileY)
			{
				for (uint32 TileX = 0;
					TileX < TilesPerWidth;
					++TileX)
				{
					uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

					uint32 TileValue = 1;
					if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
					{
						TileValue = 2;
					}

					if ((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2))))
					{
						TileValue = 2;
					}

					if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth/2))))
					{
						TileValue = 2;
					}

					if ((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth/2))))
					{
						TileValue = 2;
					}

					if ((TileX == 10) && (TileY == 6))
					{
						if (DoorUp)
						{
							TileValue = 3;
						}

						if (DoorDown)
						{
							TileValue = 4;
						}
					}

					SetTileValue(
						&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ,
						TileValue);
				}
			}

			DoorLeft = DoorRight;
			DoorBottom = DoorTop;

			if (CreatedZDoor)
			{
				DoorDown = !DoorDown;
				DoorUp = !DoorUp;
			}
			else
			{
				DoorUp = false;
				DoorDown = false;
			}

			DoorRight = false;
			DoorTop = false;

			if (RandomChoice == 2)
			{
				if (AbsTileZ == 0)
				{
					AbsTileZ = 1;
				}
				else
				{
					AbsTileZ = 0;
				}
			}
			else if (RandomChoice == 1)
			{
				ScreenX += 1; 
			}
			else
			{
				ScreenY += 1;
			}
		}

		Memory->IsInitialized = true;
	}

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	int32 TileSideInPixels = 60;
	real32 MetersToPixels = (real32)TileSideInPixels / TileMap->TileSideInMeters;

	real32 LowerLeftX = -(real32)TileSideInPixels/2;
	real32 LowerLeftY = (real32)Buffer->Height;

	for (int ControllerIndex = 0;
		ControllerIndex < ArrayCount(Input->Controllers);
		++ControllerIndex)
	{
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		entity ControllingEntity = GetEntity(
			GameState, EntityResidence_High, GameState->PlayerIndexForController[ControllerIndex]);
		if (ControllingEntity.Residence != EntityResidence_Nonexistent)
		{
			v2 ddPlayer = {};

			if (Controller->IsAnalog)
			{
				ddPlayer = v2{Controller->StickAverageX, Controller->StickAverageY};
			}
			else
			{
				//NOTE: Use digital movement tuning

				if (Controller->MoveUp.EndedDown)
				{
					ddPlayer.Y = 1.0f;
				}
				if (Controller->MoveDown.EndedDown)
				{
					ddPlayer.Y = -1.0f;
				}
				if (Controller->MoveLeft.EndedDown)
				{
					ddPlayer.X = -1.0f;
				}
				if (Controller->MoveRight.EndedDown)
				{
					ddPlayer.X = 1.0f;
				}
			}

			MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddPlayer);
		}
		else
		{
			if (Controller->Start.EndedDown)
			{
				uint32 EntityIndex = AddEntity(GameState);
				InitializePlayer(GameState, EntityIndex);
				GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
			}
		}
	}

	entity CameraFollowingEntity = GetEntity(
		GameState, EntityResidence_High, GameState->CameraFollowingEntityIndex);
	if (CameraFollowingEntity.Residence != EntityResidence_Nonexistent)
	{
		// TODO: Fix this to work in camera space
#if 0
		GameState->CameraP.AbsTileZ = CameraFollowingEntity.Dormant->P.AbsTileZ;

		tile_map_difference Diff = Subtract(TileMap, &CameraFollowingEntity->P, &GameState->CameraP);
		if (Diff.dXY.X > (9.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileX += 17;
		}
		if (Diff.dXY.X < -(9.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileX -= 17;
		}
		if (Diff.dXY.Y > (5.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileY += 9;
		}
		if (Diff.dXY.Y < -(5.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileY -= 9;
		}
#endif
	}
	// NOTE: Render

	DrawBitmap(Buffer, &GameState->Backdrop, 0, 0);

	real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

	for (int32 RelRow = -10;
		RelRow < 10;
		++RelRow)
	{
		for (int32 RelColumn = -20;
			RelColumn < 20;
			++RelColumn)
		{
			uint32 Column = GameState->CameraP.AbsTileX + RelColumn;
			uint32 Row = GameState->CameraP.AbsTileY + RelRow;
			uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);
			if (TileID > 1)
			{
				real32 Gray = 0.5f;
				if (TileID == 2)
				{
					Gray = 1.0f;
				}

				if (TileID > 2)
				{
					Gray = 0.25f;	
				}

				if ((Column == GameState->CameraP.AbsTileX) && 
					(Row == GameState->CameraP.AbsTileY))
				{
					Gray = 0.0f;	
				}

				v2 TileSide = {0.5f*TileSideInPixels, 0.5f*TileSideInPixels};
				v2 Cen = {
					ScreenCenterX - MetersToPixels*GameState->CameraP.Offset_.X  + ((real32)RelColumn)*TileSideInPixels,
					ScreenCenterY + MetersToPixels*GameState->CameraP.Offset_.Y - ((real32)RelRow)*TileSideInPixels};
				v2 Min = Cen - TileSide;
				v2 Max = Cen + TileSide;

				DrawRectangle(Buffer, Min, Max, Gray, Gray, Gray);
			}
		}
	}

	for (uint32 EntityIndex = 0;
		EntityIndex < GameState->EntityCount;
		++EntityIndex)
	{
		// TODO: Culling of entities based on Z / camera view
		if (GameState->EntityResidence[EntityIndex] == EntityResidence_High)
		{
			high_entity *HighEntity = &GameState->HighEntities[EntityIndex];
			low_entity *LowEntity = &GameState->LowEntities[EntityIndex];
			dormant_entity *DormantEntity = &GameState->DormantEntities[EntityIndex];

			real32 PlayerR = 1.0f;
			real32 PlayerG = 1.0f;
			real32 PlayerB = 0;
			real32 PlayerGroundPointX = ScreenCenterX + MetersToPixels*HighEntity->P.X;
			real32 PlayerGroundPointY = ScreenCenterY - MetersToPixels*HighEntity->P.Y;
			v2 PlayerLeftTop = {
				PlayerGroundPointX - 0.5f*MetersToPixels*DormantEntity->Width,
				PlayerGroundPointY - 0.5f*MetersToPixels*DormantEntity->Height};

			v2 EntityWidthHeight = {DormantEntity->Width, DormantEntity->Height};
			DrawRectangle(
				Buffer,
				PlayerLeftTop,
				PlayerLeftTop + MetersToPixels*EntityWidthHeight,
				PlayerR, PlayerG, PlayerB);

			hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[HighEntity->FacingDirection];
			DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
			DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);
			DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundPointX, PlayerGroundPointY, HeroBitmaps->AlignX, HeroBitmaps->AlignY);

		}
	}
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, 400);
}
