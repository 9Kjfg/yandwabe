#if !defined(HANDMADE_DEBUG_H)

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

struct debug_tree;

struct debug_view_inline_block
{
    v2 Dim;
};

struct debug_view_collapsible
{
    b32 ExpandedAlways;
    b32 ExpandedAltView;
};

enum debug_view_type
{
    DebugViewType_Unknown,
    
    DebugViewType_Basic,
    DebugViewType_InlineBlock,
    DebugViewType_Collapsible,
};

struct debug_view
{
    debug_id ID;
    debug_view *NextInHash;
    
    debug_view_type Type;

    union
    {
        debug_view_inline_block InlineBlock;
        debug_view_collapsible  Collapsible;
    };
};

struct debug_stored_event
{
    union
    {
        debug_stored_event *Next;
        debug_stored_event *NextFree;
    };

    u32 FrameIndex;
    debug_event Event;
};

struct debug_element
{
    char *GUID;
    debug_element *NextInHash;

    debug_stored_event *OldestEvent;
    debug_stored_event *MostRecentEvent;
};

struct debug_variable_group;
struct debug_variable_link
{
    debug_variable_link *Next;
    debug_variable_link *Prev;
    debug_variable_group *Children;
    debug_element *Element;
};

struct debug_variable_group
{
    debug_variable_link Sentinel;
};

struct debug_tree
{
    v2 UIP;
    debug_variable_group *Group;

    debug_tree *Next;
    debug_tree *Prev;
};

// struct debug_variable
// {
// 	debug_type Type;
// 	char *Name;
//     debug_event Event;
// };

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
    // TODO: Do we want to copy these out in their entire
    debug_event *Event;
    u32 CycleCount;
    u16 LaneIndex;
    u16 ColorIndex;
    r32 MinT;
    r32 MaxT;
};

#define MAX_REGION_PER_FRAME (4096)
struct debug_frame
{
    // IMPORTANT: This actually gets freed as a set in FreeFrame

    union
    {
        debug_frame *Next;
        debug_frame *NextFree;
    };
    
    u64 BeginClock;
    u64 EndClock;
    r32 WallSecondsElapsed;

    r32 FrameBarScale;

    u32 FrameIndex;
};

struct open_debug_block
{
    union
    {
        open_debug_block *Parent;
        open_debug_block *NextFree;
    };

    u32 StartingFrameIndex;
    debug_event *OpeningEvent;   

    // NOTE: Only for data blocks?  Probably!
    debug_variable_group *Group;
};

struct debug_thread
{
    u32 ID;
    u32 LaneIndex;
    open_debug_block *FirstOpenCodeBlock;
    open_debug_block *FirstOpenDataBlock;

    union
    {
        debug_thread *Next;
        debug_thread *NextFree;
    };
};

enum debug_interaction_type
{
    DebugInteraction_None,

    DebugInteraction_NOP,

    DebugInteraction_AutoModifyVariable,

    DebugInteraction_ToggleValue,
    DebugInteraction_DragValue,
    DebugInteraction_TearValue,

    DebugInteraction_Resize,
    DebugInteraction_Move,

    DebugInteraction_Select,
};

struct debug_interaction
{
    debug_id ID;
    debug_interaction_type Type;
    union
    {
        void *Generic;
        debug_event *Event;
        debug_tree *Tree;
        v2 *P;
    };
};

struct debug_state
{
    b32 Initialized;

    platform_work_queue *HighPriorityQueue;

    memory_arena DebugArena;
    memory_arena PerFrameArena;

    render_group *RenderGroup;
    loaded_font *DebugFont;
    hha_font *DebugFontInfo;

    b32 Compiling;
    debug_executing_process Compiler;

    v2 MenuP;
    b32 MenuActive;

    u32 SelectedIDCount;
    debug_id SelectedID[64];

    debug_element *ElementHash[1024];
    debug_view *ViewHash[4096];
    debug_tree *RootTree;
    debug_tree TreeSentinel;

    v2 LastMouseP;
    debug_interaction Interaction;
    debug_interaction HotInteraction;
    debug_interaction NextHotInteraction;
    b32 Paused;

    r32 LeftEdge;
    r32 RightEdge;
    r32 AtY;
    r32 FontScale;
    font_id FontID;
    r32 GlobalWidth;
    r32 GlobalHeight;

    char *ScopeToRecord;

    u32 TotalFrameCount;
    u32 FrameCount;
    debug_frame *OldestFrame;
    debug_frame *MostRecentFrame;

    debug_frame *CollationFrame;

    u32 FrameBarLaneCount;
    debug_thread *FirstThread;
    debug_thread *FirstFreeThread;
    open_debug_block *FirstFreeBlock;
    
    // NOTE: Per-frame storage managment
    debug_stored_event *FirstFreeStoredEvent;
    debug_frame *FirstFreeFrame;
};

#define HANDMADE_DEBUG_H
#endif