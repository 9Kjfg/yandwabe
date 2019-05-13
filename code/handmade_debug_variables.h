#if !defined(HANDMADE_DEBUG_VARIABLES_H)

#define DEBUG_MAX_VARIABLE_STACK_DEPTH 64

#if 0
struct debug_variable_definition_context
{
	debug_state *State;
	memory_arena *Arena;

	u32 GroupDepth;
	debug_variable *GroupStack[DEBUG_MAX_VARIABLE_STACK_DEPTH];
};

internal debug_variable *
DEBUGAddVariable(debug_state *State, debug_variable_type Type, char *Name)
{
	debug_variable *Var = PushStruct(&State->DebugArena, debug_variable);
	Var->Type = Type;
	Var->Name = (char *)PushCopy(&State->DebugArena, StringLength(Name) + 1, Name);
	
	return(Var);
}

internal void
DEBUGAddVariableToGroup(debug_state *State, debug_variable *Group, debug_variable *Add)
{
	debug_variable_link *Link = PushStruct(&State->DebugArena, debug_variable_link);
	DLIST_INSERT(&Group->VarGroup, Link);
	Link->Var = Add;
}

internal void
DEBUGAddVariableToDefaultGroup(debug_variable_definition_context *Context, debug_variable *Var)
{
	debug_variable *Parent = Context->GroupStack[Context->GroupDepth];
	if (Parent)
	{
		DEBUGAddVariableToGroup(Context->State, Parent, Var);
	}
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, debug_variable_type Type, char *Name)
{
	debug_variable *Var = DEBUGAddVariable(Context->State, Type, Name);
	DEBUGAddVariableToDefaultGroup(Context, Var);
	
	return(Var);
}

internal debug_variable *
DEBUGAddRootGroup(debug_state *DebugState, char *Name)
{
	debug_variable *Group = DEBUGAddVariable(DebugState, DebugVariableType_VarGroup, Name);
	DLIST_INIT(&Group->VarGroup);
	
	return(Group);	
}

internal debug_variable *
DEBUGBeginVariableGroup(debug_variable_definition_context *Context, char *Name)
{
	debug_variable *Group = DEBUGAddRootGroup(Context->State, Name);
	DEBUGAddVariableToDefaultGroup(Context, Group);

	Assert(Context->GroupDepth < (ArrayCount(Context->GroupStack) - 1));
	Context->GroupStack[++Context->GroupDepth] = Group;

	return(Group);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, b32 Value)
{
	debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_Bool32, Name);
	Var->Bool32 = Value;

	return(Var);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, r32 Value)
{
	debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_Real32, Name);
	Var->Real32 = Value;

	return(Var);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, v4 Value)
{
	debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_V4, Name);
	Var->Vector4 = Value;

	return(Var);
}

internal debug_variable *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, bitmap_id Value)
{
	debug_variable *Var = DEBUGAddVariable(Context, DebugVariableType_BitmapDisplay, Name);
	Var->BitmapDisplay.ID = Value;

	return(Var);
}

internal void
DEBUGEndVariableGroup(debug_variable_definition_context *Context)
{
	Assert(Context->GroupDepth > 0);
	--Context->GroupDepth;
}

internal void
DEBUGCreateVariables(debug_variable_definition_context *Context)
{
// TODO: Parameterize the fountain?

#define DEBUG_VARIABLE_LISTING(Name) DEBUGAddVariable(Context, #Name, DEBUGUI_##Name)

	DEBUGBeginVariableGroup(Context, "Group Chunks");
	DEBUG_VARIABLE_LISTING(DrawEntityOutlines);
	DEBUGEndVariableGroup(Context);

	DEBUGBeginVariableGroup(Context, "Group Chunks");
	DEBUG_VARIABLE_LISTING(GroundChunkOutline);
	DEBUG_VARIABLE_LISTING(GroundCheckChekerboards);
	DEBUG_VARIABLE_LISTING(RecomputeGroundChunksOnEXEChange);
	DEBUGEndVariableGroup(Context);

	DEBUGBeginVariableGroup(Context, "Particles");
	DEBUG_VARIABLE_LISTING(ParticleTest);
	DEBUG_VARIABLE_LISTING(ParitcleGrid);
	DEBUGEndVariableGroup(Context);

	DEBUGBeginVariableGroup(Context, "Renderer");
	{
		DEBUG_VARIABLE_LISTING(TestWeirdDrawBufferSize);
		DEBUG_VARIABLE_LISTING(ShowLightingSamples);
	
		DEBUGBeginVariableGroup(Context, "Camera");
		{
			DEBUG_VARIABLE_LISTING(UsedDebugCamera);
			DEBUG_VARIABLE_LISTING(DebugCameraDistance);
			DEBUG_VARIABLE_LISTING(UseRoomBasedCamera);
		}
		DEBUGEndVariableGroup(Context);
	
		DEBUGEndVariableGroup(Context);
	}

	DEBUG_VARIABLE_LISTING(UseSpacesOutline);
	DEBUG_VARIABLE_LISTING(FamiliarFollowsHero);
	
#undef DEBUG_VARIABLE_LISTING
}
#endif

#define HANDMADE_DEBUG_VARIABLES_H
#endif