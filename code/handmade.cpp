#include "handmade.h"

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

internal int32
RoundReal32ToInt32(real32 Real32)
{
	int Result = (int)(Real32 + 0.5f);
	//TODO: Intrinsic
	return(Result); 
}

internal void
DrawRectangle(
	game_offscreen_buffer *Buffer,
	real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY, uint32 Color)
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
		MaxX = 0;
	}

	if (MaxY > Buffer->Height)
	{
		MaxY = 0;
	}

	uint8 *EndOfBuffer = (uint8 *)Buffer->Memory + Buffer->Pitch*Buffer->Height;
	
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
		ArrayCount(Input->Controllers[0].Buttons));
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		Memory->IsInitialized = true;
	}
	
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
			
		}

	}

	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 0x00FF00FF);
	DrawRectangle(Buffer, 10.0f, 10.0f, 40.0f, 40.0f, 0x0000FFFF);
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