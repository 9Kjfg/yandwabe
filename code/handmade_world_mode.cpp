
struct add_low_entity_result
{
	low_entity *Low;
	uint32 LowIndex;
};
internal add_low_entity_result
AddLowEntity(game_mode_world *WorldMode, entity_type Type, world_position P)
{
	Assert(WorldMode->LowEntityCount < ArrayCount(WorldMode->LowEntities));
	uint32 EntityIndex = WorldMode->LowEntityCount++;
	
	low_entity *EntityLow = WorldMode->LowEntities + EntityIndex;
	*EntityLow = {};
	EntityLow->Sim.Type = Type;
	EntityLow->Sim.Collision = WorldMode->NullCollision;
	EntityLow->P = NullPosition();

	ChangeEntityLocation(&WorldMode->World->Arena, WorldMode->World, EntityIndex, EntityLow, P);

	add_low_entity_result Result;
	Result.Low = EntityLow;
	Result.LowIndex = EntityIndex;

	// TODO: Do we need to gave a begin/end paradigm for adding 
	// entities so that they can be brought into the high set when they 
	// are added and are in the camera region

	return(Result); 
}

internal void
DeleteLowEntity(game_mode_world *WorldMode, u32 Index)
{
	// TODO: Actually delete
}

internal add_low_entity_result
AddGroundedEntity(game_mode_world *WorldMode, entity_type Type, world_position P,
	sim_entity_collision_volume_group *Collision)
{
	add_low_entity_result Entity = AddLowEntity(WorldMode, Type, P);
	Entity.Low->Sim.Collision = Collision;
	return(Entity);
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ,
	v3 AdditionOffset = V3(0, 0, 0))
{
	world_position BasePos = {};

	real32 TileSideInMeters = 1.4f;
	real32 TileDepthInMeters = 3.0f;

	v3 TileDim = V3(TileSideInMeters, TileSideInMeters, TileDepthInMeters);
	v3 Offset = Hadamard(TileDim, V3((real32)AbsTileX, (real32)AbsTileY, (real32)AbsTileZ));
	world_position Result = MapIntoChunkSpace(World, BasePos, AdditionOffset + Offset);

	Assert(IsCannonical(World, Result.Offset_));

	return(Result);
}

internal add_low_entity_result
AddStandardRoom(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(WorldMode, EntityType_Space, P,
		WorldMode->StandartRoomCollision);

	AddFlags(&Entity.Low->Sim, EntityFlag_Traversable);

	return(Entity);
}

internal add_low_entity_result
AddWall(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(WorldMode, EntityType_Wall, P, WorldMode->WallCollision);
	AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

	return(Entity);
}

internal add_low_entity_result
AddStair(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(WorldMode, EntityType_Stairwell, P, WorldMode->StairCollision);
	
	AddFlags(&Entity.Low->Sim, EntityFlag_Collides);
	Entity.Low->Sim.WalkableDim = Entity.Low->Sim.Collision->TotalVolume.Dim.xy;
	Entity.Low->Sim.WalkableHeight = WorldMode->TypicalFloorHeight;

	return(Entity);
}

internal void
InitHitPoints(low_entity *EntityLow, uint32 HitPointCount)
{
	Assert(HitPointCount <= ArrayCount(EntityLow->Sim.HitPoint));
	EntityLow->Sim.HitPointMax = HitPointCount;
	for (uint32 HitPointIndex = 0;
		HitPointIndex < EntityLow->Sim.HitPointMax;
		++HitPointIndex)
	{
		hit_point *HitPoint = EntityLow->Sim.HitPoint + HitPointIndex;
		HitPoint->Flag = 0;
		HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;
	}
}

internal add_low_entity_result
AddSword(game_mode_world *WorldMode)
{
	add_low_entity_result Entity = AddLowEntity(WorldMode, EntityType_Sword, NullPosition());

	Entity.Low->Sim.Collision = WorldMode->SwordCollision;
	AddFlags(&Entity.Low->Sim, EntityFlag_Moveable);

	return(Entity);
}

internal add_low_entity_result
AddPlayer(game_mode_world *WorldMode)
{
	world_position P = WorldMode->CameraP;
	add_low_entity_result Entity = AddGroundedEntity(WorldMode, EntityType_Hero, P,
		WorldMode->PlayerCollision);
	AddFlags(&Entity.Low->Sim, EntityFlag_Collides|EntityFlag_Moveable);

	InitHitPoints(Entity.Low, 3);

	add_low_entity_result Sword = AddSword(WorldMode);
	Entity.Low->Sim.Sword.Index = Sword.LowIndex;
	
	if (WorldMode->CameraFollowingEntityIndex == 0)
	{
		WorldMode->CameraFollowingEntityIndex = Entity.LowIndex;
	}

	return(Entity);
}

internal add_low_entity_result
AddMonster(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(WorldMode, EntityType_Monster, P,
		WorldMode->MonsterCollision);
	AddFlags(&Entity.Low->Sim, EntityFlag_Collides|EntityFlag_Moveable);

	InitHitPoints(Entity.Low, 3);

	return(Entity);
}

internal add_low_entity_result
AddFamiliar(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
	world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
	add_low_entity_result Entity = AddGroundedEntity(WorldMode, EntityType_Familiar, P,
		WorldMode->FamiliarCollision);
	AddFlags(&Entity.Low->Sim, EntityFlag_Collides|EntityFlag_Moveable);

	return(Entity);
}

internal void
DrawHitpoints(sim_entity *Entity, render_group *RenderGroup, object_transform Transform)
{
	if (Entity->HitPointMax >= 1)
	{
		v2 HealthDim = {0.2f, 0.2f};
		real32 SpacingX = 1.5*HealthDim.x;
		v2 HitP = {-0.5f*(Entity->HitPointMax - 1)*SpacingX, -0.25f};
		v2 dHitP = {SpacingX, 0.0f};
		for (uint32 HealthIndex = 0;
			HealthIndex < Entity->HitPointMax;
			++HealthIndex)
		{
			hit_point *HitPoint = Entity->HitPoint + HealthIndex;
			v4 Color = {1.0f, 0.0f, 0.0f, 1.0f};
			if (HitPoint->FilledAmount == 0)
			{
				Color = V4(0.2f, 0.2f, 0.2f, 1.0f);
			}

			PushRect(RenderGroup, Transform, V3(HitP, 0), HealthDim, Color);
			HitP += dHitP;
		}
	}
}

internal void
ClearCollisionRulesFor(game_mode_world *WorldMode, uint32 StorageIndex)
{
	// TODO: Need to make a better data sturcture that allows
	// removal of collision rules without searching the entire table
	// NOTE: One way to make removal easy would be to always
	// add _both_ orders of the ppaiirs of storage indices to the
	// hash table, so no metter which position the entity is in,
	// you can always find it. Then, when you do your first pass
	// through for removal, you just remember the original top
	// of the free list, and when you're done, do a pass throught all
	// the new things on the free list, and remove the reverse of 
	// those pairs 
	for (uint32 HashBucket = 0;
		HashBucket < ArrayCount(WorldMode->CollisionRuleHash);
		++HashBucket)
	{
		for (pairwise_collision_rule **Rule = &WorldMode->CollisionRuleHash[HashBucket];
			*Rule;
			)
		{
			if (((*Rule)->StorageIndexA == StorageIndex) ||
				((*Rule)->StorageIndexB == StorageIndex))
			{
				pairwise_collision_rule *RemovedRule = *Rule;
				*Rule = (*Rule)->NextInHash;
				
				RemovedRule->NextInHash = WorldMode->FirstFreeCollisionRule;
				WorldMode->FirstFreeCollisionRule = RemovedRule;
			}
			else
			{
				Rule = &(*Rule)->NextInHash;
			}
		}
	}
}

internal void
AddCollisionRule(game_mode_world *WorldMode, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide)
{
	// Collapse this with ShouldCollide
	if (StorageIndexA > StorageIndexB)
	{
		uint32 Temp = StorageIndexA;
		StorageIndexA = StorageIndexB;
		StorageIndexB = Temp;
	}

	// TODO: BETTER HASH FUNCTION
	pairwise_collision_rule *Found = 0;
	uint32 HashBucket = StorageIndexA & (ArrayCount(WorldMode->CollisionRuleHash) - 1);
	for (pairwise_collision_rule *Rule = WorldMode->CollisionRuleHash[HashBucket];
		Rule;
		Rule = Rule->NextInHash)
	{
		if ((Rule->StorageIndexA == StorageIndexA) &&
			(Rule->StorageIndexB == StorageIndexB))
		{
			Found = Rule;
			break;
		}
	}

	if (!Found)
	{
		Found = WorldMode->FirstFreeCollisionRule;
		if (Found)
		{
			WorldMode->FirstFreeCollisionRule = Found->NextInHash;
		}
		else
		{
			Found = PushStruct(&WorldMode->World->Arena, pairwise_collision_rule);
		}

		Found->NextInHash = WorldMode->CollisionRuleHash[HashBucket];
		WorldMode->CollisionRuleHash[HashBucket] = Found;
	}

	if (Found)
	{
		Found->StorageIndexA = StorageIndexA;
		Found->StorageIndexB = StorageIndexB;
		Found->CanCollide = CanCollide;
	}
}

sim_entity_collision_volume_group *
MakeSimpleGroundedCollision(game_mode_world *WorldMode, real32 DimX, real32 DimY, real32 DimZ)
{
	// TODO: NOT WORLD ARENA! Change tp using the fundamental types arena, etc.
	sim_entity_collision_volume_group *Group = PushStruct(&WorldMode->World->Arena, sim_entity_collision_volume_group);
	Group->VolumeCount = 1;
	Group->Volumes = PushArray(&WorldMode->World->Arena, Group->VolumeCount, sim_entity_collision_volume);
	Group->TotalVolume.OffsetP = V3(0, 0, 0.5f*DimZ);
	Group->TotalVolume.Dim = V3(DimX, DimY, DimZ);
	Group->Volumes[0] = Group->TotalVolume;

	return(Group);
}

sim_entity_collision_volume_group *
MakeNullCollision(game_mode_world *WorldMode)
{
	// TODO: NOT WORLD ARENA! Change tp using the fundamental types arena, etc.
	sim_entity_collision_volume_group *Group = PushStruct(&WorldMode->World->Arena, sim_entity_collision_volume_group);
	Group->VolumeCount = 0;
	Group->Volumes = 0;
	Group->TotalVolume.OffsetP = V3(0.0f, 0.0f, 0.0f);
	// TODO: Should this be negative?
	Group->TotalVolume.Dim = V3(0.0f, 0.0f, 0.0f);

	return(Group);
}

// TODO: IMPORTANT: fill_ground_chunk_work
// will cause a crash if the mode goes away before the
// task finishes, this must be sut down properly
// if we ship it.
struct fill_ground_chunk_work
{
	transient_state *TranState;
	game_mode_world *WorldMode;
	ground_buffer *GroundBuffer;
	world_position ChunkP;

	task_with_memory *Task;
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(FillGroundChunkWork)
{
	TIMED_FUNCTION();
	
	fill_ground_chunk_work *Work = (fill_ground_chunk_work *)Data;
	
#if 0

	loaded_bitmap *Buffer = &Work->GroundBuffer->Bitmap;
	Buffer->AlignPercentage = V2(0.5f, 0.5f);
	Buffer->WidthOverHeight = 1.0f;
	
	real32 Width = Work->WorldMode->World->ChunkDimInMeters.x;
	real32 Height = Work->WorldMode->World->ChunkDimInMeters.y;
	v2 HalfDim = 0.5f*V2(Width, Height);

	// TODO: Decide what our pushbuffer size is
	render_group RenderGroup = BeginRenderGroup(Work->TranState->Assets, ChunkGeneration, true);
	BeginRender(RenderGroup);
	Orthographic(RenderGroup, Buffer->Width, Buffer->Height, (Buffer->Width - 2) / Width);
	Clear(RenderGroup, V4(1.0f, 0.0f, 1.0f, 1.0f));

	object_transform NoTransform = DefaultFlatTransform();

	for (int32 ChunkOffsetY =  -1;
		ChunkOffsetY <= 1;
		++ChunkOffsetY)
	{
		for (int32 ChunkOffsetX = -1;
			ChunkOffsetX <= 1;
			++ChunkOffsetX)
		{
			int32 ChunkX = Work->ChunkP.ChunkX + ChunkOffsetX;
			int32 ChunkY = Work->ChunkP.ChunkY + ChunkOffsetY;
			int32 ChunkZ = Work->ChunkP.ChunkZ;

			// TODO: MakeRandom number generation more systemic
			// TODO: Look into wang hashing or some other spatial seed generation "thing"
			random_series Series = RandomSeed(139*ChunkX + 593*ChunkX + 329*ChunkZ);
			v4 Color;
			DEBUG_IF(GroundChunks_Chekerboards)
			{
				Color = V4(1, 0, 0, 1);
				if ((ChunkX % 2) == (ChunkY % 2))
				{
					Color = V4(0, 0, 1, 1);
				}
			}
			else
			{
				Color = V4(1, 1, 1, 1);
			}

			v2 Center = V2(ChunkOffsetX*Width, ChunkOffsetY*Height);

			for (uint32 GrassIndex = 0;
				GrassIndex < 10;
				++GrassIndex)
			{
				bitmap_id Stamp = GetRandomBitmapFrom(
						Work->TranState->Assets,
						RandomChoice(&Series, 2) ? Asset_Grass : Asset_Stone,
						&Series);

				v2 P = Center + Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));
				PushBitmap(RenderGroup, NoTransform, Stamp, 2.0f, V3(P, 0.0f), Color);
			}
		}
	}

	for (int32 ChunkOffsetY = -1;
		ChunkOffsetY <= 1;
		++ChunkOffsetY)
	{
		for (int32 ChunkOffsetX = -1;
			ChunkOffsetX <= 1;
			++ChunkOffsetX)
		{
			int32 ChunkX = Work->ChunkP.ChunkX + ChunkOffsetX;
			int32 ChunkY = Work->ChunkP.ChunkY + ChunkOffsetY;
			int32 ChunkZ = Work->ChunkP.ChunkZ;

			// TODO: MakeRandom number generation more systemic
			// TODO: Look into wang hashing or some other spatial seed generation "thing"
			random_series Series = RandomSeed(139*ChunkX + 593*ChunkX + 329*ChunkZ);

			v2 Center = V2(ChunkOffsetX*Width, ChunkOffsetY*Height);

			for (uint32 GrassIndex = 0;
				GrassIndex < 10;
				++GrassIndex)
			{
				bitmap_id Stamp = GetRandomBitmapFrom(Work->TranState->Assets, Asset_Tuft, &Series);
				v2 P = Center + Hadamard(HalfDim, V2(RandomBilateral(&Series), RandomBilateral(&Series)));
				PushBitmap(RenderGroup, NoTransform, Stamp, 0.1f, V3(P, 0.0f));
			}
		}
	}

	Assert(AllResourcesPresent(RenderGroup));

	RenderGroupToOutput(RenderGroup, Buffer, &Work->Task->Arena);
	EndRender(RenderGroup);
#endif

	EndTaskWidthMemory(Work->Task);
}

internal void
FillGroundChunk(transient_state *TranState, game_mode_world *WorldMode, ground_buffer *GroundBuffer, world_position *ChunkP)
{
	task_with_memory *Task = BeginTaskWidthMemory(TranState, true);
	if (Task)
	{
		fill_ground_chunk_work *Work = PushStruct(&Task->Arena, fill_ground_chunk_work);
		Work->Task = Task;
		Work->TranState = TranState;
		Work->WorldMode = WorldMode;
		Work->GroundBuffer = GroundBuffer;
		Work->ChunkP = *ChunkP;
		GroundBuffer->P = *ChunkP;
		Platform.AddEntry(TranState->LowPriorityQueue, FillGroundChunkWork, Work);
	}
}

internal void
PlayWorld(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_World);

    game_mode_world *WorldMode = PushStruct(&GameState->ModeArena, game_mode_world);

	uint32 TilesPerWidth = 17;
	uint32 TilesPerHeight = 9;
	
	WorldMode->EffectsEntropy = RandomSeed(1234);
	WorldMode->TypicalFloorHeight = 3.0f;

	// TODO: Remove this!
	real32 PixelsToMeters = 1.0f / 42.0f;
	v3 WorldChunkDimInMeters = 
		{PixelsToMeters*(real32)GroundBufferWidth,
		PixelsToMeters*(real32)GroundBufferHeight,
		WorldMode->TypicalFloorHeight};
	
	WorldMode->World = CreateWorld(WorldChunkDimInMeters, &GameState->ModeArena);
	
	// NOTE: Reserve entity slot 0 for the null entity
	AddLowEntity(WorldMode, EntityType_Null, NullPosition());
	
	real32 TileSideInMeters = 1.4f;
	real32 TileDepthInMeters = WorldMode->TypicalFloorHeight;

	WorldMode->NullCollision = MakeNullCollision(WorldMode);
	WorldMode->SwordCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.1f);
	WorldMode->StairCollision = MakeSimpleGroundedCollision(WorldMode,
		TileSideInMeters, 2.0f*TileSideInMeters,
		1.1f*TileDepthInMeters);
	WorldMode->PlayerCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 1.2f);
	WorldMode->MonsterCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.5f);
	WorldMode->FamiliarCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.5f);
	WorldMode->WallCollision = MakeSimpleGroundedCollision(WorldMode,
		TileSideInMeters, TileSideInMeters,
		TileDepthInMeters);

	WorldMode->StandartRoomCollision = MakeSimpleGroundedCollision(WorldMode,
		TilesPerWidth*TileSideInMeters,
		TilesPerHeight*TileSideInMeters,
		0.9f*TileDepthInMeters);

	random_series Series = RandomSeed(0);

	uint32 ScreenBaseX = 0;
	uint32 ScreenBaseY = 0;
	uint32 ScreenBaseZ = 0;
	uint32 ScreenX = ScreenBaseX;
	uint32 ScreenY = ScreenBaseY;
	uint32 AbsTileZ = ScreenBaseZ;

	// TODO: Replace with real world generation
	bool32 DoorLeft = false;
	bool32 DoorRight = false;
	bool32 DoorTop = false;
	bool32 DoorBottom = false;
	bool32 DoorUp = false;
	bool32 DoorDown = false;
	for (uint32 ScreenIndex = 0;
		ScreenIndex < 20;
		++ScreenIndex)
	{
#if 1
		uint32 DoorDirection = RandomChoice(&Series, (DoorUp || DoorDown) ? 2 : 3);
#else	
		uint32 DoorDirection = RandomChoice(&Series, 2);
#endif
		//DoorDirection = 3;

		bool32 CreatedZDoor = false;
		if (DoorDirection == 3)
		{
			CreatedZDoor = true;
			DoorDown = true;
		}
		else if (DoorDirection == 2)
		{
			CreatedZDoor = true;
			DoorUp = true;
		}
		else if (DoorDirection == 1)
		{
			DoorRight = true;
		}
		else
		{
			DoorTop = true;
		}

		AddStandardRoom(WorldMode,
			ScreenX*TilesPerWidth + TilesPerWidth/2,
			ScreenY*TilesPerHeight + TilesPerHeight/2,
			AbsTileZ);

		for (uint32 TileY = 0;
			TileY < TilesPerHeight;
			++TileY)
		{
			for (uint32 TileX = 0;
				TileX < TilesPerWidth;
				++TileX)
			{
				uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
				uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

				uint32 ShouldBeDoor = false;
				if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
				{
					ShouldBeDoor = true;
				}

				if ((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2))))
				{
					ShouldBeDoor = true;
				}

				if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth/2))))
				{
					ShouldBeDoor = true;
				}

				if ((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth/2))))
				{
					ShouldBeDoor = true;
				}

				if (ShouldBeDoor)
				{
					AddWall(WorldMode, AbsTileX, AbsTileY, AbsTileZ);
				}
				else if (CreatedZDoor)
				{
					if (((AbsTileZ % 2) && (TileX == 10) && (TileY == 5)) || 
						(!(AbsTileZ % 2) && (TileX == 4) && (TileY == 5)))
					{
						AddStair(WorldMode, AbsTileX, AbsTileY, DoorDown ? AbsTileZ - 1 : AbsTileZ);
					}
				}
			}
		}

		DoorLeft = DoorRight;
		DoorBottom = DoorTop;

		if (CreatedZDoor)
		{
			DoorDown = !DoorDown;
			DoorUp = !DoorUp;
		}
		else
		{
			DoorUp = false;
			DoorDown = false;
		}

		DoorRight = false;
		DoorTop = false;

		if (DoorDirection == 3)
		{
			AbsTileZ -= 1;
		}
		else if (DoorDirection == 2)
		{
			AbsTileZ += 1;
		}
		else if (DoorDirection == 1)
		{
			ScreenX += 1; 
		}
		else
		{
			ScreenY += 1;
		}
	}

	world_position NewCameraP = {};
	uint32 CameraTileX = ScreenBaseX*TilesPerWidth + 17/2;
	uint32 CameraTileY = ScreenBaseY*TilesPerHeight + 9/2;
	uint32 CameraTileZ = ScreenBaseZ;
	NewCameraP = ChunkPositionFromTilePosition(WorldMode->World,
		CameraTileX, CameraTileY, CameraTileZ);

	WorldMode->CameraP = NewCameraP;

	AddMonster(WorldMode, CameraTileX - 3, CameraTileY + 2, CameraTileZ);

	for (u32 FamiliarIndex = 0;
		FamiliarIndex < 1;
		++FamiliarIndex)
	{
		int32 FamiliarOffsetX = RandomBetween(&Series, -7, 7);
		int32 FamiliarOffsetY = RandomBetween(&Series, -3, -1);
		if ((FamiliarOffsetX != 0) || (FamiliarOffsetY != 0))
		{
			AddFamiliar(WorldMode, CameraTileX + FamiliarOffsetX, CameraTileY + FamiliarOffsetY, CameraTileZ);
		}
	}
}

internal b32
UpdateAndRenderWorld(game_state *GameState, game_mode_world *WorldMode, transient_state *TranState,
	game_input *Input, render_group *RenderGroup, loaded_bitmap *DrawBuffer)
{
	b32 Result = false;

	world *World = WorldMode->World;
	
	v2 MouseP = {Input->MouseX, Input->MouseY};

	real32 WidthOfMonitor = 0.635; // NOTE: Horizontal measurement of monitor in meters
	real32 MetersToPixels = (real32)DrawBuffer->Width*WidthOfMonitor;
	
	real32 FocalLength = 0.6;
	real32 DistanceAboveGround = 9.0f;
	Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, DistanceAboveGround);

	Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));

	v2 ScreenCenter = {0.5f*(real32)DrawBuffer->Width, 0.5f*(real32)DrawBuffer->Height};

	rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
	rectangle3 CameraBoundsInMeters = RectMinMax(V3(ScreenBounds.Min, 0.0f), V3(ScreenBounds.Max, 0.0f));
	CameraBoundsInMeters.Min.z = -3.0f * WorldMode->TypicalFloorHeight;
	CameraBoundsInMeters.Max.z = 1.0f * WorldMode->TypicalFloorHeight;

	r32 FadeTopEndZ = 0.75f*WorldMode->TypicalFloorHeight;
	r32 FadeTopStartZ = 0.5f*WorldMode->TypicalFloorHeight;
	r32 FadeBottomStartZ = -2.0f*WorldMode->TypicalFloorHeight;
	r32 FadeBottomEndZ = -2.25*WorldMode->TypicalFloorHeight;

	// NOTE: Ground chunk rendering
	if (Global_GroundChunksOn)
	{
		for (uint32 GroundBufferIndex = 0;
			GroundBufferIndex < TranState->GroundBufferCount;
			++GroundBufferIndex)
		{
			ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
			
			if (IsValid(GroundBuffer->P))
			{
				loaded_bitmap *Bitmap = &GroundBuffer->Bitmap;
				v3 Delta = Subtract(WorldMode->World, &GroundBuffer->P, &WorldMode->CameraP);

				RenderGroup->GlobalAlpha = 1.0f;
				if (Delta.z > FadeTopStartZ)
				{
					RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeTopEndZ, Delta.z, FadeTopStartZ);
				}
				else if (Delta.z < FadeBottomStartZ)
				{
					RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeBottomEndZ, Delta.z, FadeBottomStartZ);
				}

				object_transform Transform = DefaultFlatTransform();
				Transform.OffsetP = Delta;

				real32 GroundSideInMeters = World->ChunkDimInMeters.x;
				PushBitmap(RenderGroup, Transform, Bitmap, GroundSideInMeters, V3(0, 0, 0));
				if (Global_GroundChunks_Outline)
				{
					PushRectOutline(RenderGroup, Transform, Delta, V2(GroundSideInMeters, GroundSideInMeters), V4(1.0f, 1.0f, 0.0f, 1.0f));
				}
			}	
		}

		RenderGroup->GlobalAlpha = 1.0f;

		// NOTE: Ground chunk updating
		world_position MinChunkP = MapIntoChunkSpace(World, WorldMode->CameraP, GetMinCorner(CameraBoundsInMeters));
		world_position MaxChunkP = MapIntoChunkSpace(World, WorldMode->CameraP, GetMaxCorner(CameraBoundsInMeters));
		
		for (int32 ChunkZ = MinChunkP.ChunkZ;
			ChunkZ <= MaxChunkP.ChunkZ;
			++ChunkZ)
		{
			for (int32 ChunkY = MinChunkP.ChunkY;
				ChunkY <= MaxChunkP.ChunkY;
				++ChunkY)
			{
				for (int32 ChunkX = MinChunkP.ChunkX;
					ChunkX <= MaxChunkP.ChunkX;
					++ChunkX)
				{
					//world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, ChunkZ);
					//if (Chunk)
					{
						world_position ChunkCenterP = CenteredChunkPoint(ChunkX, ChunkY, ChunkZ);
						v3 RelP = Subtract(World, &ChunkCenterP, &WorldMode->CameraP);

						// TODO: This is super indefficient fix it!
						real32 FurthestBufferLengthSq = 0.0f;
						ground_buffer *FurthestBuffer = 0;
						for (uint32 GroundBufferIndex = 0;
							GroundBufferIndex < TranState->GroundBufferCount;
							++GroundBufferIndex)
						{
							ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
							if (AreInSameChunk(World, &GroundBuffer->P, &ChunkCenterP))
							{
								FurthestBuffer = 0;
								break;
							}
							else if (IsValid(GroundBuffer->P))
							{
								v3 RelP2 = Subtract(World, &GroundBuffer->P, &WorldMode->CameraP);
								real32 BufferLengthSq = LengthSq(RelP2.xy);
								if (FurthestBufferLengthSq < BufferLengthSq)
								{
									FurthestBufferLengthSq = BufferLengthSq;
									FurthestBuffer = GroundBuffer;
								}
							}
							else
							{
								FurthestBufferLengthSq = Real32Maximum;
								FurthestBuffer = GroundBuffer;
							}
						}	
					
						if (FurthestBuffer)
						{
							FillGroundChunk(TranState, WorldMode, FurthestBuffer, &ChunkCenterP);
						}
					}
				}
			}
		}
	}

	b32 HeroesExist = false;
	b32 QuitRequested = false;
	for (int ControllerIndex = 0;
		ControllerIndex < ArrayCount(Input->Controllers);
		++ControllerIndex)
	{
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
		if (ConHero->EntityIndex == 0)
		{
			if (WasPressed(Controller->Back))
			{
				QuitRequested = true;
			}
			if (WasPressed(Controller->Start))
			{
				*ConHero = {};
				ConHero->EntityIndex = AddPlayer(WorldMode).LowIndex;
			}
		}

		if (ConHero->EntityIndex)
		{	
			HeroesExist = true;
			ConHero->dZ = 0.0f;
			ConHero->ddP = {};
			ConHero->dSword = {};

			if (Controller->IsAnalog)
			{
				ConHero->ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
			}
			else
			{
				//NOTE: Use digital movement tuning

				if (Controller->MoveUp.EndedDown)
				{
					ConHero->ddP.y = 1.0f;
				}
				if (Controller->MoveDown.EndedDown)
				{
					ConHero->ddP.y = -1.0f;
				}
				if (Controller->MoveLeft.EndedDown)
				{
					ConHero->ddP.x = -1.0f;
				}
				if (Controller->MoveRight.EndedDown)
				{
					ConHero->ddP.x = 1.0f;
				}
			}

			if (Controller->Start.EndedDown)
			{
				ConHero->dZ += 3.0f;
			}

			ConHero->dSword = {};
			if (Controller->ActionUp.EndedDown)
			{
				ChangeVolume(&GameState->AudioState, GameState->Music, 10.0f, V2(1.0f, 1.0f));
				ConHero->dSword = V2(0.0f, 1.0f);
			}
			if (Controller->ActionDown.EndedDown)
			{
				ChangeVolume(&GameState->AudioState, GameState->Music, 10.0f, V2(0.0f, 0.0f));
				ConHero->dSword = V2(0.0f, -1.0f);
			}
			if (Controller->ActionLeft.EndedDown)
			{
				ChangeVolume(&GameState->AudioState, GameState->Music, 5.0f, V2(0.0f, 0.0f));
				ConHero->dSword = V2(-1.0f, 0.0f);
			}
			if (Controller->ActionRight.EndedDown)
			{
				ChangeVolume(&GameState->AudioState, GameState->Music, 5.0f, V2(0.0f, 0.0f));
				ConHero->dSword = V2(1.0f, 0.0f);
			}

			if (WasPressed(Controller->Back))
			{
				DeleteLowEntity(WorldMode, ConHero->EntityIndex);
				ConHero->EntityIndex = 0;
			}
		}
	}

	// TODO: How big do we actually want to expand here?
	// TODO: Do we want to simulate upper floors, ets.?
	v3 SimBoundsExpansion = {15.0f, 15.0f, 0.0f};
	rectangle3 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
	temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
	world_position SimCenterP = WorldMode->CameraP;
	sim_region *SimRegion = BeginSim(&TranState->TranArena, WorldMode, 
		WorldMode->World, WorldMode->CameraP, SimBounds, Input->dtForFrame);
	
	// NOTE: This is the camera position relative to the origin of this region
	v3 CameraP = Subtract(World, &WorldMode->CameraP, &SimCenterP);

	PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1.0f));
	//PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(CameraBoundsInMeters).xy, V4(1.0f, 1.0f, 1.0f, 1.0f));
	PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(SimBounds).xy, V4(0.0f, 1.0f, 1.0f, 1.0f));
	PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->Bounds).xy, V4(1.0f, 0.0f, 1.0f, 1.0f));

	// TODO: Move this out into handmade_entity.cpp
	for (uint32 EntityIndex = 1;
		EntityIndex < SimRegion->EntityCount;
		++EntityIndex)
	{
		sim_entity *Entity = SimRegion->Entities + EntityIndex;
		if (Entity->Updatable)
		{
			real32 dt = Input->dtForFrame;

			// TODO: This is incorrect, should be computed after update!!!
			real32 ShadowAlpha = 1.0f - 0.5f*Entity->P.z;
			if (ShadowAlpha < 0.0f)
			{
				ShadowAlpha = 0.0f;
			}
			
			move_spec MoveSpec = DefaultMoveSpec();
			v3 ddP = {};

			// TODO: Probably indicates we want to separate update and render
			// for entities sometime soom
			v3 CameraRelativeGroundP = GetEntityGroundPoint(Entity) - CameraP;

			RenderGroup->GlobalAlpha = 1.0f;
			if (CameraRelativeGroundP.z > FadeTopStartZ)
			{
				RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeTopEndZ, CameraRelativeGroundP.z, FadeTopStartZ);
			}
			else if (CameraRelativeGroundP.z < FadeBottomStartZ)
			{
				RenderGroup->GlobalAlpha = Clamp01MapToRange(FadeBottomEndZ, CameraRelativeGroundP.z, FadeBottomStartZ);
			}

			//
			//NOTE: Pre-physics entity work an entity
			//
			hero_bitmap_id HeroBitmaps = {};
			asset_vector MatchVector = {};
			MatchVector.E[Tag_FacingDirection] = Entity->FacingDirection;
			asset_vector WeightVector = {};
			WeightVector.E[Tag_FacingDirection] = 1.0f;
			HeroBitmaps.Head = GetBestMatchBitmapFrom(TranState->Assets, Asset_Head, &MatchVector, &WeightVector);
			HeroBitmaps.Cape = GetBestMatchBitmapFrom(TranState->Assets, Asset_Cape, &MatchVector, &WeightVector);
			HeroBitmaps.Torso = GetBestMatchBitmapFrom(TranState->Assets, Asset_Torso, &MatchVector, &WeightVector);
			switch (Entity->Type)
			{
				case EntityType_Hero:
				{
					// TODO: Now that we gave some real usage examples, let's solidify
					// the positioning system

					for (uint32 ControllIndex = 0;
						ControllIndex < ArrayCount(GameState->ControlledHeroes);
						++ControllIndex)
					{
						controlled_hero *ConHero = GameState->ControlledHeroes + ControllIndex;

						if (Entity->StorageIndex == ConHero->EntityIndex)
						{
							if (ConHero->dZ != 0.0f)
							{	
								Entity->dP.z = ConHero->dZ;
							}
							MoveSpec.UnitMaxAccelVector = true;
							MoveSpec.Speed = 50.0f;
							MoveSpec.Drag = 8.0f;
							ddP = V3(ConHero->ddP, 0);

							if ((ConHero->dSword.x != 0.0f) || (ConHero->dSword.y != 0.0f))
							{
								sim_entity *Sword = Entity->Sword.Ptr;
								if (Sword && IsSet(Sword, EntityFlag_Nonspatial))
								{
									Sword->DistanceLimit = 5.0f;
									MakeEntitySpatial(Sword, Entity->P, V3(5.0f*ConHero->dSword, 0));
									AddCollisionRule(WorldMode, Sword->StorageIndex, Entity->StorageIndex, false);

									//PlaySound(&WorldMode->AudioState, GetRandomSoundFrom(TranState->Assets, Asset_Bloop, &WorldMode->EffectsEntropy));
								}
							}
						}
					}
				} break;
				case EntityType_Sword:
				{
					MoveSpec.UnitMaxAccelVector = false;
					MoveSpec.Speed = 0.0f;
					MoveSpec.Drag = 0.0f;

					if (Entity->DistanceLimit == 0.0f)
					{
						ClearCollisionRulesFor(WorldMode, Entity->StorageIndex);
						MakeEntityNonSpatial(Entity);
					}
				} break;
				case EntityType_Familiar:
				{
					sim_entity *ClosestHero = 0;
					real32 ClosestHeroDSq = Square(10.0f); // NOTE: Ten meter maximum search
					if (Global_AI_Familiar_FollowsHero)
					{
						// TODO: Make spation queries easy for things
						sim_entity *TestEntity = SimRegion->Entities;
						for (uint32 TestEntityIndex = 0;
							TestEntityIndex < SimRegion->EntityCount;
							++TestEntityIndex, ++TestEntity)
						{	
							if (TestEntity->Type == EntityType_Hero)
							{
								real32 TestDSq = LengthSq(TestEntity->P - Entity->P);

								if (ClosestHeroDSq > TestDSq)
								{
									ClosestHero = TestEntity;
									ClosestHeroDSq = TestDSq;
								}
							}
						}
					}

					if (ClosestHero && (ClosestHeroDSq > Square(3.0f)))
					{
						real32 Acceleration = 0.5f;
						real32 OneOverLength = Acceleration / SquareRoot(ClosestHeroDSq);
						ddP = OneOverLength*(ClosestHero->P - Entity->P);
					}

					MoveSpec.UnitMaxAccelVector = true;
					MoveSpec.Speed = 50.0f;
					MoveSpec.Drag = 8.0f;
				} break;
			}

			if (!IsSet(Entity, EntityFlag_Nonspatial) &&
				IsSet(Entity, EntityFlag_Moveable))
			{
				MoveEntity(WorldMode, SimRegion, Entity, Input->dtForFrame, &MoveSpec, ddP);
			}

			object_transform EntityTrasform = DefaultUprightTransform();
			EntityTrasform.OffsetP = GetEntityGroundPoint(Entity);
			
			//
			//NOTE: Post-physics entity work
			//
			switch (Entity->Type)
			{
				case EntityType_Hero:
				{
					// TODO: Z!!!
					real32 HeroSizeC = 2.5f;
					PushBitmap(RenderGroup, EntityTrasform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), HeroSizeC*1.0f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));
					PushBitmap(RenderGroup, EntityTrasform, HeroBitmaps.Torso, HeroSizeC*1.2f, V3(0, 0, 0));
					PushBitmap(RenderGroup, EntityTrasform, HeroBitmaps.Cape, HeroSizeC*1.2f, V3(0, 0, 0));
					PushBitmap(RenderGroup, EntityTrasform, HeroBitmaps.Head, HeroSizeC*1.2f, V3(0, 0, 0));
					DrawHitpoints(Entity, RenderGroup, EntityTrasform);
					
					if (Global_Particles_Test)
					{
						for (u32 ParticleSpawnIndex = 0;
							ParticleSpawnIndex < 2;
							++ParticleSpawnIndex)
						{
							particle *Particle = WorldMode->Particles + WorldMode->NextParticle++;
							if (WorldMode->NextParticle >= ArrayCount(WorldMode->Particles))
							{
								WorldMode->NextParticle = 0;
							}

							Particle->P = V3(RandomBetween(&WorldMode->EffectsEntropy, -0.05f, 0.05f), 0.0f, 0.0f);
							Particle->dP = V3(RandomBetween(&WorldMode->EffectsEntropy, -0.01f, 0.01f),
								7.0f*RandomBetween(&WorldMode->EffectsEntropy, 0.7f, 1.0f), 0.0f);
							Particle->ddP = V3(0.0, -9.8f, 0.0f);
							Particle->Color = 
								V4(RandomBetween(&WorldMode->EffectsEntropy, 0.75f, 1.0f),
								RandomBetween(&WorldMode->EffectsEntropy, 0.75f, 1.0f),
								RandomBetween(&WorldMode->EffectsEntropy, 0.75f, 1.0f),
								1.0f);
							Particle->dColor = V4(0, 0, 0, -0.5f);

							asset_vector MatchVector = {};
							asset_vector WeightVector = {};
							char *Nothings = "NOTHINGS";
							MatchVector.E[Tag_UnicodeCodepoint] = (r32)Nothings[RandomChoice(&WorldMode->EffectsEntropy, ArrayCount(Nothings) - 1)];
							WeightVector.E[Tag_UnicodeCodepoint] = 1.0f;
							
							Particle->BitmapID = HeroBitmaps.Head;
							//Particle->BitmapID = GetRandomBitmapFrom(TranState->Assets, Asset_Font, &WorldMode->EffectsEntropy);
						}

						// NOTE: Particle system test
						ZeroStruct(WorldMode->ParticleCels);

						r32 GridScale = 0.5f;
						r32 InvGridScale = 1.0f / GridScale;
						v3 GridOrigin = {-0.5f*GridScale*PARTICLE_CEL_DIM, 0.0f, 0.0f};
						for (u32 ParticleIndex = 0;
							ParticleIndex < ArrayCount(WorldMode->Particles);
							++ParticleIndex)
						{
							particle *Particle = WorldMode->Particles + ParticleIndex;

							v3 P = InvGridScale*(Particle->P - GridOrigin);

							s32 X = TruncateReal32ToInt32(P.x);
							s32 Y = TruncateReal32ToInt32(P.y);

							if (X < 0) {X = 0;}
							if (X > (PARTICLE_CEL_DIM - 1)) {X = (PARTICLE_CEL_DIM - 1);}
							if (Y < 0) {Y = 0;}
							if (Y > (PARTICLE_CEL_DIM - 1)) {Y = (PARTICLE_CEL_DIM - 1);}

							particle_cel *Cel = &WorldMode->ParticleCels[Y][X];
							real32 Density = Particle->Color.a;
							Cel->Density += Density;
							Cel->VelocityTimesDensity += Density*Particle->dP;
						}

						if (Global_Paritcles_ShowGrid)
						{
							for (u32 Y = 0;
								Y < PARTICLE_CEL_DIM;
								++Y)
							{
								for (u32 X = 0;
									X < PARTICLE_CEL_DIM;
									++X)
								{
									particle_cel *Cel = &WorldMode->ParticleCels[Y][X];
									r32 Alpha = Clamp01(0.1f*Cel->Density);
									PushRect(RenderGroup, EntityTrasform, GridScale*V3((r32)X, (r32)Y, 0) + GridOrigin,
										GridScale*V2(1.0f, 1.0f), V4(Alpha, Alpha, Alpha, 1.0f));
								}
							}
						}

						for (u32 ParticleIndex = 0;
							ParticleIndex < ArrayCount(WorldMode->Particles);
							++ParticleIndex)
						{
							particle *Particle = WorldMode->Particles + ParticleIndex;

							v3 P = InvGridScale*(Particle->P - GridOrigin);

							s32 X = TruncateReal32ToInt32(P.x);
							s32 Y = TruncateReal32ToInt32(P.y);

							if (X < 1) {X = 1;}
							if (X > (PARTICLE_CEL_DIM - 2)) {X = (PARTICLE_CEL_DIM - 2);}
							if (Y < 1) {Y = 1;}
							if (Y > (PARTICLE_CEL_DIM - 2)) {Y = (PARTICLE_CEL_DIM - 2);}

							particle_cel *CelCenter = &WorldMode->ParticleCels[Y][X];
							particle_cel *CelLeft = &WorldMode->ParticleCels[Y][X - 1];
							particle_cel *CelRight = &WorldMode->ParticleCels[Y][X + 1];
							particle_cel *CelDown = &WorldMode->ParticleCels[Y - 1][X];
							particle_cel *CelUp = &WorldMode->ParticleCels[Y + 1][X];
							
							v3 Dispersion = {};
							r32 Dc = 1.0f;
							Dispersion += Dc*(CelCenter->Density - CelLeft->Density)*V3(-1.0f, 0.0f, 0.0f);
							Dispersion += Dc*(CelCenter->Density - CelRight->Density)*V3(1.0f, 0.0f, 0.0f);
							Dispersion += Dc*(CelCenter->Density - CelDown->Density)*V3(0.0f, -1.0f, 0.0f);
							Dispersion += Dc*(CelCenter->Density - CelUp->Density)*V3(0.0f, 1.0f, 0.0f);

							v3 ddP = Particle->ddP + Dispersion;
							
							// NOTE: Simulate the particle forward in time
							Particle->P += (0.5f*Square(Input->dtForFrame) * Input->dtForFrame*ddP + 
								Input->dtForFrame*Particle->dP);
							Particle->dP += Input->dtForFrame * ddP;
							Particle->Color += Input->dtForFrame * Particle->dColor;

							if (Particle->P.y < 0.0f)
							{
								r32 CoefficientOfRestitution = 0.3f;
								r32 CoefficientOfFriction = 0.7f;
								Particle->P.y = -Particle->P.y;
								Particle->dP.y = -CoefficientOfRestitution*Particle->dP.y;
								Particle->dP.x = CoefficientOfFriction*Particle->dP.x;
							}
							// TODO: Shouldn't we just clamp colors in the renderer???
							v4 Color;
							Color.r = Clamp01(Particle->Color.r);
							Color.g = Clamp01(Particle->Color.g);
							Color.b = Clamp01(Particle->Color.b);
							Color.a = Clamp01(Particle->Color.a);

							if (Color.a > 0.9)
							{
								Color.a = 0.9f * Clamp01MapToRange(1.0f, Color.a, 0.9);
							}

							// NOTE: Render the particle
							PushBitmap(RenderGroup, EntityTrasform, Particle->BitmapID, 0.2f, Particle->P, Color);
						}
					}
				} break;
				
				case EntityType_Wall:
				{
					PushBitmap(RenderGroup, EntityTrasform, GetFirstBitmapFrom(TranState->Assets, Asset_Tree), 2.5f, V3(0, 0, 0));
				} break;

				case EntityType_Stairwell:
				{
					PushRect(RenderGroup, EntityTrasform, V3(0, 0, 0), Entity->WalkableDim, V4(1.0f, 0.5f, 0, 1.0f));
					PushRect(RenderGroup, EntityTrasform, V3(0, 0, Entity->WalkableHeight), Entity->WalkableDim, V4(1.0f, 1.0f, 0, 1.0f));
				} break;
				case EntityType_Sword:
				{
					MoveSpec.UnitMaxAccelVector = false;
					MoveSpec.Speed = 0.0f;
					MoveSpec.Drag = 0.0f;

					if (Entity->DistanceLimit == 0.0f)
					{
						ClearCollisionRulesFor(WorldMode, Entity->StorageIndex);
						MakeEntityNonSpatial(Entity);
					}

					PushBitmap(RenderGroup, EntityTrasform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 0.5f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));
					PushBitmap(RenderGroup, EntityTrasform, GetFirstBitmapFrom(TranState->Assets, Asset_Sword),  0.5f, V3(0, 0, 0));
				} break;
				case EntityType_Familiar:
				{
					Entity->tBob += dt;
					if (Entity->tBob > Tau32)
					{
						Entity->tBob -= Tau32;
					}
					real32 BobSine = Sin(2.0f*Entity->tBob);
					PushBitmap(RenderGroup, EntityTrasform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 2.5f, V3(0, 0, 0), V4(1, 1, 1, (0.5f*ShadowAlpha) + 0.2f*BobSine));
					PushBitmap(RenderGroup, EntityTrasform, HeroBitmaps.Head, 2.5f, V3(0, 0, 0.2f*BobSine));
				} break;
				case EntityType_Monster:
				{
					PushBitmap(RenderGroup, EntityTrasform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 4.5f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));
					PushBitmap(RenderGroup, EntityTrasform, HeroBitmaps.Torso, 4.5f, V3(0, 0, 0));

					DrawHitpoints(Entity, RenderGroup, EntityTrasform);
				} break;
				case EntityType_Space:
				{
					if (Global_Simulation_UseSpaceOutlines)
					{
						for (uint32 VolumeIndex = 0;
							VolumeIndex < Entity->Collision->VolumeCount;
							++VolumeIndex)
						{
							sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;
							PushRectOutline(RenderGroup, EntityTrasform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z), Volume->Dim.xy, V4(0.0f, 0.5f, 1.0f, 1.0f));
						}
					}
				} break;
				default:
				{
					// InvalidCodePath;
				} break;
			}

			if (DEBUG_UI_ENABLED)
			{
				debug_id EntityDebugID = DEBUG_POINTER_ID(WorldMode->LowEntities + Entity->StorageIndex);

				for (uint32 VolumeIndex = 0;
					VolumeIndex < Entity->Collision->VolumeCount;
					++VolumeIndex)
				{
					sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;
					
					v3 LocalMouseP = Unproject(RenderGroup, EntityTrasform, MouseP);

					if ((LocalMouseP.x > -0.5f*Volume->Dim.x) && (LocalMouseP.x < 0.5f*Volume->Dim.x) &&
						(LocalMouseP.y > -0.5f*Volume->Dim.y) && (LocalMouseP.y < 0.5f*Volume->Dim.y))
					{
						DEBUG_HIT(EntityDebugID, LocalMouseP.z);
					}
					
					v4 OutlineColor;
					if (DEBUG_HIGHLIGHTED(EntityDebugID, &OutlineColor))
					{
						PushRectOutline(RenderGroup, EntityTrasform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z),
							Volume->Dim.xy, OutlineColor, 0.05f);
					}
				}

				if (DEBUG_REQUESTED(EntityDebugID))
				{
					DEBUG_DATA_BLOCK("Simulation/Entity");
					// TODO: Do we want this DEBUG_VALUE(EntityDebugID);
					DEBUG_VALUE(Entity->StorageIndex);
					DEBUG_VALUE(Entity->Updatable);
					DEBUG_VALUE(Entity->Type);
					DEBUG_VALUE(Entity->P);
					DEBUG_VALUE(Entity->dP);
					DEBUG_VALUE(Entity->DistanceLimit);
					DEBUG_VALUE(Entity->FacingDirection);
					DEBUG_VALUE(Entity->tBob);
					DEBUG_VALUE(Entity->dAbsTileZ);
					DEBUG_VALUE(Entity->HitPointMax);
					DEBUG_VALUE(HeroBitmaps.Torso);
#if 0
					DEBUG_BEGIN_ARRAY();
					for (u32 HitPointIndex = 0;
						HitPointIndex < Entity->HitPointMax;
						++HitPointIndex)
					{
						DEBUG_VALUE(Entity->HitPoint[HitPointIndex]);
					}
					DEBUG_END_ARRAY();
					DEBUG_VALUE(Entity->Sword);
#endif
					DEBUG_VALUE(Entity->WalkableDim);
					DEBUG_VALUE(Entity->WalkableHeight);
				}
			}
		}
	}

	RenderGroup->GlobalAlpha = 1.0f;

#if 0
	v2 Origin = ScreenCenter;
	
	real32 Angle = 0.1f*WorldMode->Time;
#if 1
	v2 Disp = {100.0f * Cos(5.0f*Angle), 100.0f * Sin(3.0f*Angle)};
#else
	v2 Disp = {};
#endif

#if 1
	v2 XAxis = 100.0f*V2(Cos(10.0f*Angle), Sin(10.0f*Angle));
	v2 YAxis = Perp(XAxis);
#else
	v2 XAxis = {100, 0};
	v2 YAxis = {0, 100};
#endif
	v4 Color = V4(1, 1, 1, 1);



	WorldMode->Time += Input->dtForFrame;
	v3 MapColor[] =
	{
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1}
	};

	for (uint32 MapIndex = 0;
		MapIndex < ArrayCount(TranState->EnvMaps);
		++MapIndex)
	{
		environment_map *Map = TranState->EnvMaps + MapIndex;
		loaded_bitmap *LOD = &Map->LOD[0];
		bool32 RowCheckerOn = false;
		int32 CheckerWidth = 16;
		int32 CheckerHeight = 16;
		for (int32 Y = 0;
			Y < LOD->Height;
			Y += CheckerHeight)
		{
			bool32 CheckerOn = RowCheckerOn;
			for (int32 X = 0;
				X < LOD->Width;
				X += CheckerWidth)
			{
				v4 Color = CheckerOn ? V4(MapColor[MapIndex], 1.0f) : V4(0, 0, 0, 1);
				v2 MinP = V2i(X, Y);
				v2 MaxP = MinP + V2i(CheckerWidth, CheckerHeight);
				DrawRectangle(LOD, MinP, MaxP, Color);
				CheckerOn = !CheckerOn;
			}
			RowCheckerOn = !RowCheckerOn;
		}
	}
	TranState->EnvMaps[0].Pz = -2.0f;
	TranState->EnvMaps[1].Pz = 0.0f;
	TranState->EnvMaps[2].Pz = 2.0f;
	

	CoordinateSystem(RenderGroup, Disp + Origin - 0.5f*XAxis - 0.5f*YAxis,
		XAxis, YAxis, Color,
		&WorldMode->TestDiffuse,
		&WorldMode->TestNormal,
		TranState->EnvMaps + 2,
		TranState->EnvMaps + 1,
		TranState->EnvMaps + 0);

	v2 MapP = {0.0f, 0.0f};
	for (uint32 MapIndex = 0;
		MapIndex < ArrayCount(TranState->EnvMaps);
		++MapIndex)
	{
		environment_map *Map = TranState->EnvMaps + MapIndex;
		loaded_bitmap *LOD = &Map->LOD[0];

		XAxis = 0.5f*V2((real32)LOD->Width, 0);
		YAxis = 0.5f*V2(0, (real32)LOD->Height);

		CoordinateSystem(RenderGroup, MapP, XAxis, YAxis, Color, LOD, 0, 0, 0, 0);
		MapP += YAxis + V2(0.0f, 6.0f);
	}
#endif

	Orthographic(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, 1.0f);

	PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(MouseP, 0.0f), V2(2.0f, 2.0f));

	// TODO: Make sure we hoist the camera update out to a place where the renderer
	// can know about the the location of the camera at the end of the frame os there isn't
	// a frane of lag in camera updating compared to the hero.
	EndSim(SimRegion, WorldMode);
	EndTemporaryMemory(SimMemory);

	if (!HeroesExist)
	{
		PlayTitleScreen(GameState, TranState);
	}

	return(Result);
}