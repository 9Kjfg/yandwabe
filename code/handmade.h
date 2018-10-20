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

struct canonical_position
{
	/* TODO:
		Take the tile map x and y
		and the tile x and y

		and pack them into single 32-bit values for x and y
		where there is some low bits for the tile index
		and the high bits are the tile "page"

		(NOTE we can eliminate the need for floor!)
	*/
	int32 TileMapX;
	int32 TileMapY;

	int32 TileX;
	int32 TileY;
	
	/* TODO:
		Convert these to math-friendly resolustion independent representation of 
		world units relative to a tile
	*/
	// NOTE: This is tile-relative X and Y
	real32 TileRelX;
	real32 TileRelY;
};

// TODO: Is this ever necessary
struct raw_position
{
	int32 TileMapX;
	int32 TileMapY;

	// NOTE: Tile-map relative X and Y
	real32 X;
	real32 Y;
};

struct tile_map
{
	uint32 *Tiles;
};

struct world
{
	real32 TileSideInMeters;
	int32 TileSideInPixels;

	int32 CountX;
	int32 CountY;

	real32 UpperLeftX;
	real32 UpperLeftY;

	// TODO: BEginer;s sparseness
	int32 TileMapCountX;
	int32 TileMapCountY;

	tile_map *TileMaps;
};

struct game_state
{
	// TODO: Player state should be canonical position
	int32 PlayerTileMapX;
	int32 PlayerTileMapY;

	real32 PlayerX;
	real32 PlayerY;
};

#define HANDMADE_H
#endif