#if !defined(HANDMADE_PLATFORM_H)


/*
	HANDMADE_INTERNAL:
		0 - Build for public
		1 - Build for developer only

	HANDMADE_SLOW:
		0 - Not slow code allowed
		1 - Slow code welcome
*/

#ifdef __cplusplus
extern "C" {
#endif

//
// NOTE: Compiler
//

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO: Moar compilerz!!
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SSE/NEON optimization are not available for this compiler yet!!!
#endif

//
// NOTE: Types
//
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;

typedef size_t memory_index;

typedef int8 s8;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef int32 b32;

typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef float r32;

typedef float real32;
typedef double real64;

#define Real32Maximum FLT_MAX

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f
#define Tau32 6.2831853071f

#if HANDMADE_SLOW
#define Assert(Expression) if (!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath");
#define InvalidDefaultCase default: {InvalidCodePath;} break;

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
//TODO: swap, min, max ...macros?

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

inline uint32
SafeTruncateUint64(uint64 Value)
{	
	// TODO: Define for maximum values
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return(Result);
}

inline uint16
SafeTruncateToUInt16(int32 Value)
{	
	// TODO: Define for maximum values
	Assert(Value <= 65525);
	Assert(Value >= 0);
	uint16 Result = (uint16)Value;
	return(Result);
}

inline uint16
SafeTruncateToInt16(int32 Value)
{	
	// TODO: Define for maximum values
	Assert(Value < 32767);
	Assert(Value > -32768);
	uint16 Result = (uint16)Value;
	return(Result);
}


/*
    NOTE: Services that the platform layer provides to the game
*/
#if HANDMADE_INTERNAL
/* IMPORTANT
	These are NOT for doing anything in the shipping game - they are
	blocking and the write doesn't protect against lost data
*/
typedef struct debug_read_file_result
{
	uint32 ContentsSize;
	void *Contents;
} debug_read_file_result;

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

// TODO: Give these things names soon!
enum
{
	/* 0 */DebugCycleCounter_GameUpdateAndRender,
	/* 1 */DebugCycleCounter_RenderGroupToOutput,
	/* 2 */DebugCycleCounter_DrawRectangleSlowly,
	/* 3 */DebugCycleCounter_ProcessPixel,
	/* 4 */DebugCycleCounter_DrawRectangleQuickly,
	DebugCycleCounter_Count,
};

typedef struct debug_cycle_counter
{
	uint64 CycleCount;
	uint32 HitCount;
} debug_cycle_counter;

extern struct game_memory *DebugGlobalMemory;
#if _MSC_VER
	#define BEGIN_TIMED_BLOCK(ID) uint64 StartCycleCount##ID = __rdtsc();
	#define END_TIMED_BLOCK(ID) DebugGlobalMemory->Counters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount##ID; \
		++DebugGlobalMemory->Counters[DebugCycleCounter_##ID].HitCount;
// TODO: Clamp END_TIMED_BLOCK_COUNTED so that if the calc is wrong, it won't overflow?
	#define END_TIMED_BLOCK_COUNTED(ID, Count) DebugGlobalMemory->Counters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount##ID; \
		DebugGlobalMemory->Counters[DebugCycleCounter_##ID].HitCount += (Count);
#else
	#define BEGIN_TIMED_BLOCK(ID)
	#define END_TIMED_BLOCK(ID)
#endif

#endif

/*
    NOTE: Services that the game provides to the platform layer
    (this may expand in the future sound on separate thred)
*/

// FOUR THINGS - timing, conroller/keyboard input, bitmap buffer to use, sound buffer to use

// TODO: In the future, rendering _specifically_ will become a three-tiered abstaraction

#define BITMAP_BYTES_PER_PIXEL 4
typedef struct game_offscreen_buffer
{
	// NOTE: Pixels are always 32-bits wide. Memory Order BB GG RR XX
	void *Memory;
	int Width;
	int Height;
	int Pitch;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int SampleCount;

	// NOTE: Samples must be padded to a multiple of 4 samples!
	int16 *Samples;
} game_sound_output_buffer;

typedef struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
} game_button_state;

typedef struct game_controller_input
{
	bool32 IsConnected;
	bool32 IsAnalog;
	real32 StickAverageX;
	real32 StickAverageY;

	union
	{
		game_button_state Buttons[12];
		struct
		{
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;

			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;

			game_button_state LeftShoulder;
			game_button_state RightShoulder;

			game_button_state Back;
			game_button_state Start;

			// NOTE: All buttons must be added above this line
			game_button_state Terminator;
		};
	};
} game_controller_input;

typedef struct game_input
{
	game_button_state MouseButton[5];
	int32 MouseX, MouseY, MouseZ;

	bool32 ExecutableReloaded;
	real32 dtForFrame;

	game_controller_input Controllers[5];
} game_input;

typedef struct platform_file_handle
{
	b32 NoErrors;
	void *Platform;
} platform_file_handle;

typedef struct paltform_file_group
{
	u32 FileCount;
	void *Platform;
} platform_file_group;

typedef enum platform_file_type
{
	PlatformFileType_AssetFile,
	PlatformFileType_SaveGameFile,
	
	PlatformFileType_Count
} platform_file_type;

#define	PLATFORM_GET_ALL_FILE_OF_TYPES_BEGIN(name) platform_file_group name(platform_file_type Type) 
typedef PLATFORM_GET_ALL_FILE_OF_TYPES_BEGIN(platform_get_all_files_of_type_begin);

#define	PLATFORM_GET_ALL_FILE_OF_TYPES_END(name) void name(platform_file_group *FileGroup) 
typedef PLATFORM_GET_ALL_FILE_OF_TYPES_END(platform_get_all_files_of_type_end);

#define	PLATFORM_OPEN_NEXT_FILE(name) platform_file_handle name(platform_file_group *FileGroup) 
typedef PLATFORM_OPEN_NEXT_FILE(platform_open_next_file);

#define	PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest) 
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define	PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message) 
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors) 

struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

#define PLATFORM_ALLOCATE_MEMORY(name) void *name(memory_index Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);
typedef struct platform_api
{	
	platform_add_entry *AddEntry;
	platform_complete_all_work *CompleteAllWork;

	platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
	platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
	platform_open_next_file *OpenNextFile;
	platform_read_data_from_file *ReadDataFromFile;
	platform_file_error *FileError;

	platform_allocate_memory *AllocateMemory;
	platform_deallocate_memory *DeallocateMemory;

	debug_platform_free_file_memory *DEBUGFreeFileMemory;
	debug_platform_read_entire_file *DEBUGReadEntireFile;
	debug_platform_write_entire_file *DEBUGWriteEntireFile;
} platform_api;
typedef struct game_memory
{
	uint64 PermanentStorageSize;
	void *PermanentStorage; // NOTE: REQUIRED to be cleared ro zero at startup

	uint64 TransientStorageSize;
	void *TransientStorage;

	platform_work_queue *HighPriorityQueue;
	platform_work_queue *LowPriorityQueue;

	platform_api PlatformAPI;

#if HANDMADE_INTERNAL
	debug_cycle_counter Counters[DebugCycleCounter_Count];
#endif
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE: At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));

	game_controller_input *Result = &Input->Controllers[ControllerIndex];
	return(Result);
}

#ifdef __cplusplus
}
#endif

#define HANDMADE_PLATFORM_H
#endif