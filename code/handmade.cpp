
#include "handmade.h"
#include "handmade_sort.cpp"
#include "handmade_render_group.cpp"
#include "handmade_asset.cpp"
#include "handmade_audio.cpp"
#include "handmade_world.cpp"
#include "handmade_sim_region.cpp"
#include "handmade_entity.cpp"
#include "handmade_world_mode.cpp"
#include "handmade_cutscene.cpp"

internal task_with_memory *
BeginTaskWidthMemory(transient_state *TranState, b32 DependsOnGameMode)
{
	task_with_memory *FoundTask = 0;

	for (uint32 TaskIndex = 0;
		TaskIndex < ArrayCount(TranState->Tasks);
		++TaskIndex)
	{
		task_with_memory *Task = TranState->Tasks + TaskIndex;
		if (!Task->BeingUsed)
		{
			FoundTask = Task;
			Task->BeingUsed = true;
			Task->DependsOnGameMode = DependsOnGameMode;
			Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
			break;
		}
	}

	return(FoundTask);
}

internal void
EndTaskWidthMemory(task_with_memory *Task)
{
	EndTemporaryMemory(Task->MemoryFlush);

	CompletePreviousWritesBeforeFutureWrites;
	Task->BeingUsed = false;
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *Arena, int32 Width, int32 Height, bool32 ClearToZero = true)
{
	loaded_bitmap Result = {};

	Result.AlignPercentage = V2(0.5f, 0.5f);
	Result.WidthOverHeight = SafeRatio1((r32)Width, (r32)Height);

	Result.Width = SafeTruncateToUInt16(Width);
	Result.Height = SafeTruncateToUInt16(Height);
	Result.Pitch = SafeTruncateToUInt16(Result.Width*BITMAP_BYTES_PER_PIXEL);
	int32 TotalBitmapSize = Width*Height*BITMAP_BYTES_PER_PIXEL;
	Result.Memory = PushSize(Arena, TotalBitmapSize, Align(16, ClearToZero));

	return(Result);
}

internal void
MakeSphereNormalMap(loaded_bitmap *Bitmap, real32 Roughness, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
	real32 InvWidth = 1.0f / (Bitmap->Width - 1);
	real32 InvHeight = 1.0f / (Bitmap->Height - 1);

	uint8 *Row = (uint8 *)Bitmap->Memory;

	for (int32 Y = 0;
		Y < Bitmap->Height;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int32 X = 0;
			X < Bitmap->Width;
			++X)
		{
			v2 BitmapUV = {InvWidth*(real32)X, InvHeight*(real32)Y};

			real32 Nx = Cx*(2.0f*BitmapUV.x - 1.0f);
			real32 Ny = Cy*(2.0f*BitmapUV.y - 1.0f);

			real32 RootTerm = 1.0f - Nx*Nx - Ny*Ny;
			v3 Normal = {0, 0.707106781186, 0.707106781186};
			real32 Nz = 0.0;
			if (RootTerm >= 0.0f)
			{
				Nz = SquareRoot(RootTerm);
				Normal = V3(Nx, Ny, Nz);
			}


			v4 Color =
			{
				255.0f*(0.5f*(Normal.x + 1.0f)),
				255.0f*(0.5f*(Normal.y + 1.0f)),
				255.0f*(0.5f*(Normal.z + 1.0f)),
				255.0f*Roughness
			};
			
			*Pixel++ = 
				(((uint32)(Color.a + 0.5f) << 24)|
				((uint32)(Color.r + 0.5f) << 16) |
				((uint32)(Color.g + 0.5f) << 8) |
				((uint32)(Color.b + 0.5f)) << 0);
		}

		Row += Bitmap->Pitch;
	}
}

internal void
MakeSphereDiffuseMap(loaded_bitmap *Bitmap, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
	real32 InvWidth = 1.0f / (Bitmap->Width - 1);
	real32 InvHeight = 1.0f / (Bitmap->Height - 1);

	uint8 *Row = (uint8 *)Bitmap->Memory;

	for (int32 Y = 0;
		Y < Bitmap->Height;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int32 X = 0;
			X < Bitmap->Width;
			++X)
		{
			v2 BitmapUV = {InvWidth*(real32)X, InvHeight*(real32)Y};

			real32 Nx = Cx*(2.0f*BitmapUV.x - 1.0f);
			real32 Ny = Cy*(2.0f*BitmapUV.y - 1.0f);

			real32 RootTerm = 1.0f - Nx*Nx - Ny*Ny;
			real32 Alpha = 0.0f;
			if (RootTerm >= 0.0f)
			{
				Alpha = 1.0f;
			}

			v3 BaseColor = {0.0f, 0.0f, 0.0f};
			Alpha *= 255.0f;
			v4 Color =
			{
				Alpha*BaseColor.x,
				Alpha*BaseColor.y,
				Alpha*BaseColor.z,
				Alpha
			};
			
			*Pixel++ = 
				(((uint32)(Color.a + 0.5f) << 24)|
				((uint32)(Color.r + 0.5f) << 16) |
				((uint32)(Color.g + 0.5f) << 8) |
				((uint32)(Color.b + 0.5f)) << 0);
		}

		Row += Bitmap->Pitch;
	}
}

// TODO: Really want to get rid of main generation ID
internal u32
DEBUGGetMainGenerationID(game_memory *Memory)
{
	u32 Result = 0;

	transient_state *TranState = (transient_state *)Memory->TransientStorage;
	if (TranState->IsInitialized)
	{
		Result = TranState->MainGenerationID;
	}

	return(Result);
}

internal game_assets *
DEBUGGetGameAssets(game_memory *Memory)
{
	game_assets *Assets = 0;

	transient_state *TranState = (transient_state *)Memory->TransientStorage;
	if (TranState->IsInitialized)
	{
		Assets = TranState->Assets;
	}

	return(Assets);
}

internal void
SetGameMode(game_state *GameState, transient_state *TranState, game_mode GameMode)
{
	b32 NeedToWait = false;
	for (u32 TaskIndex = 0;
		TaskIndex < ArrayCount(TranState->Tasks);
		++TaskIndex)
	{
		NeedToWait = NeedToWait || TranState->Tasks[TaskIndex].DependsOnGameMode;	
	}

	if (NeedToWait)
	{
		Platform.CompleteAllWork(TranState->LowPriorityQueue);
	}

	Clear(&GameState->ModeArena);
	GameState->GameMode = GameMode;
}

#if HANDMADE_INTERNAL
debug_table *GlobalDebugTable;
game_memory *DebugGlobalMemory;
#endif

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Platform = Memory->PlatformAPI;

#if HANDMADE_INTERNAL
	GlobalDebugTable = Memory->DebugTable;
	DebugGlobalMemory = Memory;

	{DEBUG_DATA_BLOCK("Renderer");
		DEBUG_B32(Global_Renderer_TestWierdDrawBufferSize);
		
		{DEBUG_DATA_BLOCK("Camera");
			DEBUG_B32(Global_Renderer_Camera_UseDebug);
			DEBUG_VALUE(Global_Renderer_Camera_DebugDistance);
			DEBUG_B32(Global_Renderer_Camera_RoomBased);
		}
	}

	{DEBUG_DATA_BLOCK("GroundChunks");
		DEBUG_B32(Global_GroundChunks_Chekerboards);
		DEBUG_B32(Global_GroundChunks_RecomputeOnEXEChange);
		DEBUG_B32(Global_GroundChunks_Outline);
		DEBUG_B32(Global_GroundChunksOn);
	}
	{DEBUG_DATA_BLOCK("AI/Familiar");
		DEBUG_B32(Global_AI_Familiar_FollowsHero);
	}
	{DEBUG_DATA_BLOCK("Particles");
		DEBUG_B32(Global_Particles_Test);
		DEBUG_B32(Global_Paritcles_ShowGrid);
	}
	{DEBUG_DATA_BLOCK("Simulation");
		DEBUG_B32(Global_Simulation_UseSpaceOutlines);
	}

	{DEBUG_DATA_BLOCK("Profile");
		DEBUG_UI_ELEMENT(DebugType_FrameSlider, FrameSlider);
		DEBUG_UI_ELEMENT(DebugType_LastFrameInfo, LastFrame);
		DEBUG_UI_ELEMENT(DebugType_DebugMemoryInfo, DebugMemory);
		DEBUG_UI_ELEMENT(DebugType_TopClocksList,GameUpdateAndRender);
	}
#endif

	TIMED_FUNCTION();

	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
		ArrayCount(Input->Controllers[0].Buttons));
		
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!GameState->IsInitialized)
	{
		memory_arena TotalArena;
		InitializeArena(&TotalArena, Memory->PermanentStorageSize - sizeof(game_state),
			(uint8 *)Memory->PermanentStorage + sizeof(game_state));

		SubArena(&GameState->AudioArena, &TotalArena, Megabytes(1));
		SubArena(&GameState->ModeArena, &TotalArena, GetArenaSizeRemaining(&TotalArena));

		InitializeAudioState(&GameState->AudioState, &GameState->AudioArena);

		GameState->IsInitialized = true;
	}

	// NOTE: Transient initialization
	Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
	transient_state *TranState = (transient_state *)Memory->TransientStorage;
	if (!TranState->IsInitialized)
	{
		InitializeArena(&TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
			(uint8 *)Memory->TransientStorage + sizeof(transient_state));

		TranState->HighPriorityQueue = Memory->HighPriorityQueue;
		TranState->LowPriorityQueue = Memory->LowPriorityQueue;
		for (uint32 TaskIndex = 0;
			TaskIndex < ArrayCount(TranState->Tasks);
			++TaskIndex)
		{
			task_with_memory *Task = TranState->Tasks + TaskIndex;

			Task->BeingUsed = false;
			SubArena(&Task->Arena, &TranState->TranArena, Megabytes(2));	
		}
		
		TranState->Assets = AllocateGameAssets(&TranState->TranArena, TranState, Megabytes(128));

		GameState->Music = 0;//PlaySound(&GameState->AudioState, GetFirstSoundFrom(TranState->Assets, Asset_Music));

		GameState->TestDiffuse = MakeEmptyBitmap(&TranState->TranArena, 256, 256, false);
		
		GameState->TestNormal = MakeEmptyBitmap(&TranState->TranArena, GameState->TestDiffuse.Width, GameState->TestDiffuse.Height, false);
		MakeSphereNormalMap(&GameState->TestNormal, 0.0f);
		MakeSphereDiffuseMap(&GameState->TestDiffuse);

		TranState->EnvMapWidth = 512;
		TranState->EnvMapHeight = 256;

		for (uint32 MapIndex = 0;
			MapIndex < ArrayCount(TranState->EnvMaps);
			++MapIndex)
		{
			environment_map *Map = TranState->EnvMaps + MapIndex;
			uint32 Width = TranState->EnvMapWidth;
			uint32 Height = TranState->EnvMapHeight;
			for (uint32 LODIndex = 0;
				LODIndex < ArrayCount(Map->LOD);
				++LODIndex)
			{
				Map->LOD[LODIndex] = MakeEmptyBitmap(&TranState->TranArena, Width, Height, false);
				Width >>= 1;
				Height >>= 1;
			}
		}

		TranState->IsInitialized = true;
	}

	{DEBUG_DATA_BLOCK("Memory");
		memory_arena *ModeArena = &GameState->ModeArena;
		DEBUG_VALUE(ModeArena);

		memory_arena *AudioArena = &GameState->ModeArena;
		DEBUG_VALUE(AudioArena);

		memory_arena *TranArena = &TranState->TranArena;
		DEBUG_VALUE(TranArena);
	}
	
	// TODO: We should probably pull the generation stuff, because
	// if we don't use ground chunks, It's a huge waste of effort!
	if (TranState->MainGenerationID)
	{
		EndGeneration(TranState->Assets, TranState->MainGenerationID);
	}
	TranState->MainGenerationID = BeginGeneration(TranState->Assets);

	if (GameState->GameMode == GameMode_None)
	{
		PlayIntroCutScene(GameState, TranState);
	}

#if 0
	{
		v2 MusicVolume;
		MusicVolume.x = SafeRatio0((r32)Input->MouseX, (r32)Buffer->Width);
		MusicVolume.y = 1.0 - MusicVolume.x;
		ChangeVolume(&GameState->AudioState, GameState->Music, 0.01f, MusicVolume);
	}
#endif

	//
	// NOTE: Render
	//
	temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

	// TODO: Decide what our pushbuffer size is
	render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID, false);
	render_group *RenderGroup = &RenderGroup_;

	loaded_bitmap DrawBuffer = {};
	DrawBuffer.Width = RenderCommands->Width;
	DrawBuffer.Height = RenderCommands->Height;

	b32 Rerun = false;
	do
	{
		switch (GameState->GameMode)
		{
			case GameMode_TitleScreen:
			{
				Rerun = UpdateAndRenderTitleScreen(GameState, TranState, RenderGroup, &DrawBuffer,
					Input, GameState->TitleScreen);
			} break;

			case GameMode_CutScene:
			{
				Rerun = UpdateAndRenderCutScene(GameState, TranState, RenderGroup, &DrawBuffer, 
					Input, GameState->CutScene);
			} break;

			case GameMode_World:
			{
				Rerun = UpdateAndRenderWorld(GameState, GameState->WorldMode, TranState, Input, RenderGroup, &DrawBuffer);	
			} break;

			InvalidDefaultCase;
		}
	} while (Rerun);

	EndRenderGroup(RenderGroup);

	EndTemporaryMemory(RenderMemory);
#if 0
	if (!HeroesExist && QuitRequested)
	{
		Memory->QuitRequested = true;
	}
#endif
	CheckArena(&GameState->ModeArena);
	CheckArena(&TranState->TranArena);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	transient_state *TranState = (transient_state *)Memory->TransientStorage;

	OutputPlayingSounds(&GameState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena);
}

#if HANDMADE_INTERNAL
#include "handmade_debug.cpp"
#else
extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd)
{
	return(0);
}
#endif