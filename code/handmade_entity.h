#if !defined(HANDMADE_ENTITY_H)

#define InvalidP V3(10000.0f, 10000.0f, 10000.0f)

struct entity;

enum entity_type
{
	EntityType_Null,

	EntityType_HeroBody,
	EntityType_HeroHead,

	EntityType_Wall,
	EntityType_Floor,
	EntityType_FloatyThingForNow,
	EntityType_Familiar,
	EntityType_Monster,
	EntityType_Stairwell
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
	// TODO: Bake this down into one variable
	uint8 Flag;
	uint8 FilledAmount;
};

struct entity_id
{
	u32 Value;
};

enum entity_relationship
{
	Relationship_None,
	Relationship_Paired,
};

struct entity_reference
{
    entity *Ptr;
	entity_id Index;
};

inline b32 ReferenceAreEqual(entity_reference A, entity_reference B)
{
	b32 Result = ((A.Ptr == B.Ptr) &&
 		(A.Index.Value == B.Index.Value));
	return(Result);
}

struct traversable_reference
{
	entity_reference Entity;
	u32 Index;
};

inline b32 IsEqual(traversable_reference A, traversable_reference B)
{
	b32 Result = (ReferenceAreEqual(A.Entity, B.Entity) && (A.Index == B.Index));
	return(Result);
}

enum entity_flags
{
	EntityFlag_Collides = (1 << 0),
	EntityFlag_Moveable = (1 << 1),
	EntityFlag_Deleted = (1 << 2),
};

struct entity_collision_volume 
{
	v3 OffsetP;
	v3 Dim;
};

struct entity_collision_volume_group
{
	entity_collision_volume TotalVolume;

	// TODO: Volume is always expected to be greated than 0 if the entity
	// has any volume... in the future, this could be compressed of necessary to say
	// that the VolumeCount can be 0 if the TotalVolume should be used as the only
	// collision volume for the entity
	u32 VolumeCount;
	entity_collision_volume *Volumes;
};

struct entity_traversable_point
{
	v3 P;
	entity *Occupier;
};

struct move_spec
{
	bool32 UnitMaxAccelVector;
	real32 Speed;
	real32 Drag;
};

enum entity_movement_mode
{
	MovementMode_Planted,
	MovementMode_Hopping,
};

struct entity_visible_piece
{
	v4 Color;
	asset_type_id AssetType;
	r32 Height;
};

struct entity
{
	entity_id ID;
	u32 Updatable;
	
	//
    // NOTE:
	//

	brain_type BrainType;
	brain_slot BrainSlot;
	brain_id BrainID;

	//
	//	NOTE: Everything bellow here is NOT out yet
	//
    entity_type Type;
	u32 Flags;

    v3 P;
	v3 dP;
	v3 ddP; // TODO: Do not pack this

	r32 DistanceLimit;

	entity_collision_volume_group *Collision;

	r32 FacingDirection;
	r32 tBob;
	r32 dtBob;
	r32 ddtBob; // TODO: Do not pack this

	s32 dAbsTileZ;

	// TODO: Should hitpoints themselves be entites;
	u32 HitPointMax;
	hit_point HitPoint[16];

	// TODO: Only for stairwells
	v2 WalkableDim;
	r32 WalkableHeight;

	entity_movement_mode MovementMode;
	move_spec MoveSpec; // TODO: Do not pack this
	r32 tMovement;
	traversable_reference Occupying;
	traversable_reference CameFrom;

	v2 XAxis;
	v2 YAxis;

	v2 FloorDisplace;

	u32 TraversableCount;
	entity_traversable_point Traversables[16];

	u32 PieceCount;
	entity_visible_piece Pieces[4];
};

inline bool32
IsSet(entity *Entity, uint32 Flag)
{
	bool32 Result = Entity->Flags & Flag;
	return(Result);
}

inline b32 IsDeleted(entity *E)
{
	b32 Result = IsSet(E, EntityFlag_Deleted);
	return(Result);
}

inline void
AddFlags(entity *Entity, uint32 Flag)
{
	Entity->Flags |= Flag;
}

inline void
ClearFlags(entity *Entity, uint32 Flag)
{
	Entity->Flags &= ~Flag;
}

inline void
MakeEntitySpatial(entity *Entity, v3 P, v3 dP)
{
	Entity->P = P;
	Entity->dP = dP;
}

inline v3
GetEntityGroundPoint(entity *Entity, v3 ForEntityP)
{
	v3 Result = ForEntityP;
	return(Result);
}

inline v3
GetEntityGroundPoint(entity *Entity)
{
	v3 Result = GetEntityGroundPoint(Entity, Entity->P);
	return(Result);
}

inline real32
GetStairGround(entity *Entity, v3 AtGroundPoint)
{
	Assert(Entity->Type == EntityType_Stairwell);

	rectangle2 RegionRect = RectCenterDim(Entity->P.xy, Entity->WalkableDim);
	v2 Bary = Clamp01(GetBarycentric(RegionRect, AtGroundPoint.xy));
	real32 Result = Entity->P.z + Bary.y*Entity->WalkableHeight;

	return(Result);
}

#define HANDMADE_ENTITY_H
#endif