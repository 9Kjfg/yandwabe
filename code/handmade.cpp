#include "handmade.h"
#include "handmade_tile.cpp"

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

inline tile_map_position
RecanonicalizePosition(tile_map *TileMap, tile_map_position Pos)
{
	tile_map_position Result = Pos;

	RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.TileRelX);
	RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.TileRelY);

	return(Result);
}

internal void
InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base)
{
	Arena->Size = Size;
	Arena->Base = Base;
	Arena->Used = 0;
}


#define PushStruct(Arena, type) (type *)PushStruct_(Arena, sizeof(type))
void *
PushStruct_(memory_arena *Arena, memory_index Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return(Result);
}

#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
void *
PushSize_(memory_arena *Arena, memory_index Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

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
		GameState->PlayerP.AbsTileX = 1;
		GameState->PlayerP.AbsTileY = 3;
		GameState->PlayerP.TileRelX = 5.0f;
		GameState->PlayerP.TileRelY = 5.0f;

		InitializeArena(
			&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
			(uint8 *)Memory->PermanentStorage + sizeof(game_state));

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world *World = GameState->World;
		World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

		tile_map *TileMap = World->TileMap;
		// NOTE: This is set to using 256x256 tile chunks
		TileMap->ChunkShift = 8;
		TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
		TileMap->ChunkDim = (1 << TileMap->ChunkShift);

		TileMap->TileChunkCountX = 4;
		TileMap->TileChunkCountY = 4;
		TileMap->TileChunks = PushArray(
			&GameState->WorldArena,
			TileMap->TileChunkCountX*TileMap->TileChunkCountY,
			tile_chunk);

		for (uint32 Y = 0;
			Y < TileMap->TileChunkCountY;
			++Y)
		{
			for (uint32 X = 0;
				X < TileMap->TileChunkCountX;
				++X)
			{
				TileMap->TileChunks[Y*TileMap->TileChunkCountX + X].Tiles = 
					PushArray(&GameState->WorldArena, TileMap->ChunkDim*TileMap->ChunkDim, uint32);
			}
		}

		// tile_chunk TileChunk;
		// TileChunk.Tiles = (uint32 *)TempTile;
		// TileMap->TileChunks = &TileChunk;
		
		TileMap->TileSideInMeters = 1.4f;
		TileMap->TileSideInPixels = 60;
		TileMap->MetersToPixels = (real32)TileMap->TileSideInPixels / TileMap->TileSideInMeters;

		real32 LowerLeftX = -(real32)TileMap->TileSideInPixels/2;
		real32 LowerLeftY = (real32)Buffer->Height;

		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;
		for (uint32 ScreenY = 0;
			ScreenY < 32;
			++ScreenY)
		{
			for (uint32 ScreenX = 0;
			ScreenX < 32;
			++ScreenX)
			{
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

						SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY,
						((TileX == TileY) && (TileY % 2)) ? 1 : 0);
					}
				}
			}
		}

		Memory->IsInitialized = true;
	}
	
	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

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
			NewPlayerP.TileRelX += Input->dtForFrame*dPlayerX;
			NewPlayerP.TileRelY += Input->dtForFrame*dPlayerY;
			NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
			// TODO: Delta function that auto-recononicasizes

			tile_map_position PlayerLeft = NewPlayerP;
			PlayerLeft.TileRelX -= 0.5f*PlayerWidth;
			PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

			tile_map_position PlayerRight = NewPlayerP;
			PlayerRight.TileRelX += 0.5f*PlayerWidth;
			PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

			if (IsTileMapPointEmpty(TileMap, NewPlayerP) &&
				IsTileMapPointEmpty(TileMap, PlayerLeft) &&
				IsTileMapPointEmpty(TileMap, PlayerRight))
			{
				GameState->PlayerP = NewPlayerP;
			}
		}
	}

	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height,
		1.0f, 0, 1.0f);

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
			uint32 TileID = GetTileValue(TileMap, Column, Row);
			real32 Gray = 0.5f;
			if (TileID == 1)
			{
				Gray = 1.0f;
			}
			if ((Column == GameState->PlayerP.AbsTileX) && 
				(Row == GameState->PlayerP.AbsTileY))
			{
				Gray = 0.0f;	
			}
			real32 CenX = ScreenCenterX - TileMap->MetersToPixels*GameState->PlayerP.TileRelX  + ((real32)RelColumn)*TileMap->TileSideInPixels;
			real32 CenY = ScreenCenterY + TileMap->MetersToPixels*GameState->PlayerP.TileRelY - ((real32)RelRow)*TileMap->TileSideInPixels;
			real32 MinX = CenX - 0.5f*TileMap->TileSideInPixels;
			real32 MinY = CenY - 0.5f*TileMap->TileSideInPixels;
			real32 MaxX = CenX + 0.5f*TileMap->TileSideInPixels;
			real32 MaxY = CenY + 0.5f*TileMap->TileSideInPixels;

			DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
		}
	}

	real32 PlayerR = 1.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 0;
	real32 PlayerLeft =	ScreenCenterX - 0.5*TileMap->MetersToPixels*PlayerWidth;
	real32 PlayerTop = ScreenCenterY - TileMap->MetersToPixels*PlayerHeight;
	DrawRectangle(
		Buffer,
		PlayerLeft, PlayerTop,
		PlayerLeft + TileMap->MetersToPixels*PlayerWidth,
		PlayerTop + TileMap->MetersToPixels*PlayerHeight,
		PlayerR, PlayerG, PlayerB);
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