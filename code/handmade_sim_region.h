#if !defined(HANDMADE_SIMULATION_REGION_H)

struct move_spec
{
	bool32 UnitMaxAccelVector;
	real32 Speed;
	real32 Drag;
};

struct entity_hash
{
    entity *Ptr;
    entity_id Index;
};

struct sim_region
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
    entity *Entities;

    // NOTE: Must be a power of two
    entity_hash Hash[4096];
};

#define HANDMADE_SIMULATION_REGION_H
#endif