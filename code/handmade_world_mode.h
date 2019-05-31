#if !defined(HANDMADE_WORLD_MODE_H)

struct game_mode_world;

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

internal void AddCollisionRule(game_mode_world *WorldMode, uint32 StorageIndexA, uint32 StorageIndexB, bool32 ShouldCollide);
internal void ClearCollisionRulesFor(game_mode_world *WorldMode, uint32 StorageIndex);

struct ground_buffer
{
	// NOTE: An invalid P tells us that ground buffer has not been filled
	world_position P; // NOTE: This is the center of the bitmap
	loaded_bitmap Bitmap;
};

struct particle_cel
{
	real32 Density;
	v3 VelocityTimesDensity;
};

struct particle
{
	bitmap_id BitmapID;
	v3 P;
	v3 dP;
	v3 ddP;
	v4 dColor;
	v4 Color;
};

struct game_mode_world
{
	world *World;
	real32 TypicalFloorHeight;

	// TODO: Should we allow spit-screen?
	uint32 CameraFollowingEntityIndex;
	world_position CameraP;

	// TODO: Change the name to "stored entity"
	uint32 LowEntityCount;
	low_entity LowEntities[4096];

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

	random_series GeneralEntropy;
	random_series EffectsEntropy; // NOTE: This is entropy that doesn't affect the gameplay
	real32 tSine;

#define PARTICLE_CEL_DIM 16
	u32 NextParticle;
	particle Particles[256];

	particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
};

inline low_entity *
GetLowEntity(game_mode_world *WorldMode, uint32 Index)
{
	low_entity *Result = 0;
	
	if ((Index > 0) && (Index < WorldMode->LowEntityCount))
	{
		Result = WorldMode->LowEntities + Index;
	}

	return(Result);	
}

#define HANDMADE_WORLD_MODE_H
#endif