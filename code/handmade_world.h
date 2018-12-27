#if !defined(HANDMADE_WORLD_H)

struct world_position
{
    // TODO: Puzzler! How can we get rid of abstile* here,
    // and still allow references to entities to ve able to figure
    // out _where the are_ (or rather, which world_chunk the are in?)

	int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;

	//NOTE: These are the offsets from the chunk center
	v3 Offset_;
};


// TODO: Could make this just tile_chunk and hen allow multiple tile chunks per X\Y\Z
struct world_entity_block
{
	uint32 EntityCount;
	uint32 LowEntityIndex[16];
	world_entity_block *Next;
};

struct world_chunk
{
	int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;

	// TODO: Profile this and determine if a pointer would be better here!
	world_entity_block FirstBlock;

	world_chunk *NextInHash;
};

struct world
{
	v3 ChunkDimInMeters;

	// TODO: WorldChunkHash should probably switch to pointers IF
	// tile entity blocks continue to be stored en masse directly in the tile chunk!
	// NOTE: A the moment, this must ve a power of two

	world_chunk ChunkHash[4096];

	world_entity_block *FirstFree;                                                                                                                     
};

#define HANDMADE_WORLD_H
#endif