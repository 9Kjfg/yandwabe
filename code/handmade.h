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

	uint32 TempCount;
};

struct temporary_memory
{
	memory_arena *Arena;
	memory_index Used;
};

inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
	Arena->Size = Size;
	Arena->Base = (uint8 *)Base;
	Arena->Used = 0;
	Arena->TempCount = 0;
}

inline memory_index
GetAlignmentOffset(memory_arena *Arena, memory_index Alignment = 4)
{
	memory_index AlignmentOffset = 0;

	memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
	memory_index AlignmentMask = Alignment - 1;
	if (ResultPointer & AlignmentMask)
	{
		AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
	}

	return(AlignmentOffset);
}

inline memory_index 
GetArenaSizeRemaining(memory_arena *Arena, memory_index Alignment = 4)
{
	memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Alignment));
	return(Result);
}

#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), ##__VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(Arena, (Count)*sizeof(type), ##__VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ##__VA_ARGS__)
inline void *
PushSize_(memory_arena *Arena, memory_index Size, memory_index Alignment = 4)
{
	memory_index AlignmentOffset = GetAlignmentOffset(Arena);
	Size += AlignmentOffset;

	Assert((Arena->Used + Size) <= Arena->Size);
	void *Result = Arena->Base + Arena->Used + AlignmentOffset;
	Arena->Used += Size;

	return(Result);
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance));

inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
	temporary_memory Result;

	Result.Arena = Arena;
	Result.Used = Arena->Used;

	++Arena->TempCount;

	return(Result);
}

inline void
EndTemporaryMemory(temporary_memory TempMem)
{
	memory_arena *Arena = TempMem.Arena;
	Assert(Arena->Used >= TempMem.Used);
	Arena->Used = TempMem.Used;
	Assert(Arena->TempCount > 0);
	--Arena->TempCount;
}

inline void
CheckArena(memory_arena *Arena)
{
	Assert(Arena->TempCount == 0);
}

inline void
SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size, memory_index Alignment = 16)
{
	Result->Size = Size;
	Result->Base = (uint8 *)PushSize_(Arena, Size, Alignment);
	Result->Used = 0 ;
	Result->TempCount = 0;
}

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
#include "handmade_render_group.h"

struct hero_bitmaps
{
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

struct ground_buffer
{
	// NOTE: An invalid P tells us that ground buffer has not been filled
	world_position P; // NOTE: This is the center of the bitmap
	loaded_bitmap Bitmap;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	real32 TypicalFloorHeight;

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

	real32 Time;

	loaded_bitmap TestDiffuse; // TODO: Re-fill this guy with gray
	loaded_bitmap TestNormal;
};

struct task_with_memory
{
	bool32 BeingUsed;
	memory_arena Arena;

	temporary_memory MemoryFlush;
};

struct transient_state
{
	bool32 IsInitialized;
	memory_arena TranArena;
	
	task_with_memory Tasks[4];
	
	uint32 GroundBufferCount;
	ground_buffer *GroundBuffers;

	platform_work_queue *HighPriorityQueue;
	platform_work_queue *LowPriorityQueue;
	uint64 Pad; 

	uint32 EnvMapWidth;
	uint32 EnvMapHeight;
	// NOTE: 0 is buttom, 1 is middle, 2 is top
	environment_map EnvMaps[3];

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

global_variable platform_add_entry *PlatformAddEntry;
global_variable platform_complete_all_work *PlatformCompleteAllWork;

#define HANDMADE_H
#endif