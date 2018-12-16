
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

#define TILES_PER_CHUNK 16

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
		(IsCannonical(World->ChunkDimInMeters.X, Offset.X) &&
		IsCannonical(World->ChunkDimInMeters.Y, Offset.Y) &&
		IsCannonical(World->ChunkDimInMeters.Z, Offset.Z));

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

inline world_chunk *
GetWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
	memory_arena *Arena = 0)
{
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

	world_chunk *Chunk = World->ChunkHash + HashSlot;
	
	do
	{
		if ((ChunkX == Chunk->ChunkX) &&
			(ChunkY == Chunk->ChunkY) &&
			(ChunkZ == Chunk->ChunkZ))
		{
			break;
		}

		if (Arena && (Chunk->ChunkX != TILE_CHUNK_UNINITIALIZED) && (!Chunk->NextInHash))
		{
			Chunk->NextInHash = PushStruct(Arena, world_chunk);
			Chunk = Chunk->NextInHash;
			Chunk->ChunkX = TILE_CHUNK_UNINITIALIZED;
		}

		if (Arena && (Chunk->ChunkX == TILE_CHUNK_UNINITIALIZED))
		{
			Chunk->ChunkX = ChunkX;
			Chunk->ChunkY = ChunkY;
			Chunk->ChunkZ = ChunkZ;
			
			Chunk->NextInHash = 0;
			break;
		}

		Chunk = Chunk->NextInHash;
	} while(Chunk);

	return(Chunk);
}

internal void
InitializeWorld(world *World, real32 TileSideInMeters, real32 TileDepthInMeters)
{
	World->TileSideInMeters = TileSideInMeters;
	World->ChunkDimInMeters = {
		(real32)TILES_PER_CHUNK*World->TileSideInMeters,
		(real32)TILES_PER_CHUNK*World->TileSideInMeters,
		TileDepthInMeters};
	World->TileDepthInMeters = TileDepthInMeters;
	World->FirstFree = 0;

	for (uint32 ChunkIndex = 0;
		ChunkIndex < ArrayCount(World->ChunkHash);
		++ChunkIndex)
	{
		World->ChunkHash[ChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
		World->ChunkHash[ChunkIndex].FirstBlock.EntityCount = 0;
	}
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
	RecanonicalizeCoord(World->ChunkDimInMeters.X, &Result.ChunkX, &Result.Offset_.X);
	RecanonicalizeCoord(World->ChunkDimInMeters.Y, &Result.ChunkY, &Result.Offset_.Y);
	RecanonicalizeCoord(World->ChunkDimInMeters.Z, &Result.ChunkZ, &Result.Offset_.Z);

	return(Result);
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ,
	v3 AdditionOffset = V3(0, 0, 0))
{
	world_position BasePos = {};

	v3 TileDim = V3(World->TileSideInMeters, World->TileSideInMeters, World->TileDepthInMeters);
	v3 Offset = Hadamard(TileDim, V3((real32)AbsTileX, (real32)AbsTileY, (real32)AbsTileZ));
	world_position Result = MapIntoChunkSpace(World, BasePos, AdditionOffset + Offset);

	Assert(IsCannonical(World, Result.Offset_));

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

inline void
ChangeEntityLocationRaw(memory_arena *Arena, world *World, uint32 LowEntityIndex,
	world_position *OldP, world_position *NewP)
{
	// TODO: if this moves an entity  into the camera bounds, should it automatically
	// go into the high set immediately
	// If it moves _out_ of the camera  bounds. should it be removes from the high set
	// immediately

	Assert(!OldP || IsValid(*OldP));
	Assert(!NewP || IsValid(*NewP));

	if (OldP && NewP && AreInSameChunk(World, OldP, NewP))
	{
		// NOTE: Leave entity where it is
	}
	else
	{
		if (OldP)
		{
			// NOTE: Pull the entity out of its old entity block
			world_chunk *Chunk = GetWorldChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
			Assert(Chunk);
			if (Chunk)
			{
				bool32 NotFound = true;
				world_entity_block *FirstBlock = &Chunk->FirstBlock;
				for (world_entity_block *Block = FirstBlock;
					Block && NotFound;
					Block = Block->Next)
				{
					for (uint32 Index = 0;
						(Index < Block->EntityCount) && NotFound;
						++Index)
					{
						if (Block->LowEntityIndex[Index] == LowEntityIndex)
						{
							Assert(FirstBlock->EntityCount > 0);
							Block->LowEntityIndex[Index] = 
									FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];
							if (FirstBlock->EntityCount == 0)
							{
								if (FirstBlock->Next)
								{
									world_entity_block *NextBlock = FirstBlock->Next;
									*FirstBlock = *NextBlock;

									NextBlock->Next = World->FirstFree;
									World->FirstFree = NextBlock;
								}
							}

							NotFound = false;
						}
					}
				}
			}
		}

		if (NewP)
		{
			// NOTE: Insert the entity into its new entity block
			world_chunk *Chunk = GetWorldChunk(World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena);
			Assert(Chunk);

			world_entity_block *Block = &Chunk->FirstBlock;
			if (Block->EntityCount == ArrayCount(Block->LowEntityIndex))
			{
				// We're out of room, get a new block
				world_entity_block *OldBlock = World->FirstFree;
				if (OldBlock)
				{
					World->FirstFree = OldBlock->Next;
				}
				else
				{
					OldBlock = PushStruct(Arena, world_entity_block);
				}
				*OldBlock = *Block;
				Block->Next = OldBlock;
				Block->EntityCount = 0;
			}

			Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
			Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
		}
	}
}

internal void
ChangeEntityLocation(memory_arena *Arena, world *World, uint32 LowEntityIndex,
	low_entity *LowEntity, world_position NewPInit)
{
	world_position *OldP = 0;
	world_position *NewP = 0;

	if (!IsSet(&LowEntity->Sim, EntityFlag_Nonspatial) && IsValid(LowEntity->P))
	{
		OldP = &LowEntity->P;
	}

	if (IsValid(NewPInit))
	{
		NewP = &NewPInit;
	}

	ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
	if (NewP)
	{
		LowEntity->P = *NewP;
		ClearFlags(&LowEntity->Sim, EntityFlag_Nonspatial);
	}
	else
	{
		LowEntity->P = NullPosition();
		AddFlags(&LowEntity->Sim, EntityFlag_Nonspatial);
	}
}