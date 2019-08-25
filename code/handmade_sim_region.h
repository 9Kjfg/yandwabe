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
    entity_id Index; // TODO: whe are we storing these in the hash??
};

struct brain_hash
{
    brain *Ptr;
    brain_id ID; // TODO: whe are we storing these in the hash??
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

    u32 MaxBrainCount;
    u32 BrainCount;
    brain *Brains;

    // NOTE: Must be a power of two
    entity_hash EntityHash[4096];
    brain_hash BrainHash[256];
};

internal entity_hash *GetHashFromID(sim_region *SimRegion, entity_id ID);

#define HANDMADE_SIMULATION_REGION_H
#endif