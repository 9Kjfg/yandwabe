#if !defined(HANDMADE_SHARED_H)

#include "handmade_intrinsics.h"
#include "handmade_math.h"

inline b32
IsEndOfLine(char C)
{
    b32 Result = (C == '\n') || (C == '\r');
    return(Result);
}

inline b32
IsWhitespace(char C)
{
    b32 Result = ((C == ' ') || (C == '\t') || (C == '\v') || (C == '\f') ||IsEndOfLine(C));
    return(Result);
}

inline b32
StringAreEqual(char *A, char *B)
{
	b32 Result = (A == B);

	if (A && B)
	{
		while (*A && *B && (*A == *B))
		{
			++A;
			++B;
		}
		
		Result = ((*A == 0) && (*B == 0));
	}

	return(Result);
}

inline b32
StringAreEqual(memory_index ALength, char *A, char *B)
{
    char *At = B;
    for (umm Index = 0;
        Index < ALength;
        ++Index, ++At)
    {
        if ((*At == 0) || (A[Index] != *At))
        {
            return(false);
        }
    }

    b32 Result = (*At == 0);
    return(Result);
}

inline b32
StringAreEqual(memory_index ALength, char *A, memory_index BLength, char *B)
{
	b32 Result = (ALength == BLength);

	if (Result)
	{
		for (u32 Index = 0;
			Index < ALength;
			++Index)
		{
			if (A[Index] != B[Index])
			{
				Result = false;
				break;
			}
		}
	}

	return(Result);
}

#define HANDMADE_SHARED_H
#endif