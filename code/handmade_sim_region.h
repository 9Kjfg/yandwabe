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
	EntityType_Floor,
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
	// TODO: Collides and ZSupported probably can be for _non_ colliding entities 
	EntityFlag_Collides = (1 << 0),
	EntityFlag_Nonspatial = (1 << 1),
	EntityFlag_Moveable = (1 << 2),

	EntityFlag_Simming = (1 << 30)
};

struct sim_entity_collision_volume 
{
	v3 OffsetP;
	v3 Dim;
};

struct sim_entity_traversable_point
{
	v3 P;
};

struct sim_entity_collision_volume_group
{
	sim_entity_collision_volume TotalVolume;

	// TODO: Volume is always expected to be greated than 0 if the entity
	// has any volume... in the future, this could be compressed of necessary to say
	// that the VolumeCount can be 0 if the TotalVolume should be used as the only
	// collision volume for the entity
	u32 VolumeCount;
	sim_entity_collision_volume *Volumes;

	u32 TraversableCount;
	sim_entity_traversable_point *Traversables;
};

struct sim_entity
{
	// NOTE: These are only for the sim region
	world_chunk *OldChunk;
	u32 StorageIndex;
	u32 Updatable;
	//
    
    entity_type Type;
	u32 Flags;

    v3 P;
	v3 dP;

	r32 DistanceLimit;

	sim_entity_collision_volume_group *Collision;

	r32 FacingDirection;
	r32 tBob;

	s32 dAbsTileZ;

	// TODO: Should hitpoints themselves be entites;
	u32 HitPointMax;
	hit_point HitPoint[16];

	entity_reference Sword;

	// TODO: Only for stairwells
	v2 WalkableDim;
	r32 WalkableHeight;
};

struct sim_entity_hash
{
    sim_entity *Ptr;
    uint32 Index;
};

introspect(category:"regular butter") struct sim_region
{
    // TODO: Need a hash table here to map stored entity indeces
    // to sim entities
    
    world *World;
	r32 MaxEntityRadius;
	r32 MaxEntityVelocity;

    world_position Origin;
    rectangle3 Bounds;
	rectangle3 UpdatableBounds;

    u32 MaxEntityCount;
    u32 EntityCount;
    sim_entity *Entities;

    // NOTE: Must be a power of two
    sim_entity_hash Hash[4096];
};

#define HANDMADE_SIMULATION_REGION_H
#endif