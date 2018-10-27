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
	game_offscreen_buffer *Buffer,
	real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
	real32 R, real32 G, real32 B)
{
	int MinX = RoundReal32ToInt32(RealMinX);
	int MinY = RoundReal32ToInt32(RealMinY);
	int MaxX = RoundReal32ToInt32(RealMaxX);
	int MaxY = RoundReal32ToInt32(RealMaxY);

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
DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY)
{
	int MinX = RoundReal32ToInt32(RealX);
	int MinY = RoundReal32ToInt32(RealY);
	int MaxX = RoundReal32ToInt32(RealX + (real32)Bitmap->Width);
	int MaxY = RoundReal32ToInt32(RealY + (real32)Bitmap->Height);

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


	// TODO: SourceRow needs to be changed based on clipping
	uint32 *SourceRow = Bitmap->Pixel + Bitmap->Width*(Bitmap->Height - 1);
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
			*Dest++ = *Source++;
		}
		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;
	}

	// DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height,
	// 	1.0f, 0, 1.0f);
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
DEBUGLoadBMP(thread_context *Thread,
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

		uint32 *SourceDest = Pixel;
		for (int32 Y = 0;
			Y < Header->Width;
			++Y)
		{
			for (int32 X = 0;
				X < Header->Height;
				++X)
			{
				*SourceDest = (*SourceDest >> 8) | (*SourceDest << 24);
				++SourceDest;
			}
		}
	}

	return(Result);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
		ArrayCount(Input->Controllers[0].Buttons));
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	real32 PlayerHeight = 1.4f;
	real32 PlayerWidth = 0.75f*PlayerHeight;

	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		GameState->Backdrop = 
			DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
		
		GameState->HeroHead = 
			DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
		GameState->HeroCape = 
			DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background_front_cape.bmp");
		GameState->HeroTorso = 
			DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background_front_torso.bmp");

		GameState->PlayerP.AbsTileX = 1;
		GameState->PlayerP.AbsTileY = 3;
		GameState->PlayerP.OffsetX = 5.0f;
		GameState->PlayerP.OffsetY = 5.0f;

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
		uint32 ScreenX = 0;
		uint32 ScreenY = 0;
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
		if (Controller->IsAnalog)
		{
		}
		else
		{
			//NOTE: Use digital movement tuning
			real32 dPlayerX = 0.0f;
			real32 dPlayerY = 0.0f;

			if (Controller->MoveUp.EndedDown)
			{
				dPlayerY = 1.0f;
			}
			if (Controller->MoveDown.EndedDown)
			{
				dPlayerY = -1.0f;
			}
			if (Controller->MoveLeft.EndedDown)
			{
				dPlayerX = -1.0f;
			}
			if (Controller->MoveRight.EndedDown)
			{
				dPlayerX = 1.0f;
			}
			real32 PlayerSpeed = 2.0f;

			if (Controller->ActionUp.EndedDown)
			{
				PlayerSpeed = 10.0f;
			}

			dPlayerX *= PlayerSpeed;
			dPlayerY *= PlayerSpeed;

			//TODO: Diagonal will be faster! Fix once we have vectors
			tile_map_position NewPlayerP = GameState->PlayerP; 
			NewPlayerP.OffsetX += Input->dtForFrame*dPlayerX;
			NewPlayerP.OffsetY += Input->dtForFrame*dPlayerY;
			NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
			// TODO: Delta function that auto-recononicasizes

			tile_map_position PlayerLeft = NewPlayerP;
			PlayerLeft.OffsetX -= 0.5f*PlayerWidth;
			PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

			tile_map_position PlayerRight = NewPlayerP;
			PlayerRight.OffsetX += 0.5f*PlayerWidth;
			PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

			if (IsTileMapPointEmpty(TileMap, NewPlayerP) &&
				IsTileMapPointEmpty(TileMap, PlayerLeft) &&
				IsTileMapPointEmpty(TileMap, PlayerRight))
			{
				if (!AreOnSameTile(&GameState->PlayerP, &NewPlayerP))
				{
					uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);
					if (NewTileValue == 3)
					{
						++NewPlayerP.AbsTileZ;
					}
					else if (NewTileValue == 4)
					{
						--NewPlayerP.AbsTileZ;
					}
				}


				GameState->PlayerP = NewPlayerP;
			}
		}
	}

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
			uint32 Column = GameState->PlayerP.AbsTileX + RelColumn;
			uint32 Row = GameState->PlayerP.AbsTileY + RelRow;
			uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->PlayerP.AbsTileZ);
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

				if ((Column == GameState->PlayerP.AbsTileX) && 
					(Row == GameState->PlayerP.AbsTileY))
				{
					Gray = 0.0f;	
				}

				real32 CenX = ScreenCenterX - MetersToPixels*GameState->PlayerP.OffsetX  + ((real32)RelColumn)*TileSideInPixels;
				real32 CenY = ScreenCenterY + MetersToPixels*GameState->PlayerP.OffsetY - ((real32)RelRow)*TileSideInPixels;
				real32 MinX = CenX - 0.5f*TileSideInPixels;
				real32 MinY = CenY - 0.5f*TileSideInPixels;
				real32 MaxX = CenX + 0.5f*TileSideInPixels;
				real32 MaxY = CenY + 0.5f*TileSideInPixels;

				DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
			}
		}
	}

	real32 PlayerR = 1.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 0;
	real32 PlayerLeft =	ScreenCenterX - 0.5*MetersToPixels*PlayerWidth;
	real32 PlayerTop = ScreenCenterY - MetersToPixels*PlayerHeight;
	DrawRectangle(
		Buffer,
		PlayerLeft, PlayerTop,
		PlayerLeft + MetersToPixels*PlayerWidth,
		PlayerTop + MetersToPixels*PlayerHeight,
		PlayerR, PlayerG, PlayerB);

	DrawBitmap(Buffer, &GameState->HeroHead, PlayerLeft, PlayerTop);
	//DrawBitmap(Buffer, &GameState->HeroCape, 0, 0);
	//DrawBitmap(Buffer, &GameState->HeroTorso, 0, 0);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, 400);
}

/*
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for(int Y = 0; 
		Y < Buffer->Height;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0; 
			X < Buffer->Width;
			++X)
		{
			uint8 Bluee = (uint8)(X + XOffset);
			uint8 Green = (uint8)(Y + YOffset);

			*Pixel = ((Green << 8) | Bluee);
			++Pixel;
		}

		Row += Buffer->Pitch;
	}
}
*/