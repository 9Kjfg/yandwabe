#if !defined(HANDMADE_DEBUG_H)

#define DEBUG_FRAME_COUNT 256

enum debug_variable_to_text_flag
{
    DEBUGVarToText_AddDebugUI = 0x1,
    DEBUGVarToText_AddName = 0x2,
    DEBUGVarToText_FloatSuffix = 0x4,
    DEBUGVarToText_LineFeedEnd = 0x8,
    DEBUGVarToText_NullTerminator = 0x10,
    DEBUGVarToText_Colon = 0x20,
    DEBUGVarToText_PrettyBools = 0x40,
    DEBUGVarToText_ShowEntireGUID = 0x80,
    DEBUGVarToText_AddValue = 0x100
};

struct debug_tree;

struct debug_view_inline_block
{
    v2 Dim;
};

struct debug_view_profile_graph
{
    debug_view_inline_block Block;
    char *GUID;
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
        debug_view_profile_graph ProfileGraph;
        debug_view_collapsible Collapsible;
    };
};

struct debug_profile_node
{
    struct debug_element *Element;
    struct debug_stored_event *FirstChild;
    struct debug_stored_event *NextSameParent;
    u64 Duration;
    u64 ParentRelativeClock;
    u32 Reserved;
    u16 ThreadOrdinal;
    u16 CoreIndex;
};

struct debug_stored_event
{
    union
    {
        debug_stored_event *Next;
        debug_stored_event *NextFree;
    };

    u32 FrameIndex;

    union
    {
        debug_event Event;
        debug_profile_node ProfileNode;
    };
    // TODO: Stall call attribution data here
};

/*
struct debug_string
{
    u32 Length;
    char *Value;
};
 */

struct debug_element_frame
{
    debug_stored_event *OldestEvent;
    debug_stored_event *MostRecentEvent;
};

struct debug_element
{
    char *OriginalGUID; // NOTE: Can never be printed! Might point into unloaded DLL.
    char *GUID;
    u32 FileNameCount;
    u32 LineNumber;
    u32 NameStartsAt;

    debug_type Type;

    b32 ValueWasEdited;

    debug_element_frame Frames[DEBUG_FRAME_COUNT];

    debug_element *NextInHash;
};

inline char *GetName(debug_element *Element) {char *Result = Element->GUID + Element->NameStartsAt; return(Result);}
//inline debug_string GetFileName(debug_element *Element) {debug_string Result = {Element->FileNameCount, Element->GUID}; return(Result);}

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
    char *Name;
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

#define MAX_REGION_PER_FRAME (4096)
struct debug_frame
{
    // IMPORTANT: This actually gets freed as a set in FreeFrame
    u64 BeginClock;
    u64 EndClock;
    r32 WallSecondsElapsed;

    r32 FrameBarScale;

    u32 FrameIndex;

    u32 StoredEventCount;
    u32 ProfileBlockCount;
    u32 DataBlockCount;

    debug_stored_event *RootProfileNode;
};

struct open_debug_block
{
    union
    {
        open_debug_block *Parent;
        open_debug_block *NextFree;
    };

    u32 StartingFrameIndex;
    debug_element *Element;
    u64 BeginClock;
    debug_stored_event *Node;
    
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

#include "handmade_debug_ui.h"

struct debug_state
{
    b32 Initialized;

    platform_work_queue *HighPriorityQueue;

    memory_arena DebugArena;
    memory_arena PerFrameArena;

    render_group RenderGroup;
    loaded_font *DebugFont;
    hha_font *DebugFontInfo;

    object_transform TextTransform;
	object_transform ShadowTransform;
    object_transform UITransform;
    object_transform BackingTransform;

    v2 MenuP;
    b32 MenuActive;

    u32 SelectedIDCount;
    debug_id SelectedID[64];

    debug_element *ElementHash[1024];
    debug_view *ViewHash[4096];
    debug_variable_group *RootGroup;
    debug_variable_group *ProfileGroup;
    debug_tree TreeSentinel;

    v2 LastMouseP;
    b32 AltUI;
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

    r32 MouseTextStackY;

    char *ScopeToRecord;

    u32 TotalFrameCount;

    u32 ViewingFrameOrdinal;

    u32 MostRecentFrameOrdinal;
    u32 CollationFrameOrdinal;
    u32 OldestFrameOrdinal;
    debug_frame Frames[DEBUG_FRAME_COUNT];

    debug_element *RootProfileElement;

    u32 FrameBarLaneCount;
    debug_thread *FirstThread;
    debug_thread *FirstFreeThread;
    open_debug_block *FirstFreeBlock;

    debug_stored_event *FirstFreeStoredEvent;
};

struct debug_statistic
{
	r64 Min;
	r64 Max;
	r64 Avg;
	u32 Count;
};

internal debug_variable_group *CreateVariableGroup(debug_state *DebugState, u32 NameLength, char *Name);
internal debug_variable_group *CloneVariableGroup(debug_state *DebugState, debug_variable_link *Source);

inline b32
DebugIDsAreEqual(debug_id A, debug_id B)
{
	b32 Result = 
		((A.Value[0] == B.Value[0]) && 
		(A.Value[1] == B.Value[1]));
		
	return(Result);
}

#define HANDMADE_DEBUG_H
#endif