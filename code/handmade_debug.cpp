// TODO: Stop using stdio
#include <stdio.h>

global_variable r32 LeftEdge;
global_variable r32 AtY;
global_variable r32 FontScale;
global_variable font_id FontID;

internal void
UpdateDebugRecords(debug_state *DebugState, u32 CounterCount, debug_record *Counters)
{
	for (u32 CounterIndex = 0;
		CounterIndex < CounterCount;
		++CounterIndex)
	{
		debug_record *Source = Counters + CounterIndex;
		debug_counter_state *Dest = DebugState->CounterStates + DebugState->CounterCount++;
		
		u64 HitCount_CycleCount = AtomicExchangeU64(&Source->HitCount_CycleCount, 0);
		Dest->FileName = Source->FileName;
		Dest->FunctionName = Source->FunctionName;
		Dest->LineNumber = Source->LineNumber;
		Dest->Snapshots[DebugState->SnapshotIndex].HitCount = (u32)(HitCount_CycleCount >> 32);
		Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = (u32)(HitCount_CycleCount & 0xFFFFFFFF);
	}
}

internal void
DEBUGReset(game_assets *Assets, u32 Width, u32 Height)
{
	TIMED_BLOCK();

	asset_vector MatchVector = {};
	asset_vector WeightVector = {};
	MatchVector.E[Tag_FontType] = (r32)FontType_Debug;
	WeightVector.E[Tag_FontType] = 1.0f;
	FontID = GetBestMatchFontFrom(Assets, Asset_Font,
		&MatchVector, &WeightVector);

	FontScale = 1.0f;
	Orthographic(DEBUGRenderGroup, Width, Height, 1.0f);
	LeftEdge = -0.5f*(r32)Width;
	
	hha_font *Info = GetFontInfo(Assets, FontID);
	AtY = 0.5f*Height - FontScale*GetStartingBaselineY(Info);
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

internal void
DEBUGTextLine(char *String)
{
	if (DEBUGRenderGroup)
	{
		render_group *RenderGroup = DEBUGRenderGroup;
		
		loaded_font *Font = PushFont(RenderGroup, FontID);
		if (Font)
		{
			hha_font *Info = GetFontInfo(RenderGroup->Assets, FontID);

			u32 PrevCodePoint = 0;
			r32 CharScale = FontScale;
			v4 Color = V4(1, 1, 1, 1);
			r32 AtX = LeftEdge;
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
					CharScale = FontScale*Clamp01(CScale*(r32)(At[2] - '0'));
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
						hha_bitmap *Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

						PushBitmap(RenderGroup, BitmapID, CharScale*(r32)Info->Dim[1], V3(AtX, AtY, 0), V4(1, 1, 1, 1));
					}

					PrevCodePoint = CodePoint;

					++At;
				}
			}

			AtY -= GetLineAdvanceFor(Info)*FontScale;
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

internal void
DEBUGOverlay(game_memory *Memory)
{
	debug_state *DebugState = (debug_state *)Memory->DebugStorage;
	if (DebugState && DEBUGRenderGroup)
	{
        render_group *RenderGroup = DEBUGRenderGroup;

        // TODO: Layout/ cached font info / etc. for real debug display
        loaded_font *Font = PushFont(RenderGroup, FontID);
		if (Font)
		{
            hha_font *Info = GetFontInfo(RenderGroup->Assets, FontID);

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
                    AccumDebugStatistic(&CycleCount, Counter->Snapshots[SnapshotIndex].CycleCount);
                    
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
                
				if (Counter->FunctionName)
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
						Counter->FunctionName,
						Counter->LineNumber,
						(u32)CycleCount.Avg,
						(u32)HitCount.Avg,
						(u32)CycleOverHit.Avg);
					DEBUGTextLine(TextBuffer);
#endif
				}
            }
			
			r32 BarWidth = 8.0f;
			r32 BarSpacing = 10.0f;
			r32 ChartLeft = LeftEdge + 10.0f;
			r32 ChartHeight = 300.0f;
			r32 ChartWidth = BarSpacing*(r32)DEBUG_SNAPSHOT_COUNT;
			r32 ChartMinY = AtY - (ChartHeight + 10.0f);
			r32 Scale = 1.0f / 0.03333f;

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

			for (u32 SnapshotIndex = 0;
				SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
				++SnapshotIndex)
			{
				debug_frame_end_info *Info = DebugState->FrameEndInfos + SnapshotIndex;
				r32 SrackY = ChartMinY;
				r32 PrevTimestampSeconds = 0.0f;
				for (u32 TimestampIndex = 0;
					TimestampIndex < Info->TimestampCount;
					++TimestampIndex)
				{
					debug_frame_timestamp *Timestamp = Info->Timestamps + TimestampIndex;
					r32 ThisSecondsElapsed = Timestamp->Seconds - PrevTimestampSeconds;
					PrevTimestampSeconds = Timestamp->Seconds;

					v3 Color = Colors[TimestampIndex % ArrayCount(Colors)];
					r32 ThisProportion = Scale*ThisSecondsElapsed;
					r32 ThisHeight = ChartHeight*ThisProportion;
					PushRect(RenderGroup, V3(ChartLeft + BarSpacing*(r32)SnapshotIndex + 0.5f*BarWidth, 
						SrackY + 0.5f*ThisHeight, 0), V2(BarWidth, ThisHeight), V4(Color, 1));
					SrackY += ThisHeight;
				}
			}

			PushRect(RenderGroup, V3(ChartLeft + 0.5f*ChartWidth, ChartMinY + ChartHeight, 0),
				V2(ChartWidth, 1.0f), V4(1, 1, 1, 1));
        }
	}
}

debug_record DebugRecords_Main[__COUNTER__];

extern u32 const DebugRecords_Optimized_Count;
debug_record DebugRecords_Optimized[];

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd)
{
	debug_state *DebugState = (debug_state *)Memory->DebugStorage;
	if (DebugState)
	{
		DebugState->CounterCount = 0;
		UpdateDebugRecords(DebugState, ArrayCount(DebugRecords_Main), DebugRecords_Main);
		UpdateDebugRecords(DebugState, DebugRecords_Optimized_Count, DebugRecords_Optimized);

		DebugState->FrameEndInfos[DebugState->SnapshotIndex] = *Info;

		++DebugState->SnapshotIndex;
		if (DebugState->SnapshotIndex >= DEBUG_SNAPSHOT_COUNT)
		{
			DebugState->SnapshotIndex = 0;
		}
	}
}