#if !defined(HANDMADE_H)

/*
	HANDMADE_INTERNAL:
		0 - Build for public
		1 - Build for developer only

	HANDMADE_SLOW:
		0 - Not slow code allowed
		1 - Slow code welcome
*/


#include "handmade_platform.h"

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

#if HANDMADE_SLOW
#define Assert(Expression) if (!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
//TODO: swap, min, max ...macros?

inline uint32
SafeTruncateUint64(uint64 Value)
{	
	// TODO: Define for maximum values
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return(Result);
}

inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));

	game_controller_input *Result = &Input->Controllers[ControllerIndex];
	return(Result);
}

//
//
//

#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct memory_arena
{
	memory_index Size;
	uint8 *Base;
	memory_index Used;
};

struct world
{
	tile_map *TileMap;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	tile_map_position PlayerP;
	// TODO: Player state should be canonical position
};

#define HANDMADE_H
#endif