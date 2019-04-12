#if !defined(HANDMADE_DEBUG_H)

struct debug_counter_snapshot
{
    u32 HitCount;
    u64 CycleCount;
};

struct debug_counter_state
{
    char *FileName;
    char *BlockName;

    int LineNumber;
};

struct debug_frame_region
{
    u32 LaneIndex;
    r32 MinT;
    r32 MaxT;
};

struct debug_frame
{
    u64 BeginClock;
    u64 EndClock;

    u32 RegionCount;
    debug_frame_region *Regions;
};

struct debug_state
{
    b32 Initialized;

    // NOTE: Collation
    memory_arena CollateArena;
    temporary_memory CollateTemp;

    u32 FrameBarLaneCount;
    u32 FrameCount;
    r32 FrameBarScale;

    debug_frame *Frames;
};

// TODO: Fix this for looped live code editing
struct render_group;
struct game_assets;
global_variable render_group *DEBUGRenderGroup;

internal void DEBUGReset(game_assets *Assets, u32 Width, u32 Height);
internal void DEBUGOverlay(game_memory *Memory);

#define HANDMADE_DEBUG_H
#endif