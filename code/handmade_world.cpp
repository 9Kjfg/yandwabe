
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define TILES_PER_CHUNK 8

#define TILE_CHUNK_UNINITIALIZED INT32_MAX

inline world_position
NullPosition()
{
	world_position Result = {};

	Result.ChunkX = TILE_CHUNK_UNINITIALIZED;

	return(Result);
}

inline bool32
IsValid(world_position P)
{
	bool32 Result = (P.ChunkX != TILE_CHUNK_UNINITIALIZED);
	return(Result);
}

inline bool32
IsCannonical(real32 ChunkDim, real32 TileRel)
{
	// TODO: Fix floating point math  so this can be exact <
	real32 Epsilon = 0.01f;
	bool32 Result = (
		(TileRel >= -(0.5f*ChunkDim + Epsilon)) &&
		(TileRel <= (0.5f*ChunkDim + Epsilon)));

	return(Result);
}

inline bool32
IsCannonical(world *World, v3 Offset)
{
	bool32 Result = 
		(IsCannonical(World->ChunkDimInMeters.x, Offset.x) &&
		IsCannonical(World->ChunkDimInMeters.y, Offset.y) &&
		IsCannonical(World->ChunkDimInMeters.z, Offset.z));

	return(Result);
}

inline bool32
AreInSameChunk(world *World, world_position *A, world_position *B)
{
	Assert(IsCannonical(World, A->Offset_));
	Assert(IsCannonical(World, B->Offset_));

	bool32 Result = 
		((A->ChunkX == B->ChunkX) &&
		(A->ChunkY == B->ChunkY) &&
		(A->ChunkZ == B->ChunkZ));

	return(Result);
}

inline bool32
AreInSameChunk(world *World, world_chunk *A, world_position *B)
{
	Assert(IsCannonical(World, B->Offset_));

	bool32 Result = 
		((A->ChunkX == B->ChunkX) &&
		(A->ChunkY == B->ChunkY) &&
		(A->ChunkZ == B->ChunkZ));

	return(Result);
}

inline void
ClearWorldEntityBlock(world_entity_block *Block)
{
	Block->EntityCount = 0;
	Block->Next = 0;
	Block->EntityDataSize = 0;
}

inline world_chunk **
GetWorldChunkInternal(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ)
{
	TIMED_FUNCTION();

	Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
	Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);

	// TODO: BETER HASH FUNCTION
	uint32 HashValue = 19*ChunkX + 7*ChunkY + 3*ChunkZ;
	uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
	Assert(HashSlot < ArrayCount(World->ChunkHash));

	
	world_chunk **Chunk = &World->ChunkHash[HashSlot];
	while (*Chunk &&
		!((ChunkX == (*Chunk)->ChunkX) &&
		(ChunkY == (*Chunk)->ChunkY) &&
		(ChunkZ == (*Chunk)->ChunkZ)))
	{
		Chunk = &(*Chunk)->NextInHash;
	}

	return(Chunk);
}

inline world_chunk *
GetWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
	memory_arena *Arena = 0)
{
	world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY, ChunkZ);
	world_chunk *Result = *ChunkPtr;
	if (!Result && Arena)
	{
		Result = PushStruct(Arena, world_chunk, NoClear());
		Result->FirstBlock = 0;
		Result->ChunkX = ChunkX;
		Result->ChunkY = ChunkY;
		Result->ChunkZ = ChunkZ;

		Result->NextInHash = *ChunkPtr;
		*ChunkPtr =  Result;
	}

	return(Result);
}

internal world_chunk *
RemoveWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
	memory_arena *Arena = 0)
{
	world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY, ChunkZ);
	world_chunk *Result = *ChunkPtr;
	if (Result)
	{
		*ChunkPtr = Result->NextInHash;
	}

	return(Result);
}

internal world *
CreateWorld(v3 ChunkDimInMeters, memory_arena *ParentArena)
{
	world *World = PushStruct(ParentArena, world);

	World->ChunkDimInMeters = ChunkDimInMeters;
	World->FirstFree = 0;
	SubArena(&World->Arena, ParentArena, GetArenaSizeRemaining(ParentArena), NoClear());

	return(World);
}

inline void
RecanonicalizeCoord(real32 ChunkDim, int32 *Tile, real32 *TileRel)
{
	// TODO: Need to do something that doesn't use the divide/multiply method
	// for recanonicalizing because this can end up rounding back on to the tile
	// you just came from

	// NOTE: Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
	// within the safe margin!
	// TODO: Assert that we are nowhere near the edges of the world

	int32 Offset = RoundReal32ToInt32(*TileRel / ChunkDim);
	*Tile += Offset; 
	*TileRel -= Offset*ChunkDim;

	Assert(IsCannonical(ChunkDim, *TileRel));
}

inline world_position
MapIntoChunkSpace(world *World, world_position BasePos, v3 Offest)
{
	world_position Result = BasePos;

	Result.Offset_ += Offest;
	RecanonicalizeCoord(World->ChunkDimInMeters.x, &Result.ChunkX, &Result.Offset_.x);
	RecanonicalizeCoord(World->ChunkDimInMeters.y, &Result.ChunkY, &Result.Offset_.y);
	RecanonicalizeCoord(World->ChunkDimInMeters.z, &Result.ChunkZ, &Result.Offset_.z);

	return(Result);
}

inline v3
Subtract(world *World, world_position *A, world_position *B)
{
	v3 dTile = {
		(real32)A->ChunkX - (real32)B->ChunkX,
		(real32)A->ChunkY - (real32)B->ChunkY,
		(real32)A->ChunkZ - (real32)B->ChunkZ};

	v3 Result = Hadamard(World->ChunkDimInMeters, dTile) + (A->Offset_ - B->Offset_);

	return(Result);
}

inline world_position
CenteredChunkPoint(uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ)
{
	world_position Result = {};

	Result.ChunkX = ChunkX;
	Result.ChunkY = ChunkY;
	Result.ChunkZ = ChunkZ;

	return(Result);
}

inline world_position
CenteredChunkPoint(world_chunk *Chunk)
{
	world_position Result = CenteredChunkPoint(Chunk->ChunkX, Chunk->ChunkY, Chunk->ChunkZ);
	return(Result);
}

inline b32
HasRoomFor(world_entity_block *Block, u32 Size)
{
	b32 Result = ((Block->EntityDataSize + Size) <= sizeof(Block->EntityData));
	return(Result);
}

internal void
PackEntityIntoChunk(world *World, entity *Source, world_chunk *Chunk)
{
	u32 PackSize = sizeof(Source);

	if (!Chunk->FirstBlock || !HasRoomFor(Chunk->FirstBlock, PackSize))
	{
		if (!World->FirstFreeBlock)
		{
			World->FirstFreeBlock = PushStruct(&World->Arena, world_entity_block);
			World->FirstFreeBlock->Next = 0;
		}

		Chunk->FirstBlock = World->FirstFreeBlock;
		World->FirstFreeBlock = Chunk->FirstBlock->Next;

		ClearWorldEntityBlock(Chunk->FirstBlock);
	}

	world_entity_block *Block = Chunk->FirstBlock;

	Assert(HasRoomFor(Block, PackSize));
	u8 *Dest = (Block->EntityData + Block->EntityDataSize);
	Block->EntityDataSize += PackSize;

	*(entity *)Dest = *Source;
}

internal void
PackEntityIntoWorld(world *World, entity *Source, world_position At)
{
	world_chunk *Chunk = GetWorldChunk(World, At.ChunkX, At.ChunkY, At.ChunkZ, &World->Arena);
	PackEntityIntoChunk(World, Source, Chunk);
}

inline void
AddBlockToFreeList(world *World, world_entity_block *Old)
{
	Old->Next = World->FirstFreeBlock;
	World->FirstFreeBlock = Old;
}

inline void
AddChunkToFreeList(world *World, world_chunk *Old)
{
	Old->NextInHash = World->FirstFreeChunk;
	World->FirstFreeChunk = Old;
}

