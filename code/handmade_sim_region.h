#if !defined(HANDMADE_SIMULATION_REGION_H)

struct move_spec
{
	bool32 UnitMaxAccelVector;
	real32 Speed;
	real32 Drag;
};

enum entity_type
{
	EntityType_Null,

	EntityType_Hero,
	EntityType_Wall,
	EntityType_Familiar,
	EntityType_Monster,
	EntityType_Sword,
	EntityType_Stairwell
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	// TODO: Bake this down into one variable
	uint8 Flag;
	uint8 FilledAmount;
};

struct sim_entity;
union entity_reference
{
    sim_entity *Ptr;
    uint32 Index;
};

enum sim_entity_flags
{
	EntityFlag_Collides = (1 << 0),
	EntityFlag_Nonspatial = (1 << 1),
	EntityFlag_Moveable = (1 << 2),
	EntityFlag_ZSupported = (1 << 3),

	EntityFlag_Simming = (1 << 30)
};

struct sim_entity
{
	// NOTE: These are only for the sim region
	world_chunk *OldChunk;
	uint32 StorageIndex;
	uint32 Updatable;
	//
    
    entity_type Type;
	uint32 Flags;

    v3 P;
	v3 dP;

	real32 DistanceLimit;

	v3 Dim;

	uint32 FacingDirection;
	real32 tBob;

	int32 dAbsTileZ;

	// TODO: Should hitpoints themselves be entites;
	uint32 HitPointMax;
	hit_point HitPoint[16];

	entity_reference Sword;

	// TODO: Only for stairwells
	real32 WalkableHeight;

};

struct sim_entity_hash
{
    sim_entity *Ptr;
    uint32 Index;
};

struct sim_region
{
    // TODO: Need a hash table here to map stored entity indeces
    // to sim entities
    
    world *World;
	real32 MaxEntityRadius;
	real32 MaxEntityVelocity;

    world_position Origin;
    rectangle3 Bounds;
	rectangle3 UpdatableBounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity *Entities;


    // NOTE: Must be a power of two
    sim_entity_hash Hash[4096];
};

#define HANDMADE_SIMULATION_REGION_H
#endif