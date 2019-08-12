#if !defined(HANDMADE_ENTITY_H)

#define InvalidP V3(10000.0f, 10000.0f, 10000.0f)

inline bool32
IsSet(entity *Entity, uint32 Flag)
{
	bool32 Result = Entity->Flags & Flag;
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
MakeEntityNonSpatial(entity *Entity)
{
	AddFlags(Entity, EntityFlag_Nonspatial);
	Entity->P = InvalidP;
}

inline void
MakeEntitySpatial(entity *Entity, v3 P, v3 dP)
{
	ClearFlags(Entity, EntityFlag_Nonspatial);
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