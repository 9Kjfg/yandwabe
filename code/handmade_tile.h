#if !defined(HANDMADE_TILE_H)


// TODO: Replace this with a v3 once we get to v3
struct tile_map_difference
{
	v2 dXY;
	real32 dZ;
};

struct tile_map_position
{
	// NOTE: These are fixed point tile location. The hight
	// bits are the tile chunk index, and the low bits are the tile
	// index in the chunk

	int32 AbsTileX;
	int32 AbsTileY;
	int32 AbsTileZ;

	//NOTE: These are the offsets from the tile center
	v2 Offset_;
};


struct tile_chunk_position
{
	int32 TileChunkX;
	int32 TileChunkY;
	int32 TileChunkZ;

	int32 RelTileX;
	int32 RelTileY;
};

struct tile_chunk
{
	int32 TileChunkX;
	int32 TileChunkY;
	int32 TileChunkZ;

	// TODO: Real structure for a tile
	int32 *Tiles;

	tile_chunk *NextInHash;
};

struct tile_map
{
    int32 ChunkShift;
	int32 ChunkMask;
	int32 ChunkDim;

	real32 TileSideInMeters;

	// NOTE: A the moment, this must ve a power of two
	tile_chunk TileChunkHash[4096];
};

#define HANDMADE_TILE_H
#endif