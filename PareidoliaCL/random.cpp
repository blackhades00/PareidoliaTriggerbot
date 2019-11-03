/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "random.h"

#include "ntdll.h"


//=============================================================================
// Macros
//=============================================================================
#define SYSTEM_TIME_MONTH_MASK      0x001F
#define SYSTEM_TIME_DAY_MASK        0x000F

#define PERFORMANCE_COUNTER_MASK    0x000000000000000000000000007FFFFF


//=============================================================================
// Private Types
//=============================================================================
#pragma warning(push)
#pragma warning(disable : 4201) // Nonstandard extension: nameless struct/union
typedef union _WEAK_SEED {
    struct {
        ULONG Month     : 5;
        ULONG Day       : 4;
        ULONG TimeStamp : 23;
    };
    ULONG Value;
} WEAK_SEED, *PWEAK_SEED;
#pragma warning(pop)

C_ASSERT(sizeof(WEAK_SEED) == sizeof(ULONG));

typedef struct _RANDOM_CONTEXT {
    ULONG Seed;
} RANDOM_CONTEXT, *PRANDOM_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
static RANDOM_CONTEXT g_RndContext = {};


//=============================================================================
// Private Prototypes
//=============================================================================
_Check_return_
_Success_(return != FALSE)
static
BOOL
RndpGenerateSeedWeak(
    _Out_ PULONG pSeed
);


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
BOOL
RndInitialization()
{
    ULONG Seed = 0;
    BOOL status = TRUE;

    status = RndpGenerateSeedWeak(&Seed);
    if (!status)
    {
        goto exit;
    }

    //
    // Initialize the global context.
    //
    g_RndContext.Seed = Seed;

exit:
    return status;
}


//=============================================================================
// Public Interface
//=============================================================================
ULONG
RndUlong()
{
    return RtlRandomEx(&g_RndContext.Seed);
}


_Use_decl_annotations_
ULONG
RndUlongInRange(
    ULONG Minimum,
    ULONG Maximum
)
{
    ULONG Base = 0;
    ULONG Value = 0;

    Base = RndUlong();
    Value = Base % (Maximum - Minimum + 1) + Minimum;

    return Value;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
static
BOOL
RndpGenerateSeedWeak(
    PULONG pSeed
)
{
    LARGE_INTEGER PerformanceCounter = {};
    SYSTEMTIME SystemTime = {};
    WEAK_SEED Seed = {};
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pSeed = 0;

    status = QueryPerformanceCounter(&PerformanceCounter);
    if (!status)
    {
        goto exit;
    }

    GetSystemTime(&SystemTime);

    Seed.Month = SystemTime.wMonth & SYSTEM_TIME_MONTH_MASK;
    Seed.Day = SystemTime.wDay & SYSTEM_TIME_DAY_MASK;
    Seed.TimeStamp = PerformanceCounter.QuadPart & PERFORMANCE_COUNTER_MASK;

    //
    // Set out parameters.
    //
    *pSeed = Seed.Value;

exit:
    return status;
}
