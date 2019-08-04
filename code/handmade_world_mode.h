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
	entity_id CameraFollowingEntityIndex;
	world_position CameraP;

	// TODOL Must be power of to
	pairwise_collision_rule *CollisionRuleHash[256];
	pairwise_collision_rule *FirstFreeCollisionRule;

	sim_entity_collision_volume_group *NullCollision;
	sim_entity_collision_volume_group *StairCollision;
	sim_entity_collision_volume_group *HeroBodyCollision;
	sim_entity_collision_volume_group *HeroHeadCollision;
	sim_entity_collision_volume_group *MonsterCollision;
	sim_entity_collision_volume_group *FamiliarCollision;
	sim_entity_collision_volume_group *WallCollision;
	sim_entity_collision_volume_group *FloorCollision;

	real32 Time;

	random_series GeneralEntropy;
	random_series EffectsEntropy; // NOTE: This is entropy that doesn't affect the gameplay
	real32 tSine;

#define PARTICLE_CEL_DIM 16
	u32 NextParticle;
	particle Particles[256];

	b32 CreationBufferLocked;// TODO: Remove this eventually, just for catching bugs
	low_entity CreationBuffer;
	u32 LastUsedEntityStorageIndex; // TODO: Worry about this wrapping - free list for IDs?

	particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
};

struct game_state;
internal void PlayWorld(game_state *GameState, transient_state *TranState);

#define HANDMADE_WORLD_MODE_H
#endif