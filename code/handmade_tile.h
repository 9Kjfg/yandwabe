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

	uint32 AbsTileX;
	uint32 AbsTileY;
	uint32 AbsTileZ;

	//NOTE: These are the offsets from the tile center
	v2 Offset;
};


struct tile_chunk_position
{
	uint32 TileChunkX;
	uint32 TileChunkY;
	uint32 TileChunkZ;

	uint32 RelTileX;
	uint32 RelTileY;
};

struct tile_chunk
{
	// TODO: Real structure for a tile
	uint32 *Tiles;
};

struct tile_map
{
    uint32 ChunkShift;
	uint32 ChunkMask;
	uint32 ChunkDim;

	real32 TileSideInMeters;

	//TODO: REAL sparseness so anywhere in the world can be
	// represented without the giant pointer array
	uint32 TileChunkCountX;
	uint32 TileChunkCountY;
	uint32 TileChunkCountZ;
    tile_chunk *TileChunks;
};

#define HANDMADE_TILE_H
#endif