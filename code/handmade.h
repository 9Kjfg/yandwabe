#if !defined(HANDMADE_H)

#include "handmade_platform.h"

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

//
//
//

struct memory_arena
{
	memory_index Size;
	uint8 *Base;
	memory_index Used;
};

inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
	Arena->Size = Size;
	Arena->Base = (uint8 *)Base;
	Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
inline void *
PushSize_(memory_arena *Arena, memory_index Size)
{
	Assert((Arena->Used + Size) <= Arena->Size);
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return(Result);
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance));

inline void
ZeroSize(memory_index Size, void *Ptr)
{
	uint8 *Byte = (uint8 *)Ptr;
	while (Size--)
	{
		*Byte++ = 0;
	}
}

#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_entity.h"

struct loaded_bitmap
{
	int32 Width;
	int32 Height;
	int32 Pitch;
	void *Memory;
};

struct hero_bitmaps
{
	v2 Align;
	loaded_bitmap Head;
	loaded_bitmap Cape;
	loaded_bitmap Torso;
};

struct low_entity
{
	// TODO: It seems like we have to store ChunkX/Y/Z with each
	// entity because even though the sim region gather doesn't need it 
	// at first, and we could get by without it, entity references pull
	// in etities WITHOUT going through their world_chunk, and thus
	// still need to know the ChunkX/Y/Z
	world_position P;
	sim_entity Sim;
};

enum entity_residence
{
	EntityResidence_Nonexistent,
	EntityResidence_Low,
	EntityResidence_High
};

struct entity_visible_piece
{
	loaded_bitmap *Bitmap;
	v2 Offset;
	real32 OffsetZ;
	real32 EntityZC;
	
	real32 R, G, B, A;
	v2 Dim;
};

struct controlled_hero
{
	uint32 EntityIndex;

	// NOTE: These are the controller requests for simulation
	v2 ddP;
	v2 dSword;
	real32 dZ;
};

struct pairwise_collision_rule
{
	bool32 CanCollide;
	uint32 StorageIndexA;
	uint32 StorageIndexB;

	pairwise_collision_rule *NextInHash;
};

struct game_state;
internal void AddCollisionRule(game_state *GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 ShouldCollide);
internal void ClearCollisionRulesFor(game_state *GameState, uint32 StorageIndex);

struct game_state
{
	memory_arena WorldArena;
	world *World;

	// TODO: Should we allow spit-screen?
	uint32 CameraFollowingEntityIndex;
	world_position CameraP;

	controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];

	// TODO: Change the name to "stored entity"
	uint32 LowEntityCount;
	low_entity LowEntities[4096];

	loaded_bitmap Grass[2];
	loaded_bitmap Stone[4];
	loaded_bitmap Tuft[3];

	loaded_bitmap Backdrop;
	loaded_bitmap Shadow;
	hero_bitmaps HeroBitmaps[4];

	loaded_bitmap Tree;
	loaded_bitmap Sword;
	loaded_bitmap Stairwell;
	real32 MetersToPixels;

	// TODOL Must be power of to
	pairwise_collision_rule *CollisionRuleHash[256];
	pairwise_collision_rule *FirstFreeCollisionRule;

	sim_entity_collision_volume_group *NullCollision;
	sim_entity_collision_volume_group *SwordCollision;
	sim_entity_collision_volume_group *StairCollision;
	sim_entity_collision_volume_group *PlayerCollision;
	sim_entity_collision_volume_group *MonsterCollision;
	sim_entity_collision_volume_group *FamiliarCollision;
	sim_entity_collision_volume_group *WallCollision;
	sim_entity_collision_volume_group *StandartRoomCollision;

	world_position GroundBufferP;
	loaded_bitmap GroundBuffer;
};

// TODO: This is dumb, this should gust be just be part of
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.
struct entity_visible_piece_group
{
	game_state *GameState;
	uint32 PieceCount;
	entity_visible_piece Pieces[32];
};

inline low_entity *
GetLowEntity(game_state *GameState, uint32 Index)
{
	low_entity *Result = 0;
	
	if ((Index > 0) && (Index < GameState->LowEntityCount))
	{
		Result = GameState->LowEntities + Index;
	}

	return(Result);	
}

#define HANDMADE_H
#endif