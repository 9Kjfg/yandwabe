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

struct tile_chunk_position
{
	uint32 TileChunkX;
	uint32 TileChunkY;

	uint32 RelTileX;
	uint32 RelTileY;
};

struct world_position
{
	/* TODO:
		Take the tile map x and y
		and the tile x and y

		and pack them into single 32-bit values for x and y
		where there is some low bits for the tile index
		and the high bits are the tile "page"

		(NOTE we can eliminate the need for floor!)
	*/

	uint32 AbsTileX;
	uint32 AbsTileY;

	// TODO: should these be from the center of a tile
	// TODO: rename to offset X and Y
	real32 TileRelX;
	real32 TileRelY;
};

struct tile_chunk
{
	uint32 *Tiles;
};

struct world
{
	uint32 ChunkShift;
	uint32 ChunkMask;
	uint32 ChunkDim;

	real32 TileSideInMeters;
	int32 TileSideInPixels;
	real32 MetersToPixels;

	// TODO: Beginer's sparseness
	int32 TileChunkCountX;
	int32 TileChunkCountY;

	tile_chunk *TileChunks;
};

struct game_state
{
	world_position PlayerP;
	// TODO: Player state should be canonical position
};

#define HANDMADE_H
#endif