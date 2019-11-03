/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "time_util.h"

#include "ntdll.h"


//=============================================================================
// Constants
//=============================================================================
#define SECOND_IN_MICROSECONDS 1000000


//=============================================================================
// Private Types
//=============================================================================
typedef struct _TIME_CONTEXT {
    LARGE_INTEGER PerformanceFrequency;
} TIME_CONTEXT, *PTIME_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
static TIME_CONTEXT g_TimeContext = {};


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
BOOL
TmuInitialization()
{
    LARGE_INTEGER PerformanceFrequency = {};
    LARGE_INTEGER PerformanceCounter = {};
    BOOL status = TRUE;

    status = QueryPerformanceFrequency(&PerformanceFrequency);
    if (!status)
    {
        goto exit;
    }

    //
    // Verify that we can query the performance counter.
    //
    status = QueryPerformanceCounter(&PerformanceCounter);
    if (!status)
    {
        goto exit;
    }

    //
    // Initialize the global context.
    //
    g_TimeContext.PerformanceFrequency = PerformanceFrequency;

exit:
    return status;
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
LONGLONG
TmuMicrosecondsToTicks(
    LONGLONG Microseconds
)
{
    LONGLONG Ticks = 0;

    Ticks = Microseconds * g_TimeContext.PerformanceFrequency.QuadPart;
    Ticks /= SECOND_IN_MICROSECONDS;

    return Ticks;
}


_Use_decl_annotations_
LONGLONG
TmuTicksToMicroseconds(
    LONGLONG Ticks
)
{
    LONGLONG Microseconds = 0;

    Microseconds = Ticks * SECOND_IN_MICROSECONDS;
    Microseconds /= g_TimeContext.PerformanceFrequency.QuadPart;

    return Microseconds;
}


_Use_decl_annotations_
BOOL
TmuStallExecution(
    ULONG Microseconds
)
/*++

Description:

    Execute a spinloop for a specified time interval.

Parameters:

    Microseconds - The number of microseconds to stall.

Remarks:

    This routine does not prevent the executing thread from being preempted.

--*/
{
    LONGLONG NumberOfTicks = 0;
    LARGE_INTEGER InitialTime = {};
    LONGLONG BreakTick = {};
    LARGE_INTEGER CurrentTime = {};
    BOOL status = TRUE;

    NumberOfTicks = TmuMicrosecondsToTicks(Microseconds);

    if (!QueryPerformanceCounter(&InitialTime))
    {
        status = FALSE;
        goto exit;
    }

    BreakTick = InitialTime.QuadPart + NumberOfTicks;

    if (BreakTick < InitialTime.QuadPart)
    {
        status = FALSE;
        goto exit;
    }

    do
    {
        YieldProcessor();

        if (!QueryPerformanceCounter(&CurrentTime))
        {
            status = FALSE;
            goto exit;
        }

        if (CurrentTime.QuadPart < InitialTime.QuadPart)
        {
            status = FALSE;
            goto exit;
        }
    }
    while (CurrentTime.QuadPart < BreakTick);

exit:
    return status;
}
