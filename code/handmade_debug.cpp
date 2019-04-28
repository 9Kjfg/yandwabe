// TODO: Stop using stdio
#include <stdio.h>
#include "handmade_debug_variables.h"

internal void RestartCollation(debug_state *DebugState, u32 InvalidEventArrayIndex);

inline debug_state *
DEBUGGetState(game_memory *Memory)
{
	debug_state *DebugState = (debug_state *)Memory->DebugStorage;
	Assert(DebugState->Initialized);
	return(DebugState);
}

inline debug_state *
DEBUGGetState(void)
{
	debug_state *Result = DEBUGGetState(DebugGlobalMemory);
	return(Result);
}

internal debug_variable_hierarchy *
AddHierarchy(debug_state *DebugState, debug_variable_reference *Group, v2 AtP)
{
	debug_variable_hierarchy *Hierarchy = PushStruct(&DebugState->DebugArena, debug_variable_hierarchy);
	Hierarchy->UIP = AtP;
	Hierarchy->Group = Group;
	Hierarchy->Next = DebugState->HierarchySentinel.Next;
	Hierarchy->Prev = &DebugState->HierarchySentinel;

	Hierarchy->Next->Prev = Hierarchy;
	Hierarchy->Prev->Next = Hierarchy;

	return(Hierarchy);
}

internal void
DEBUGStart(game_assets *Assets, u32 Width, u32 Height)
{
	TIMED_FUNCTION();

	debug_state *DebugState = (debug_state *)DebugGlobalMemory->DebugStorage;
	if (DebugState)
	{
		if (!DebugState->Initialized)
		{
			DebugState->HighPriorityQueue = DebugGlobalMemory->HighPriorityQueue;
			DebugState->HierarchySentinel.Next = &DebugState->HierarchySentinel;
			DebugState->HierarchySentinel.Prev = &DebugState->HierarchySentinel;
			DebugState->HierarchySentinel.Group = 0;

			InitializeArena(&DebugState->DebugArena, DebugGlobalMemory->DebugStorageSize - sizeof(debug_state), DebugState + 1);

			debug_variable_definition_context Context = {};
			Context.State = DebugState;
			Context.Arena = &DebugState->DebugArena;
			Context.Group = DEBUGBeginVariableGroup(&Context, "Root");

			DEBUGBeginVariableGroup(&Context, "Debugging");

			DEBUGCreateVariables(&Context);
			DEBUGBeginVariableGroup(&Context, "Profile");
			DEBUGBeginVariableGroup(&Context, "By Thread");
			debug_variable_reference *ThreadList =
				DEBUGAddVariable(&Context, DebugVariableType_ConterThreadList, "");
			ThreadList->Var->Profile.Dimension = V2(1024.0f, 100.0f);
			DEBUGEndVariableGroup(&Context);
			DEBUGBeginVariableGroup(&Context, "By Function");
			debug_variable_reference *FunctionList =
				DEBUGAddVariable(&Context, DebugVariableType_ConterThreadList, "");
			FunctionList->Var->Profile.Dimension = V2(1024.0f, 200.0f);
			DEBUGEndVariableGroup(&Context);
			DEBUGEndVariableGroup(&Context);

			DEBUGEndVariableGroup(&Context);

			DebugState->RootGroup = Context.Group;

			DebugState->RenderGroup = AllocateRenderGroup(Assets, &DebugState->DebugArena, Megabytes(2), false);
			
			DebugState->Paused = false;
			DebugState->ScopeToRecord = 0;

			DebugState->Initialized = true;
			
			SubArena(&DebugState->CollateArena, &DebugState->DebugArena, Megabytes(32), 4);
			DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);
			
			RestartCollation(DebugState, 0);
			
			AddHierarchy(DebugState, DebugState->RootGroup, V2(-0.5f*Width, 0.5f*Height));
		}

		BeginRender(DebugState->RenderGroup);
		DebugState->DebugFont = PushFont(DebugState->RenderGroup, DebugState->FontID);
		DebugState->DebugFontInfo = GetFontInfo(DebugState->RenderGroup->Assets, DebugState->FontID);

		DebugState->GlobalWidth = (r32)Width;
		DebugState->GlobalHeight = (r32)Height;

		asset_vector MatchVector = {};
		asset_vector WeightVector = {};
		MatchVector.E[Tag_FontType] = (r32)FontType_Debug;
		WeightVector.E[Tag_FontType] = 1.0f;
		DebugState->FontID = GetBestMatchFontFrom(Assets, Asset_Font,
			&MatchVector, &WeightVector);

		DebugState->FontScale = 1.0f;
		Orthographic(DebugState->RenderGroup, Width, Height, 1.0f);
		DebugState->LeftEdge = -0.5f*(r32)Width;
		DebugState->RightEdge = 0.5f*(r32)Width;
		
		DebugState->AtY = 0.5f*Height;
	}
}

inline b32
IsHex(char Char)
{
	b32 Result = (((Char >= '0') && (Char <= '9')) || ((Char >= 'A') && (Char <= 'F')));
	return(Result);
}

inline u32
GetHex(char Char)
{
	u32 Result = 0;

	if ((Char >= '0') && ((Char <= '9')))
	{
		Result = Char - '0';
	}
	else if ((Char >= 'A') && (Char <= 'F'))
	{
		Result = 0xA + (Char - 'A');
	}

	return(Result);
}

internal rectangle2
DEBUGTextOp(debug_state *DebugState, debug_text_op Op, v2 P, char *String, v4 Color = V4(1, 1, 1, 1))
{
	rectangle2 Result = InvertedInfinityRectangle2();
	if (DebugState && DebugState->DebugFont)
	{
		render_group *RenderGroup = DebugState->RenderGroup;
		loaded_font *Font = DebugState->DebugFont;
		hha_font *Info = DebugState->DebugFontInfo;

		u32 PrevCodePoint = 0;
		r32 CharScale = DebugState->FontScale;
		r32 AtY = P.y;
		r32 AtX = P.x;
		for (char *At = String;
			*At;)
		{
			if ((At[0] == '\\') &&
				(At[1] == '#') &&
				(At[2] != 0) &&
				(At[3] != 0) &&
				(At[4] != 0))
			{
				r32 CScale = 1.0f / 9.0f;
				Color = V4(Clamp01(CScale*(r32)(At[2] - '0')),
					Clamp01(CScale*(r32)(At[3] - '0')),
					Clamp01(CScale*(r32)(At[4] - '0')),
					1.0f);

				At += 5;
			}
			else if ((At[0] == '\\') &&
				(At[1] == '^') &&
				(At[2] == 0))
			{
				r32 CScale = 1.0f / 9.0f;
				CharScale = DebugState->FontScale*Clamp01(CScale*(r32)(At[2] - '0'));
				At += 4;
			}
			else
			{
				u32 CodePoint = *At;

				if ((At[0] == '\\') &&
					(IsHex(At[1])) &&
					(IsHex(At[2])) &&
					(IsHex(At[3])) &&
					(IsHex(At[4])))
				{
					CodePoint = 
						((GetHex(At[1]) << 12) |
						(GetHex(At[2]) << 8) |
						(GetHex(At[3]) << 4) |
						(GetHex(At[4]) << 0));

					At += 4;
				}

				r32 AdvanceX = CharScale*GetHorizontalAdvanceForPair(Info, Font, PrevCodePoint, CodePoint);
				AtX += AdvanceX;

				if (CodePoint != ' ')
				{
					bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, Info, Font, CodePoint);
					hha_bitmap *_Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

					r32 BitmapScale = CharScale*(r32)_Info->Dim[1];
					v3 BitmapOffset = V3(AtX, AtY, 0);
					if (Op == DEBUGTextOp_DrawText)
					{
						PushBitmap(RenderGroup, BitmapID, BitmapScale, BitmapOffset, Color);
					}
					else
					{
						Assert(Op == DEBUGTextOp_SizeText);

						loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationID);
						if (Bitmap)
						{
							used_bitmap_dim Dim = GetBitmapDim(RenderGroup, Bitmap, BitmapScale, BitmapOffset);
							rectangle2 GlyphDim = RectMinDim(Dim.P.xy, Dim.Size);
							Result = Union(Result, GlyphDim);
						}
					}
				}

				PrevCodePoint = CodePoint;

				++At;
			}
		}
	}

	return(Result);
}

internal void
DEBUGTextOutAt(v2 P, char *String, v4 Color = V4(1, 1, 1, 1))
{
	debug_state *DebugState = DEBUGGetState();
	if (DebugState)
	{
		render_group *RenderGroup = DebugState->RenderGroup;
		DEBUGTextOp(DebugState, DEBUGTextOp_DrawText, P, String, Color);
	}
}

internal rectangle2
DEBUGGetTextSize(debug_state *DebugState, char *String)
{
	rectangle2 Result = DEBUGTextOp(DebugState, DEBUGTextOp_SizeText, V2(0, 0), String);
	return(Result);
}

internal void
DEBUGTextLine(char *String)
{
	debug_state *DebugState = DEBUGGetState();
	if (DebugState)
	{
		render_group *RenderGroup = DebugState->RenderGroup;
		
		loaded_font *Font = PushFont(RenderGroup, DebugState->FontID);
		if (Font)
		{
			hha_font *Info = GetFontInfo(RenderGroup->Assets, DebugState->FontID);

			DEBUGTextOutAt(V2(DebugState->LeftEdge,
				DebugState->AtY - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), String);

			DebugState->AtY -= GetLineAdvanceFor(Info)*DebugState->FontScale;
		}
	}
}

struct debug_statistic
{
	r64 Min;
	r64 Max;
	r64 Avg;
	u32 Count;
};

inline void
BeginStatistic(debug_statistic *Stat)
{
	Stat->Min = Real32Maximum;
	Stat->Max = -Real32Maximum;
	Stat->Avg = 0.0;
	Stat->Count = 0;
}

inline void
AccumDebugStatistic(debug_statistic *Stat, r64 Value)
{
	++Stat->Count;
	
	if (Stat->Min > Value)
	{
		Stat->Min = Value;
	}

	if (Stat->Max < Value)
	{
		Stat->Max = Value;
	}

	Stat->Avg += Value;
}

inline void
EndDebugStatisctic(debug_statistic *Stat)
{
	if (Stat->Count)
	{
		Stat->Avg /= Stat->Count;
	}
	else
	{
		Stat->Min = Stat->Max = 0.0;
	}
}

internal memory_index
DEBUGVariableToText(char *Buffer, char *End, debug_variable *Var, u32 Flags)
{
	char *At = Buffer;

	if (Flags & DEBUGVarToText_AddDebugUI)
	{
		At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
			"#define DEBUGUI_");
	}

	if (Flags & DEBUGVarToText_AddName)
	{
		At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
			"%s%s ", Var->Name, (Flags & DEBUGVarToText_Colon) ? ":" : "");
	}
	
	switch (Var->Type)
	{
		case DebugVariableType_Bool32:
		{
			if (Flags & DEBUGVarToText_PrettyBools)
			{
				At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
					"%s", Var->Bool32 ? "true" : "false");
			}
			else
			{
				At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
					"%d", Var->Bool32);
			}
		} break;

		case DebugVariableType_Int32:
		{
			At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
				"%d", Var->Int32);
		} break;

		case DebugVariableType_UInt32:
		{
			At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
				"%u", Var->UInt32);
		} break;

		case DebugVariableType_V2:
		{
			At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
				"V2(%f, %f)", Var->Vector2.x, Var->Vector2.y);
		} break;

		case DebugVariableType_V3:
		{
			At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
				"V3(%f, %f, %f)", Var->Vector3.x, Var->Vector3.y, Var->Vector3.z);
		} break;

		case DebugVariableType_V4:
		{
			At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
				"V4(%f, %f, %f, %f)",
				Var->Vector4.x, Var->Vector4.y, Var->Vector4.z, Var->Vector4.w);
		} break;

		case DebugVariableType_Real32:
		{
			At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
				"%f", Var->Real32);
			if (Flags & DEBUGVarToText_FloatSuffix)
			{
				*At++ = 'f';
			}
		} break;

		case DebugVariableType_Group:
		{
			
		} break;

		InvalidDefaultCase;
	}

	if (Flags & DEBUGVarToText_LineFeedEnd)
	{
		*At++ = '\n';
	}

	if (Flags & DEBUGVarToText_NullTerminator)
	{
		*At++ = 0;
	}

	return(At - Buffer);
}

internal void
WriteHandmadeConfig(debug_state *DebugState)
{
	// TODO: Need a giant buffer here
	char Temp[4096];
	char *At = Temp;
	char *End = Temp + sizeof(Temp);

	int Depth = 0;
	debug_variable_reference *Ref = DebugState->RootGroup->Var->Group.FirstChild;

	while (Ref)
	{
		debug_variable *Var = Ref->Var;
		if (DEBUGShouldBeWritten(Var->Type))
		{
			for (int Indent = 0;
				Indent < Depth;
				++Indent)
			{
				*At++ = ' ';
				*At++ = ' ';
				*At++ = ' ';
				*At++ = ' ';
			}
			
			if (Var->Type == DebugVariableType_Group)
			{
				At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At), "//");
			}

			At += DEBUGVariableToText(At, End, Var, 
				DEBUGVarToText_AddDebugUI|
				DEBUGVarToText_AddName|
				DEBUGVarToText_LineFeedEnd|
				DEBUGVarToText_FloatSuffix);
		}		

		if (Var->Type == DebugVariableType_Group)
		{
			Ref = Var->Group.FirstChild;
			++Depth;
		}
		else
		{
			while (Ref)
			{
				if (Ref->Next)
				{
					Ref = Ref->Next;
					break;	
				}
				else
				{
					Ref = Ref->Parent;
					--Depth;
				}
			}
		}
	}
	Platform.DEBUGWriteEntireFile("../code/handmade_config.h", (u32)(At - Temp), Temp);

	if (!DebugState->Compiling)
	{
		DebugState->Compiling = true;
		DebugState->Compiler = Platform.DEBUGExecuteSystemCommand("..\\code", "c:\\windows\\system32\\cmd.exe", "/C build.bat");
	}
}

internal void
DrawProfileIn(debug_state *DebugState, rectangle2 ProfileRect, v2 MouseP)
{
	PushRect(DebugState->RenderGroup, ProfileRect, 0.0f, V4(0, 0, 0, 0.25f));

	r32 BarSpacing = 4.0f;
	r32 LaneHeight = 0;
	u32 LaneCount = DebugState->FrameBarLaneCount;

	u32 MaxFrame = DebugState->FrameCount;
	if (MaxFrame > 10)
	{
		MaxFrame = 10;
	}

	if ((LaneCount > 0) && (MaxFrame > 0))
	{
		r32 PixelsPerFramePlusSpacing = GetDim(ProfileRect).y / (r32)MaxFrame;
		r32 PixelsPerFrame = PixelsPerFramePlusSpacing - BarSpacing;
		LaneHeight = PixelsPerFrame / (r32)LaneCount;
	}

	r32 BarHeight = LaneHeight*LaneCount;
	r32 BarPlusSpacing = BarHeight + 4.0f;
	r32 ChartLeft = ProfileRect.Min.x;
	r32 ChartHeight = BarPlusSpacing*(r32)MaxFrame;
	r32 ChartWidth = GetDim(ProfileRect).x;
	r32 ChartTop = ProfileRect.Max.y;
	r32 Scale = ChartWidth*DebugState->FrameBarScale;

	v3 Colors[]
	{
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
		{1, 1, 0},
		{0, 1, 1},
		{1, 0, 1},
		{1, 0.5f, 0},
		{0, 1, 0.5f},
		{0.5f, 1, 0},
		{0, 1, 0.5f},
		{0.5f, 0, 1},
		{0, 0.5f, 1},
	};

	for (u32 FrameIndex = 0;
		FrameIndex < MaxFrame;
		++FrameIndex)
	{
		debug_frame *Frame = DebugState->Frames + DebugState->FrameCount - (FrameIndex + 1);
		r32 StackX = ChartLeft;
		r32 StackY = ChartTop - BarPlusSpacing*(r32)FrameIndex;
		for (u32 RegionIndex = 0;
			RegionIndex < Frame->RegionCount;
			++RegionIndex)
		{
			debug_frame_region *Region = Frame->Regions + RegionIndex;

			//v3 Color = Colors[RegionIndex % ArrayCount(Colors)];
			v3 Color = Colors[Region->ColorIndex % ArrayCount(Colors)];
			r32 ThisMinX = StackX + Scale*Region->MinT;
			r32 ThisMaxX = StackX + Scale*Region->MaxT;
			
			rectangle2 RegionRect = RectMinMax(V2(ThisMinX, StackY - LaneHeight*(Region->LaneIndex + 1)),
				V2(ThisMaxX, StackY - LaneHeight*Region->LaneIndex));
			PushRect(DebugState->RenderGroup, RegionRect, 0, V4(Color, 1));

			if (IsInRectangle(RegionRect, MouseP))
			{
				debug_record *Record = Region->Record;
				char TextBuffer[256];
				_snprintf_s(TextBuffer, sizeof(TextBuffer),
					"%s(%4d): [%s(%d)]",
					Record->BlockName,
					Region->CycleCount,
					Record->FileName,
					Record->LineNumber);
				DEBUGTextOutAt(MouseP + V2(0, 10), TextBuffer);
				
				//HotRecord = Record;
			}
		}
	}
#if 0
	PushRect(RenderGroup, V3(ChartLeft + 0.5f*ChartWidth, ChartMinY + ChartHeight, 0),
		V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
#endif

}

internal void
DEBUGDrawMainMenu(debug_state *DebugState, render_group *RenderGroup, v2 MouseP)
{
	for (debug_variable_hierarchy *Hierarchy = DebugState->HierarchySentinel.Next;
		Hierarchy != &DebugState->HierarchySentinel;
		Hierarchy = Hierarchy->Next)
	{
		r32 AtX = Hierarchy->UIP.x;
		r32 AtY = Hierarchy->UIP.y;
		r32 LineAdvance = DebugState->FontScale*GetLineAdvanceFor(DebugState->DebugFontInfo);

		r32 SpacingY = 4.0f;
		int Depth = 0;
		debug_variable_reference *Ref = Hierarchy->Group->Var->Group.FirstChild;
		while (Ref)
		{
			debug_variable *Var = Ref->Var;

			b32 IsHot = DebugState->Hot == Var;
			v4 ItemColor = (IsHot && (DebugState->HotInteraction == 0)) ? V4(1, 1, 0, 1) : V4(1, 1, 1, 1);
			
			rectangle2 Bounds = {};
			switch(Var->Type)
			{
				case DebugVariableType_ConterThreadList:
				{
					v2 MinCorner = V2(AtX + Depth*2.0f*LineAdvance, AtY - Var->Profile.Dimension.y);
					v2 MaxCorner = V2(MinCorner.x + Var->Profile.Dimension.x, AtY);
					v2 SizeP = V2(MaxCorner.x, MinCorner.y);
					Bounds = RectMinMax(MinCorner, MaxCorner);
					DrawProfileIn(DebugState, Bounds, MouseP);

					rectangle2 SizeBox = RectCenterHalfDim(SizeP, V2(4.0f, 4.0f));
					PushRect(DebugState->RenderGroup, SizeBox, 0,
						(IsHot && (DebugState->HotInteraction == DebugInteraction_ResizeProfile)) ? 
						V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
				
					if (IsInRectangle(SizeBox, MouseP))
					{
						DebugState->NextHotInteraction = DebugInteraction_ResizeProfile;
						DebugState->NextHot = Var;
					}
					else if (IsInRectangle(Bounds, MouseP))
					{
						DebugState->NextHotInteraction = DebugInteraction_None;
						DebugState->NextHot = Var;
					}

					Bounds.Min.y -= SpacingY;
				} break;
				
				default:
				{
					char Text[256];
					DEBUGVariableToText(Text, Text + sizeof(Text), Var,
						DEBUGVarToText_AddName|
						DEBUGVarToText_NullTerminator|
						DEBUGVarToText_Colon|
						DEBUGVarToText_PrettyBools);

					r32 LeftPx = AtX + Depth*2.0f*LineAdvance;
					r32 TopPy = AtY;
					rectangle2 TextBounds = DEBUGGetTextSize(DebugState, Text);

					Bounds = RectMinMax(V2(LeftPx + TextBounds.Min.x, TopPy - LineAdvance), 
						V2(LeftPx + TextBounds.Max.x, TopPy));
					DEBUGTextOutAt(V2(LeftPx, TopPy - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)),
						Text, ItemColor);
				
					if (IsInRectangle(Bounds, MouseP))
					{
						DebugState->NextHotInteraction = DebugInteraction_None;
						DebugState->NextHot = Var;
					}
				} break;
			}

			AtY = GetMinCorner(Bounds).y;

			if ((Var->Type == DebugVariableType_Group) && Var->Group.Expanded)
			{
				Ref = Var->Group.FirstChild;
				++Depth;
			}
			else
			{
				while (Ref)
				{
					if (Ref->Next)
					{
						Ref = Ref->Next;
						break;	
					}
					else
					{
						Ref = Ref->Parent;
						--Depth;
					}
				}
			}
		}

		DebugState->AtY = AtY;

		if (1)
		{
			rectangle2 MoveBox = RectCenterHalfDim(Hierarchy->UIP - V2(4.0f, 4.0f), V2(4.0f, 4.0f));
			PushRect(DebugState->RenderGroup, MoveBox, 0, V4(1, 1, 1, 1));

			if (IsInRectangle(MoveBox, MouseP))
			{
				DebugState->NextHotInteraction = DebugInteraction_MoveHierarchy;
				DebugState->NextHotHierarchy = Hierarchy;
			}
		}
	}

#if 0
	u32 NewHotMenuIndex = ArrayCount(DebugVariableList);
	r32 BestDistanceSq = Real32Maximum;

	r32 MenuRadius = 200.0f;
	r32 AngleStep = Tau32 / (r32)ArrayCount(DebugVariableList);
	for (u32 MenuItemIndex = 0;
		MenuItemIndex < ArrayCount(DebugVariableList);
		++MenuItemIndex)
	{
		debug_variable *Var = DebugVariableList + MenuItemIndex;
		char *Text = Var->Name;

		v4 ItemColor = Var->Value ? V4(1, 1, 1, 1) : V4(0.5f, 0.5f, 0.5f, 1.0f);
		if (MenuItemIndex == DebugState->HotMenuIndex)
		{
			ItemColor = V4(1, 1, 0, 1);
		}

		r32 Angle = (r32)MenuItemIndex*AngleStep;
		v2 TextP = DebugState->MenuP + (MenuRadius*Arm2(Angle));

		r32 ThisDistanceSq = LengthSq(TextP - MouseP);
		if (BestDistanceSq > ThisDistanceSq)
		{
			NewHotMenuIndex = MenuItemIndex;
			BestDistanceSq = ThisDistanceSq;
		}

		rectangle2 TextBounds = DEBUGGetTextSize(DebugState, Text);
		DEBUGTextOutAt(TextP - 0.5f*GetDim(TextBounds), Text, ItemColor);
	}

	if (LengthSq(MouseP - DebugState->MenuP) > MenuRadius)
	{
		DebugState->HotMenuIndex = NewHotMenuIndex;
	}
	else
	{
		DebugState->HotMenuIndex = ArrayCount(DebugVariableList);
	}
#endif
}

internal void
DEBUGBeginInteract(debug_state *DebugState, game_input *Input, v2 MouseP, b32 AltUI)
{

	if (DebugState->HotInteraction)
	{
		DebugState->Interaction = DebugState->HotInteraction;
	}
	else
	{
		if (DebugState->Hot)
		{
			if (AltUI)
			{
				DebugState->Interaction = DebugInteraction_TearValue;
			}
			else
			{
				switch (DebugState->Hot->Type)
				{
					case DebugVariableType_Bool32:
					{
						DebugState->Interaction = DebugInteraction_ToggleValue;
					} break;

					case DebugVariableType_Real32:
					{
						DebugState->Interaction = DebugInteraction_DragValue;
					} break;

					case DebugVariableType_Group:
					{
						DebugState->Interaction = DebugInteraction_ToggleValue;
					} break;
				}
			}
	
			if (DebugState->Interaction)
			{
				DebugState->InteractingWith = DebugState->Hot;
			}
		}
		else
		{
			DebugState->Interaction = DebugInteraction_NOP;
		}
	}
}

internal void
DEBUGEndInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
	if (DebugState->Interaction != DebugInteraction_NOP)
	{	
		debug_variable *Var = DebugState->InteractingWith;
		switch (DebugState->Interaction)
		{
			case DebugInteraction_ToggleValue:
			{
				Assert(Var);
				switch (Var->Type)
				{
					case DebugVariableType_Bool32:
					{
						Var->Bool32 = !Var->Bool32;
					} break;

					case DebugVariableType_Group:
					{
						Var->Group.Expanded = !Var->Group.Expanded;
					} break;
				}
			} break;
		}
		
		WriteHandmadeConfig(DebugState);
	}
	
	DebugState->Interaction = DebugInteraction_None;
	DebugState->InteractingWith = 0;
	DebugState->DraggingHierarchy = 0;
}

internal void
DEBUGInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
	v2 dMouseP = MouseP - DebugState->LastMouseP;
/*
	if (Input->MouseButtons[PlatformMouseBotton_Right].EndedDown)
	{
		if (Input->MouseButtons[PlatformMouseBotton_Right].HalfTransitionCount > 0)
		{
			DebugState->MenuP = MouseP;
		}
		DrawDebugMainMenu(DebugState, RenderGroup, MouseP);
	}
	else if (Input->MouseButtons[PlatformMouseBotton_Right].HalfTransitionCount > 0)
*/
	if (DebugState->Interaction)
	{
		debug_variable *Var = DebugState->InteractingWith;
		// Mouse move interaction
		switch (DebugState->Interaction)
		{
			case DebugInteraction_DragValue:
			{
				switch(Var->Type)
				{
					case DebugVariableType_Real32:
					{
						Var->Real32 += 9.1f*dMouseP.y;
					} break;
				}
			} break;

			case DebugInteraction_ResizeProfile:
			{
				Var->Profile.Dimension += V2(dMouseP.x, -dMouseP.y);
				Var->Profile.Dimension.x = Maximum(Var->Profile.Dimension.x, 10.0f);
				Var->Profile.Dimension.y = Maximum(Var->Profile.Dimension.y, 10.0f);
			} break;

			case DebugInteraction_MoveHierarchy:
			{
				DebugState->DraggingHierarchy->UIP += V2(dMouseP.x, dMouseP.y);
			} break;

			case DebugInteraction_TearValue:
			{
				if (!DebugState->DraggingHierarchy)
				{
					debug_variable_reference *RootGroup = DEBUGAddRootGroup(DebugState, "NewUserGroup");
					DEBUGAddVariableReference(DebugState, RootGroup, DebugState->InteractingWith);
					DebugState->DraggingHierarchy = AddHierarchy(DebugState, RootGroup, V2(0, 0));
				}

				DebugState->DraggingHierarchy->UIP = MouseP;
			} break;
		}

		b32 AltUI = Input->MouseButtons[PlatformMouseBotton_Right].EndedDown;

		// Click interaction
		for (u32 TransitionIndex = Input->MouseButtons[PlatformMouseBotton_Left].HalfTransitionCount;
			TransitionIndex > 1;
			--TransitionIndex)
		{
			DEBUGEndInteract(DebugState, Input, MouseP);
			DEBUGBeginInteract(DebugState, Input, MouseP, AltUI);
		}

		if (!Input->MouseButtons[PlatformMouseBotton_Left].EndedDown)
		{
			DEBUGEndInteract(DebugState, Input, MouseP);
		}
	}
	else
	{
		DebugState->Hot = DebugState->NextHot;
		DebugState->HotInteraction = DebugState->NextHotInteraction;
		DebugState->DraggingHierarchy = DebugState->NextHotHierarchy;

		b32 AltUI = Input->MouseButtons[PlatformMouseBotton_Right].EndedDown;
		for (u32 TransitionIndex = Input->MouseButtons[PlatformMouseBotton_Left].HalfTransitionCount;
			TransitionIndex > 1;
			--TransitionIndex)
		{
			DEBUGBeginInteract(DebugState, Input, MouseP, AltUI);
			DEBUGEndInteract(DebugState, Input, MouseP);
		}
		
		if (Input->MouseButtons[PlatformMouseBotton_Left].EndedDown)
		{
			DEBUGBeginInteract(DebugState, Input, MouseP, AltUI);
		}
	}

	DebugState->LastMouseP = MouseP;
}

internal void
DEBUGEnd(game_input *Input, loaded_bitmap *DrawBuffer)
{
	TIMED_FUNCTION();

	debug_state *DebugState = DEBUGGetState();
	if (DebugState)
	{
        render_group *RenderGroup = DebugState->RenderGroup;

		DebugState->NextHot = 0;
		DebugState->NextHotHierarchy = 0;
		DebugState->NextHotInteraction = DebugInteraction_None;
		debug_record *HotRecord = 0;

		v2 MouseP = V2(Input->MouseX, Input->MouseY);
		DEBUGDrawMainMenu(DebugState, RenderGroup, MouseP);
		DEBUGInteract(DebugState, Input, MouseP);

		if (DebugState->Compiling)
		{
			debug_process_state State = Platform.DEBUGGetProcessState(DebugState->Compiler);
			if (State.IsRunning)
			{
				DEBUGTextLine("COMPILING");
			}
			else
			{
				DebugState->Compiling = false;
			}
		}

        loaded_font *Font = DebugState->DebugFont;
		hha_font *Info = DebugState->DebugFontInfo;
		if (Font)
		{
#if 0
            for (u32 CounterIndex = 0;
                CounterIndex < DebugState->CounterCount;
                ++CounterIndex)
            {
                debug_counter_state *Counter = DebugState->CounterStates + CounterIndex;
                
                debug_statistic HitCount, CycleCount, CycleOverHit;
                BeginStatistic(&HitCount);
                BeginStatistic(&CycleCount);
                BeginStatistic(&CycleOverHit);
                for (u32 SnapshotIndex = 0;
                    SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
                    ++SnapshotIndex)
                {
                    AccumDebugStatistic(&HitCount, Counter->Snapshots[SnapshotIndex].HitCount);
                    AccumDebugStatistic(&CycleCount, (u32)Counter->Snapshots[SnapshotIndex].CycleCount);
                    
                    r64 HOC = 0.0;
                    if (Counter->Snapshots[SnapshotIndex].HitCount)
                    {
                        HOC = ((r64)Counter->Snapshots[SnapshotIndex].CycleCount /
                            (r64)Counter->Snapshots[SnapshotIndex].HitCount);
                    }
                    AccumDebugStatistic(&CycleOverHit, HOC);
                }
                EndDebugStatisctic(&HitCount);
                EndDebugStatisctic(&CycleCount);
                EndDebugStatisctic(&CycleOverHit);
                
				if (Counter->BlockName)
				{
					if (CycleCount.Max > 0.0f)
					{
						r32 BarWidth = 4.0f;
						r32 ChartLeft = 0;
						r32 ChartMinY = AtY;
						r32 ChartHeight = Info->AscenderHeight*FontScale;
						r32 Scale = 1.0f / (r32)CycleCount.Max;
						for (u32 SnapshotIndex = 0;
							SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
							++SnapshotIndex)
						{
							r32 ThisProportion = Scale*(r32)Counter->Snapshots[SnapshotIndex].CycleCount;
							r32 ThisHeight = ChartHeight*ThisProportion;
							PushRect(RenderGroup, V3(ChartLeft + BarWidth*(r32)SnapshotIndex + 0.5f*BarWidth, ChartMinY + 0.5f*ThisHeight, 0), V2(BarWidth, ThisHeight), V4(ThisProportion, 1, 0, 1));
						} 
					}
#if 1
					char TextBuffer[256];
					_snprintf_s(TextBuffer, sizeof(TextBuffer),
						"%32s(%4d): %10ucy %8uh %10ucy/h",
						Counter->BlockName,
						Counter->LineNumber,
						(u32)CycleCount.Avg,
						(u32)HitCount.Avg,
						(u32)CycleOverHit.Avg);
					DEBUGTextLine(TextBuffer);
#endif
				}
            }
#endif
		}

		if (DebugState->FrameCount)
		{
			char TextBuffer[256];
				_snprintf_s(TextBuffer, sizeof(TextBuffer),
					"Last frame time: %.02fms",
					DebugState->Frames[DebugState->FrameCount - 1].WallSecondsElapsed * 1000.0f);
				DEBUGTextLine(TextBuffer);
		}

		if (WasPressed(Input->MouseButtons[PlatformMouseBotton_Left]))
		{
			if (HotRecord)
			{
				DebugState->ScopeToRecord = HotRecord;
			}
			else
			{
				DebugState->ScopeToRecord = 0;
			}
			RefreshCollation(DebugState);
		}
	
		TileRenderGroupToOutput(DebugState->HighPriorityQueue, DebugState->RenderGroup, DrawBuffer);
		EndRenderGroup(DebugState->RenderGroup);
	}
}

#define DebugRecords_Main_Count __COUNTER__
extern u32 DebugRecords_Optimized_Count;

global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;

inline u32
GetLaneFromThreadID(debug_state *DebugState, u32 ThreadID)
{
	u32 Result = 0;
	return(Result);
}

internal debug_thread *
GetDebugThread(debug_state *DebugState, u32 ThreadID)
{
	debug_thread *Result = 0;
	for (debug_thread *Thread = DebugState->FirstThread;
		Thread;
		Thread = Thread->Next)
	{
		if (Thread->ID == ThreadID)
		{
			Result = Thread;
			break;
		}
	}

	if (!Result)
	{
		Result = PushStruct(&DebugState->CollateArena, debug_thread);
		Result->ID = ThreadID;
		Result->LaneIndex = DebugState->FrameBarLaneCount++;
		Result->FirstOpenBlock = 0;
		Result->Next = DebugState->FirstThread;
		DebugState->FirstThread = Result;
	}

	return(Result);
}

internal debug_frame_region *
AddRegion(debug_state *DebugState, debug_frame *CurrentFrame)
{
	Assert(CurrentFrame->RegionCount < MAX_REGION_PER_FRAME)
	debug_frame_region *Result = CurrentFrame->Regions + CurrentFrame->RegionCount++;

	return(Result);
}

internal void
RestartCollation(debug_state *DebugState, u32 InvalidEventArrayIndex)
{
	EndTemporaryMemory(DebugState->CollateTemp);
	DebugState->CollateTemp = BeginTemporaryMemory(&DebugState->CollateArena);
	
	DebugState->FirstThread = 0;
	DebugState->FirstFreeBlock = 0;

	DebugState->Frames = PushArray(&DebugState->CollateArena, MAX_DEBUG_EVENT_ARRAY_COUNT*4, debug_frame);
	DebugState->FrameBarLaneCount = 0;
	DebugState->FrameCount = 0;
	DebugState->FrameBarScale = 1.0f / 60000000.f;

	DebugState->CollationArrayIndex = InvalidEventArrayIndex + 1;
	DebugState->CollationFrame = 0;
}

inline debug_record *
GetRecordFrom(open_debug_block *Block)
{
	debug_record *Result = Block ? Block->Source : 0;
	return(Result);
}

internal void
CollateDebugRecords(debug_state *DebugState, u32 InvalidEventArrayIndex)
{
	for (;
		;
		++DebugState->CollationArrayIndex)
	{
		if (DebugState->CollationArrayIndex == MAX_DEBUG_EVENT_ARRAY_COUNT)
		{
			DebugState->CollationArrayIndex = 0;
		}

		u32 EventArrayIndex = DebugState->CollationArrayIndex;
		if (EventArrayIndex == InvalidEventArrayIndex)
		{
			break;
		}

		for (u32 EventIndex = 0;
			EventIndex < GlobalDebugTable->EventCount[EventArrayIndex];
			++EventIndex)
		{
			debug_event *Event = GlobalDebugTable->Events[EventArrayIndex] + EventIndex;
			debug_record *Source = (GlobalDebugTable->Records[Event->TranslationUnit] + 
				Event->DebugRecordIndex);
			
			if (Event->Type == DebugEvent_FrameMarker)
			{
				if (DebugState->CollationFrame)
				{
					DebugState->CollationFrame->EndClock = Event->Clock;
					DebugState->CollationFrame->WallSecondsElapsed = Event->SecondsElapsed;
					++DebugState->FrameCount;
#if 0
					r32 ClockRange = (r32)(DebugState->CollationFrame->EndClock - DebugState->CollationFrame->BeginClock);

					if (ClockRange > 0.0f)
					{
						r32 FrameBarScale = 1.0f / ClockRange;
						if (DebugScale->FrameBarScale > FrameBarScale)
						{
							DebugState->FrameBarScale = FrameBarScale;
						}
					}
#endif
			}

				DebugState->CollationFrame = DebugState->Frames + DebugState->FrameCount;
				DebugState->CollationFrame->BeginClock = Event->Clock;
				DebugState->CollationFrame->EndClock = 0;
				DebugState->CollationFrame->RegionCount = 0;
				DebugState->CollationFrame->Regions = PushArray(&DebugState->CollateArena, MAX_REGION_PER_FRAME, debug_frame_region);
				DebugState->CollationFrame->WallSecondsElapsed = 0;
			}
			else if (DebugState->CollationFrame)
			{
				u32 FrameIndex = DebugState->FrameCount - 1;
				debug_thread *Thread = GetDebugThread(DebugState, Event->TC.ThreadID);
				u32 RelativeClock = Event->Clock - DebugState->CollationFrame->BeginClock;
				if (Event->Type == DebugEvent_BeginBlock)
				{
					open_debug_block *DebugBlock = DebugState->FirstFreeBlock;
					if (DebugBlock)
					{
						DebugState->FirstFreeBlock = DebugBlock->NextFree;
					}
					else
					{
						DebugBlock = PushStruct(&DebugState->CollateArena, open_debug_block);
					}

					DebugBlock->StartingFrameIndex = FrameIndex;
					DebugBlock->OpeningEvent = Event;
					DebugBlock->Parent = Thread->FirstOpenBlock;
					DebugBlock->Source = Source;
					Thread->FirstOpenBlock = DebugBlock;
					DebugBlock->NextFree = 0;
				}
				else if (Event->Type == DebugEvent_EndBlock)
				{
					if (Thread->FirstOpenBlock)
					{
						open_debug_block *MatchingBlock = Thread->FirstOpenBlock;
						debug_event *OpeningEvent = MatchingBlock->OpeningEvent;
						if ((OpeningEvent->TC.ThreadID == Event->TC.ThreadID) &&
							(OpeningEvent->DebugRecordIndex == Event->DebugRecordIndex) && 
							(OpeningEvent->TranslationUnit == Event->TranslationUnit))
						{
							if (MatchingBlock->StartingFrameIndex == FrameIndex)
							{
								if (GetRecordFrom(MatchingBlock->Parent) == DebugState->ScopeToRecord)
								{
									r32 MinT = (r32)(OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
									r32 MaxT = (r32)(Event->Clock - DebugState->CollationFrame->BeginClock);
									r32 ThresholdT = 0.01;
									if ((MaxT - MinT) > ThresholdT)
									{
										debug_frame_region *Region = AddRegion(DebugState, DebugState->CollationFrame);
										Region->Record = Source;
										Region->CycleCount = (Event->Clock - OpeningEvent->Clock);
										Region->LaneIndex = (u16)Thread->LaneIndex;
										Region->MinT = (r32)(OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
										Region->MaxT = (r32)(Event->Clock - DebugState->CollationFrame->BeginClock);
										Region->ColorIndex = (u16)OpeningEvent->DebugRecordIndex;
									}
								}
							}
							else
							{
								// TODO: Record all frame in between and begin/end spans!
							}

							Thread->FirstOpenBlock->NextFree = DebugState->FirstFreeBlock;
							DebugState->FirstFreeBlock = Thread->FirstOpenBlock;
							Thread->FirstOpenBlock = MatchingBlock->Parent;
						}
						else
						{
							// TODO: Record span that goes to the beginning of the frame series?
						}
					}
				}
				else
				{
					Assert(!"Invalid event type");
				}
			}
		}
	}
}

internal void
RefreshCollation(debug_state *DebugState)
{
	RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
	CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd)
{
	GlobalDebugTable->RecordCount[0] = DebugRecords_Main_Count;
	GlobalDebugTable->RecordCount[1] = DebugRecords_Optimized_Count;

	++GlobalDebugTable->CurrentEventArrayIndex;
	if (GlobalDebugTable->CurrentEventArrayIndex >= ArrayCount(GlobalDebugTable->Events))
	{
		GlobalDebugTable->CurrentEventArrayIndex = 0;
	}

	u64 ArrayIndex_EventIndex = AtomicExchangeU64(
		&GlobalDebugTable->EventArrayIndex_EventIndex,
		(u64)GlobalDebugTable->CurrentEventArrayIndex << 32);

	u32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
	u32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;
	GlobalDebugTable->EventCount[EventArrayIndex] = EventCount;

	debug_state *DebugState = DEBUGGetState(Memory);
	if (DebugState)
	{
		if (Memory->ExecutableReloaded)
		{
			RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
		}
	
		if (!DebugState->Paused)
		{
			if (DebugState->FrameCount >= (MAX_DEBUG_EVENT_ARRAY_COUNT*4))
			{
				RestartCollation(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
			}
			
			CollateDebugRecords(DebugState, GlobalDebugTable->CurrentEventArrayIndex);
		}
	}

	return(GlobalDebugTable);
}