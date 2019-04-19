#if !defined(HANDMADE_DEBUG_H)

struct render_group;
struct game_assets;
struct loaded_bitmap;

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
    debug_record *Record;
    u32 CycleCount;
    u16 LaneIndex;
    u16 ColorIndex;
    r32 MinT;
    r32 MaxT;
};

#define MAX_REGION_PER_FRAME (4096)
struct debug_frame
{
    u64 BeginClock;
    u64 EndClock;
    r32 WallSecondsElapsed;

    u32 RegionCount;
    debug_frame_region *Regions;
};

struct open_debug_block
{
    u32 StartingFrameIndex;
    debug_record *Source;
    debug_event *OpeningEvent;   
    open_debug_block *Parent;

    open_debug_block *NextFree;
};

struct debug_thread
{
    u32 ID;
    u32 LaneIndex;
    open_debug_block *FirstOpenBlock;
    debug_thread *Next;
};

struct debug_state
{
    b32 Initialized;
    b32 Paused;

    platform_work_queue *HighPriorityQueue;

    memory_arena DebugArena;
    render_group *RenderGroup;

    r32 LeftEdge;
    r32 AtY;
    r32 FontScale;
    font_id FontID;
    r32 GlobalWidth;
    r32 GlobalHeight;

    debug_record *ScopeToRecord;
    // NOTE: Collation
    memory_arena CollateArena;
    temporary_memory CollateTemp;

    u32 CollationArrayIndex;
    debug_frame *CollationFrame;
    u32 FrameBarLaneCount;
    u32 FrameCount;
    r32 FrameBarScale;

    rectangle2 ProfileRect;
    
    debug_frame *Frames;
    debug_thread *FirstThread;
    open_debug_block *FirstFreeBlock;
};


internal void DEBUGStart(game_assets *Assets, u32 Width, u32 Height);
internal void DEBUGEnd(game_input *Input, loaded_bitmap *DrawBuffer);

internal void RefreshCollation(debug_state *DebugState);

#define HANDMADE_DEBUG_H
#endif