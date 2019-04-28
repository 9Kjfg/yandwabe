#if !defined(HANDMADE_DEBUG_VARIABLES_H)


struct debug_variable_definition_context
{
	debug_state *State;
	memory_arena *Arena;

	debug_variable_reference *Group;
};

internal debug_variable *
DEBUGAddUnreferenceVariable(debug_state *State, debug_variable_type Type, char *Name)
{
	debug_variable *Var = PushStruct(&State->DebugArena, debug_variable);
	Var->Type = Type;
	Var->Name = (char *)PushCopy(&State->DebugArena, StringLength(Name) + 1, Name);
	
	return(Var);
}

internal debug_variable_reference *
DEBUGAddVariableReference(debug_state *State, debug_variable_reference *GroupRef, debug_variable *Var)
{
	debug_variable_reference *Ref = PushStruct(&State->DebugArena, debug_variable_reference);
	Ref->Var = Var;
	Ref->Next = 0;

	Ref->Parent = GroupRef;
	debug_variable *Group = (Ref->Parent) ? Ref->Parent->Var : 0;
	if (Group)
	{
		if (Group->Group.LastChild)
		{
			Group->Group.LastChild = Group->Group.LastChild->Next = Ref;
		}
		else
		{
			Group->Group.LastChild = Group->Group.FirstChild = Ref;
		}
	}

	return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariableReference(debug_variable_definition_context *Context, debug_variable *Var)
{
	debug_variable_reference *Ref = DEBUGAddVariableReference(Context->State, Context->Group, Var);

	return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, debug_variable_type Type, char *Name)
{
	debug_variable *Var = DEBUGAddUnreferenceVariable(Context->State, Type, Name);
	debug_variable_reference *Ref = DEBUGAddVariableReference(Context, Var);

	return(Ref);
}

internal debug_variable *
DEBUGAddRootGroupInternal(debug_state *State, char *Name)
{
	debug_variable *Group = DEBUGAddUnreferenceVariable(State, DebugVariableType_Group, Name);
	Group->Group.Expanded = true;
	Group->Group.FirstChild = Group->Group.LastChild = 0;

	return(Group);
}

internal debug_variable_reference *
DEBUGAddRootGroup(debug_state *State, char *Name)
{
	debug_variable_reference *GroupRef = 
		DEBUGAddVariableReference(State, 0, DEBUGAddRootGroupInternal(State, Name));

	return(GroupRef);
}

internal debug_variable_reference *
DEBUGBeginVariableGroup(debug_variable_definition_context *Context, char *Name)
{
	debug_variable_reference *Group =
		DEBUGAddVariableReference(Context, DEBUGAddRootGroupInternal(Context->State, Name));
	Group->Var->Group.Expanded = false;
	
	Context->Group = Group;

	return(Group);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, b32 Value)
{
	debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_Bool32, Name);
	Ref->Var->Bool32 = Value;

	return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, r32 Value)
{
	debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_Real32, Name);
	Ref->Var->Real32 = Value;

	return(Ref);
}

internal debug_variable_reference *
DEBUGAddVariable(debug_variable_definition_context *Context, char *Name, v4 Value)
{
	debug_variable_reference *Ref = DEBUGAddVariable(Context, DebugVariableType_V4, Name);
	Ref->Var->Vector4 = Value;

	return(Ref);
}


internal void
DEBUGEndVariableGroup(debug_variable_definition_context *Context)
{
	Assert(Context->Group);

	Context->Group = Context->Group->Parent;
}

internal void
DEBUGCreateVariables(debug_variable_definition_context *Context)
{
// TODO: Parameterize the fountain?

#define DEBUG_VARIABLE_LESTING(Name) DEBUGAddVariable(Context, #Name, DEBUGUI_##Name)

	DEBUGBeginVariableGroup(Context, "Group Chunks");
	DEBUG_VARIABLE_LESTING(GroundChunkOutline);
	DEBUG_VARIABLE_LESTING(GroundCheckChekerboards);
	DEBUG_VARIABLE_LESTING(RecomputeGroundChunksOnEXEChange);
	DEBUGEndVariableGroup(Context);

	DEBUGBeginVariableGroup(Context, "Particles");
	DEBUG_VARIABLE_LESTING(ParticleTest);
	DEBUG_VARIABLE_LESTING(ParitcleGrid);
	DEBUGEndVariableGroup(Context);

	DEBUGBeginVariableGroup(Context, "Renderer");
	{
		DEBUG_VARIABLE_LESTING(TestWeirdDrawBufferSize);
		DEBUG_VARIABLE_LESTING(ShowLightingSamples);
	
		DEBUGBeginVariableGroup(Context, "Camera");
		{
			DEBUG_VARIABLE_LESTING(UsedDebugCamera);
			DEBUG_VARIABLE_LESTING(DebugCameraDistance);
			DEBUG_VARIABLE_LESTING(UseRoomBasedCamera);
		}
		DEBUGEndVariableGroup(Context);
	
		DEBUGEndVariableGroup(Context);
	}

	DEBUG_VARIABLE_LESTING(UseSpacesOutline);
	DEBUG_VARIABLE_LESTING(FamiliarFollowsHero);

#undef DEBUG_VARIABLE_LESTING
}

#define HANDMADE_DEBUG_VARIABLES_H
#endif