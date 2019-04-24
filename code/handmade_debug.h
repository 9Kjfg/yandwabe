#if !defined(HANDMADE_DEBUG_H)

struct debug_variable;

enum debug_variable_to_text_flag
{
    DEBUGVarToText_AddDebugUI = 0x1,
    DEBUGVarToText_AddName = 0x2,
    DEBUGVarToText_FloatSuffix = 0x4,
    DEBUGVarToText_LineFeedEnd = 0x8,
    DEBUGVarToText_NullTerminator = 0x10,
    DEBUGVarToText_Colon = 0x20,
    DEBUGVarToText_PrettyBools = 0x40
};

enum debug_variable_type
{
	DebugVariableType_Bool32,
    DebugVariableType_Int32,
    DebugVariableType_UInt32,
    DebugVariableType_Real32,
    DebugVariableType_V2,
    DebugVariableType_V3,
    DebugVariableType_V4,

    DebugVariableType_Group,
};

struct debug_variable_group
{
    b32 Expanded;
    debug_variable *FirstChild;
    debug_variable *LastChild;
};

struct debug_variable
{
	debug_variable_type Type;
	char *Name;
    debug_variable *Next;
	debug_variable *Parent;
    
    union
    {
        b32 Bool32;
        s32 Int32;
        u32 UInt32;
        r32 Real32;
        v2 Vector2;
        v3 Vector3;
        v4 Vector4;
        debug_variable_group Group;
    };
};

struct render_group;
struct game_assets;
struct loaded_bitmap;
struct loaded_font;
struct hha_font;

enum debug_text_op
{
    DEBUGTextOp_DrawText,
    DEBUGTextOp_SizeText,
};

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

    debug_variable *RootGroup;

    render_group *RenderGroup;
    loaded_font *DebugFont;
    hha_font *DebugFontInfo;

    b32 Compiling;
    debug_executing_process Compiler;

    v2 MenuP;
    b32 MenuActive;
    
    debug_variable *HotVariable;

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

    b32 ProfileOn;
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