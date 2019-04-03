#if !defined(HANDMADE_DEBUG_H)

#define TIMED_BLOCK__(Number, ...) timed_block TimedBlock_##Number(__COUNTER__, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ##__VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ##__VA_ARGS__)

struct debug_record
{
    char *FileName;
    char *FunctionName;

    int LineNumber;
    u32 Reserved;

    u64 HitCount_CycleCount;
};

debug_record DebugRecordsArray[];

#define DEBUG_RECORD(Index) (DEBUG_PREFIX##_DebugRecords + Index)

struct timed_block
{
    debug_record *Record;
    u64 StartCycles;
    u32 HitCount;

    timed_block(int Counter, char *FileName, int LineNumber, char *FunctionName, int HitCountInit = 1)
    {
        HitCount = HitCountInit;
        Record = DebugRecordsArray + Counter;
        Record->FileName = FileName;
        Record->LineNumber = LineNumber;
        Record->FunctionName = FunctionName;

        StartCycles = __rdtsc();
    }

    ~timed_block()
    {
        u64 Delta = (__rdtsc() - StartCycles) | ((u64)HitCount << 32);
        AtomicAddU64(&Record->HitCount_CycleCount, Delta);
    }
};

#define HANDMADE_DEBUG_H
#endif