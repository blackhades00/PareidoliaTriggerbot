/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "triggerbot.h"

#include "debug.h"
#include "driver.h"
#include "keyboard.h"
#include "log.h"
#include "memory_util.h"
#include "ntdll.h"
#include "random.h"
#include "time_util.h"

#include "../Common/config.h"

#include "../MouClassInputInjection/MouiiCL/mouse_input_injection.h"

#include "../VivienneVMM/common/arch_x64.h"
#include "../VivienneVMM/common/driver_io_types.h"

#include "../VivienneVMM/VivienneCL/driver_io.h"


//=============================================================================
// Constants
//=============================================================================
/*
    The minimum and maximum duration in milliseconds for the new round CECR
    request.
*/
#define NEW_ROUND_REQUEST_DURATION_MIN_MS  50
#define NEW_ROUND_REQUEST_DURATION_MAX_MS  10000

/*
    The new round CECR request buffer size.
*/
#define NUMBER_OF_PLAYERS_MAX               12

#define NEW_ROUND_REQUEST_BUFFER_SIZE \
    (UFIELD_OFFSET(CEC_REGISTER_VALUES, Values[NUMBER_OF_PLAYERS_MAX]))

/*
    The index of the debug address register used by the new round CECR request.
*/
#define NEW_ROUND_REQUEST_DEBUG_ADDRESS_REGISTER 1

C_ASSERT(
    0 <= NEW_ROUND_REQUEST_DEBUG_ADDRESS_REGISTER &&
    NEW_ROUND_REQUEST_DEBUG_ADDRESS_REGISTER <= DAR_COUNT);


//=============================================================================
// Private Types
//=============================================================================
/*++

Name:

    GAME_ROUND_CONTEXT

Description:

    The trace state information for the local player.

Parameters:

    TraceStateAddress - The virtual address of the local player's trace state
        variable in the target (remote) Overwatch process.

    NumberOfTriggerEvents - The number of trigger activation events for the
        game round.

Remarks:

    The game round context must be initialized each time the local player
    spawns as the Widowmaker hero for the first time since their last hero
    swap. The following scenarios are examples of when the user must initialize
    a new context:

        - The player joins a ranked matchmaking game and selects the Widowmaker
            hero during 'Setup Time'. The context should be initialized when
            the player spawns after the setup time expires.

        - The player swaps from the McCree hero to the Widowmaker hero. The
            context should be initialized when the player spawns as the
            Widowmaker hero.

    The context remains valid until the local player leaves the match, spawns
    as a non-Widowmaker hero, or changes teams.

--*/
typedef struct _GAME_ROUND_CONTEXT {
    ULONG_PTR TraceStateAddress;
    SIZE_T NumberOfTriggerEvents;
} GAME_ROUND_CONTEXT, *PGAME_ROUND_CONTEXT;

typedef struct _TRIGGERBOT_CONTEXT {
    OVERWATCH_CONTEXT OverwatchContext;
    ULONG NewRoundRequestDuration;
    BOOL Active;
    GAME_ROUND_CONTEXT RoundContext;
} TRIGGERBOT_CONTEXT, *PTRIGGERBOT_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
static TRIGGERBOT_CONTEXT g_Triggerbot = {};


//=============================================================================
// Private Prototypes
//=============================================================================
static
VOID
TrgpAdjustNewRoundRequestDuration(
    _In_ BOOL fIncreaseDuration
);

_Check_return_
_Success_(return != FALSE)
static
BOOL
TrgpInitializeNewRound();

static
VOID
TrgpResetRound();

_Check_return_
_Success_(return != FALSE)
static
BOOL
TrgpToggle();

_Check_return_
_Success_(return != FALSE)
static
BOOL
TrgpReadTraceState(
    _Out_ PWIDOWMAKER_TRACE_STATE pTraceState
);

_Check_return_
_Success_(return != FALSE)
static
BOOL
TrgpActivateTrigger();


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
VOID
TrgInitialization(
    POVERWATCH_CONTEXT pOverwatchContext
)
/*++

Description:

    Initialize the global triggerbot context for the target Overwatch context.

Parameters:

    pOverwatchContext - The target Overwatch context.

Remarks:

    The triggerbot context is valid until the target Overwatch process
    terminates.

--*/
{
    RtlCopyMemory(
        &g_Triggerbot.OverwatchContext,
        pOverwatchContext,
        sizeof(OVERWATCH_CONTEXT));

    g_Triggerbot.NewRoundRequestDuration =
        CFG_TB_NEW_ROUND_REQUEST_DURATION_MS;
}


//=============================================================================
// Public Interface
//=============================================================================
VOID
TrgTickLoop()
/*++

Description:

    The triggerbot tick loop.

Remarks:

    The tick loop executes until the user presses the CFG_KEY_EXIT_CLIENT
    keybind.

    The tick loop is designed to enforce the following states:

        Valid States:

            - TraceStateAddress valid and Triggerbot active.
            - TraceStateAddress valid and Triggerbot inactive.
            - TraceStateAddress invalid and Triggerbot inactive.

        Invalid States:

            - TraceStateAddress invalid and Triggerbot active.

    The following diagram depicts a simplified overview of the tick loop:

                        .-----------------------.
                        |                       |
                        v                       |
        +===============================+       |
        |      Process user input       |       |
        +===============================+       |
                        |                       |
                        v                       |
        .-------------------------------.       |
        |  Is the round context valid?  |--No-->+
        '-------------------------------'       |
                        |                       |
                       Yes                      |
                        |                       |
                        v                       |
        .-------------------------------.       |
        |  Is the triggerbot enabled?   |--No-->+
        '-------------------------------'       |
                        |                       |
                       Yes                      |
                        |                       |
                        v                       |
        +===============================+       |
        |  Read Widowmaker Trace State  |       |
        +===============================+       |
                        |                       |
                        v                       |
        .-------------------------------.       |
        |   Is the player's crosshair   |--No-->+
        |     over an enemy player?     |       |
        '-------------------------------'       |
                        |                       |
                       Yes                      |
                        |                       |
                        v                       |
        +===============================+       |
        |       Activate trigger        |       |
        +===============================+       |
                        |                       |
                        v                       |
        +===============================+       |
        |       Trigger cooldown        |       |
        +===============================+       |
                        |                       |
                        '-----------------------'

--*/
{
    WIDOWMAKER_TRACE_STATE TraceState = 0;

    INF_PRINT("Starting TB.");

    for (;;)
    {
        _ASSERTE(!(
            g_Triggerbot.Active &&
            !g_Triggerbot.RoundContext.TraceStateAddress));

        //
        // Choose the sleep duration based on the active state so that we use
        //  less cycles when the triggerbot is inactive.
        //
        if (g_Triggerbot.Active)
        {
            Sleep(CFG_TB_TICK_LOOP_INTERVAL_ACTIVE_MS);
        }
        else
        {
            Sleep(CFG_TB_TICK_LOOP_INTERVAL_INACTIVE_MS);
        }

        //
        // Process user input.
        //
        if (IS_KEY_DOWN_ASYNC(CFG_KEY_EXIT_CLIENT))
        {
            INF_PRINT("Exiting TB.");
            goto exit;
        }

        if (IS_KEY_DOWN_ASYNC(CFG_KEY_DECREASE_NEW_ROUND_REQUEST_DURATION))
        {
            TrgpAdjustNewRoundRequestDuration(FALSE);
        }

        if (IS_KEY_DOWN_ASYNC(CFG_KEY_INCREASE_NEW_ROUND_REQUEST_DURATION))
        {
            TrgpAdjustNewRoundRequestDuration(TRUE);
        }

        if (IS_KEY_DOWN_ASYNC(CFG_KEY_INITIALIZE_NEW_ROUND))
        {
            if (!TrgpInitializeNewRound())
            {
                WRN_PRINT("Failed to initialize a new round context.");
                continue;
            }
        }

        if (IS_KEY_DOWN_ASYNC(CFG_KEY_TOGGLE_TRIGGERBOT))
        {
            if (!TrgpToggle())
            {
                continue;
            }
        }

        //
        // End this iteration if the round context is invalid.
        //
        if (!g_Triggerbot.RoundContext.TraceStateAddress)
        {
            continue;
        }

        //
        // End this iteration if the triggerbot is inactive.
        //
        if (!g_Triggerbot.Active)
        {
            continue;
        }

        //
        // Determine if we should trigger.
        //
        if (!TrgpReadTraceState(&TraceState))
        {
            //
            // If we fail to read the trace state then the user most likely
            //  exited the match. Reset the round context and end this
            //  iteration.
            //
            TrgpResetRound();

            continue;
        }

        if (!OwIsTracingEnemyPlayerEntity(TraceState))
        {
            continue;
        }

        //
        // Trigger.
        //
        if (!TrgpActivateTrigger())
        {
            ERR_PRINT("Activate trigger failed. Exiting.");
            goto exit;
        }

#if defined(CFG_TB_PRINT_TRACE_STATE_ON_TRIGGER_ACTIVATION)
        //
        // NOTE We print the trace state value after the trigger activation
        //  event so that the print routine does not delay the event.
        //
        INF_PRINT("Trigger activated. (TraceState = 0x%08X)", TraceState);
#endif
    }

exit:
    return;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
static
VOID
TrgpAdjustNewRoundRequestDuration(
    BOOL fIncreaseDuration
)
/*++

Description:

    Adjust the duration of the CECR request used to initialize a new round
    context.

Parameters:

    fIncreaseDuration - Specifies whether to increase or decrease the duration.

Remarks:

    The duration is incremented or decremented by the step value. See the
    CFG_TB_NEW_ROUND_REQUEST_DURATION_STEP_MS config setting.
    
    The duration is clamped to the range:

        [NEW_ROUND_REQUEST_DURATION_MIN_MS, NEW_ROUND_REQUEST_DURATION_MAX_MS]

--*/
{
    ULONG PreviousDuration = 0;
    ULONG NewDuration = 0;

    PreviousDuration = g_Triggerbot.NewRoundRequestDuration;

    //
    // Calculate and clamp the new duration.
    //
    if (fIncreaseDuration)
    {
        NewDuration =
            g_Triggerbot.NewRoundRequestDuration +
            CFG_TB_NEW_ROUND_REQUEST_DURATION_STEP_MS;

        if (NEW_ROUND_REQUEST_DURATION_MAX_MS < NewDuration ||
            NewDuration < PreviousDuration)
        {
            NewDuration = NEW_ROUND_REQUEST_DURATION_MAX_MS;
        }
    }
    else
    {
        NewDuration =
            g_Triggerbot.NewRoundRequestDuration -
            CFG_TB_NEW_ROUND_REQUEST_DURATION_STEP_MS;

        if (NEW_ROUND_REQUEST_DURATION_MIN_MS > NewDuration ||
            NewDuration > PreviousDuration)
        {
            NewDuration = NEW_ROUND_REQUEST_DURATION_MIN_MS;
        }
    }

    _ASSERTE(NEW_ROUND_REQUEST_DURATION_MIN_MS <= NewDuration);
    _ASSERTE(NEW_ROUND_REQUEST_DURATION_MAX_MS >= NewDuration);

    //
    // Update the global triggerbot context.
    //
    g_Triggerbot.NewRoundRequestDuration = NewDuration;

    if (g_Triggerbot.NewRoundRequestDuration != PreviousDuration)
    {
        INF_PRINT("New round request duration: %u ms (Previous %u)",
            g_Triggerbot.NewRoundRequestDuration,
            PreviousDuration);
    }
    else
    {
        INF_PRINT("Failed to update new round request duration. (Limit: %u)",
            g_Triggerbot.NewRoundRequestDuration);
    }
}


_Use_decl_annotations_
static
BOOL
TrgpInitializeNewRound()
/*++

Description:

    Attempt to resolve the remote virtual address of the local player's trace
    state variable by issuing a Vivienne CECR request for the trace state
    instruction.

Remarks:

    NOTE The user must press their Overwatch 'zoom' keybind to exercise the
    trace state instruction while the request is active.

    NOTE The CECR request may return zero, one, or multiple values:
    
        If the request returns zero values then the user missed the execution
        timing window.
        
        If the request returns more than one value then an unknown event caused
        the instruction to be executed for two different trace state objects.
        This may occur when there are multiple Widowmaker players in a match.
        In this scenario the user should wait a few seconds then try to
        initialize a new round context. See the
        CFG_TB_NEW_ROUND_REQUEST_DURATION_MS config setting.

        If the request returns one value then the returned value is most likely
        the remote virtual address of the local player's trace state object.
        The user should verify this by enabling the triggerbot and aiming their
        crosshair over a dynamic, non-player entity (e.g., the payload, a
        closed spawn door, or the lid of a trash can). The triggerbot should
        trigger as soon as the local player's Widow's Kiss is fully charged. If
        the triggerbot does not trigger or behaves erratically then the user
        should initialize a new round context.

--*/
{
    PCEC_REGISTER_VALUES pRegisterValues = NULL;
    ULONG cbRegisterValues = 0;
    BOOL status = TRUE;

    //
    // Reset the current round context and disable the triggerbot.
    //
    TrgpResetRound();

    INF_PRINT("Initializing a new round context.");

    //
    // Allocate the captured context buffer.
    //
    cbRegisterValues = NEW_ROUND_REQUEST_BUFFER_SIZE;

    pRegisterValues = (PCEC_REGISTER_VALUES)MemAllocateHeap(cbRegisterValues);
    if (!pRegisterValues)
    {
        ERR_PRINT("MemAllocateHeap failed.");
        status = FALSE;
        goto exit;
    }

    INF_PRINT("Issuing CECR request. (Duration = %u ms)",
        g_Triggerbot.NewRoundRequestDuration);

    //
    // Issue a CECR request for the remote trace state instruction.
    //
    status = VivienneIoCaptureRegisterValues(
        g_Triggerbot.OverwatchContext.ProcessId,
        NEW_ROUND_REQUEST_DEBUG_ADDRESS_REGISTER,
        g_Triggerbot.OverwatchContext.TraceStateInstruction.Address,
        HWBP_TYPE::Execute,
        HWBP_SIZE::Byte,
        g_Triggerbot.OverwatchContext.TraceStateInstruction.Register,
        g_Triggerbot.NewRoundRequestDuration,
        pRegisterValues,
        cbRegisterValues);
    if (!status)
    {
        ERR_PRINT(
            "[CECR FAILURE A] VivienneIoCaptureRegisterValues failed: %u",
            GetLastError());
        goto exit;
    }
    //
    if (!pRegisterValues->NumberOfValues)
    {
        ERR_PRINT("[CECR FAILURE B] Failed to find a trace state address.");
        status = FALSE;
        goto exit;
    }
    //
    if (1 != pRegisterValues->NumberOfValues)
    {
        ERR_PRINT(
            "[CECR FAILURE C] Found multiple trace state addresses."
            " (Found %u)",
            pRegisterValues->NumberOfValues);
        status = FALSE;
        goto exit;
    }

    //
    // Update the round context.
    //
    g_Triggerbot.RoundContext.TraceStateAddress =
        pRegisterValues->Values[0] +
        g_Triggerbot.OverwatchContext.TraceStateInstruction.Displacement;

    DBG_PRINT("TraceStateAddress: %p",
        g_Triggerbot.RoundContext.TraceStateAddress);

    INF_PRINT("[CECR SUCCESS] Round initialized. (Ready to trigger)");

exit:
    if (pRegisterValues)
    {
        VERIFY(MemFreeHeap(pRegisterValues));
    }

    return status;
}


static
VOID
TrgpResetRound()
/*++

Description:

    Reset the round context and disable the triggerbot.

--*/
{
    INF_PRINT("Resetting round context. (NumberOfTriggerEvents = %Iu)",
        g_Triggerbot.RoundContext.NumberOfTriggerEvents);

    RtlSecureZeroMemory(
        &g_Triggerbot.RoundContext,
        sizeof(g_Triggerbot.RoundContext));

    //
    // Disable the triggerbot.
    //
    if (g_Triggerbot.Active)
    {
        g_Triggerbot.Active = FALSE;

        INF_PRINT("TB: Inactive");
    }
}


_Use_decl_annotations_
static
BOOL
TrgpToggle()
/*++

Description:

    Toggle the triggerbot active state.

--*/
{
    BOOL status = TRUE;

    //
    // Prevent the user from enabling the triggerbot if the round context is
    //  invalid.
    //
    if (!g_Triggerbot.RoundContext.TraceStateAddress)
    {
        WRN_PRINT("Initialize a new round context before enabling TB.");
        status = FALSE;
        goto exit;
    }

    //
    // Update the triggerbot active state.
    //
    g_Triggerbot.Active = !g_Triggerbot.Active;

    INF_PRINT("TB: %s", g_Triggerbot.Active ? "Enabled" : "Disabled");

exit:
    return status;
}


_Use_decl_annotations_
static
BOOL
TrgpReadTraceState(
    PWIDOWMAKER_TRACE_STATE pTraceState
)
/*++

Description:

    Read the ULONG value at the remote virtual address of the local player's
    trace state variable.

Parameters:

    pTraceState - Returns the trace state value.

Remarks:

    On failure, the caller must invalidate the current round context.

--*/
{
    WIDOWMAKER_TRACE_STATE TraceState = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pTraceState = 0;

    status = PareidoliaIoReadVirtualMemory(
        g_Triggerbot.OverwatchContext.ProcessId,
        g_Triggerbot.RoundContext.TraceStateAddress,
        &TraceState,
        sizeof(TraceState));
    if (!status)
    {
        ERR_PRINT("PareidoliaIoReadVirtualMemory failed: %u (Address = %p)",
            GetLastError(),
            g_Triggerbot.RoundContext.TraceStateAddress);
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pTraceState = TraceState;

exit:
    return status;
}


_Use_decl_annotations_
static
BOOL
TrgpActivateTrigger()
/*++

Description:

    Execute a trigger activation event by injecting a left mouse button click
    and release.

Remarks:

    If this routine fails then the client or driver has encountered a critical
    error, and the caller must exit the tick loop.

--*/
{
#if defined(CFG_TB_PRINT_TRIGGER_TICK_DATA)
    LARGE_INTEGER ClickTime = {};
    LARGE_INTEGER ReleaseTime = {};
    LARGE_INTEGER FinishTime = {};
#endif
    ULONG ReleaseDelay = 0;
    ULONG CooldownDelay = 0;
    BOOL status = TRUE;

#if defined(CFG_TB_PRINT_TRIGGER_TICK_DATA)
    QueryPerformanceCounter(&ClickTime);
#endif

#if !defined(CFG_TB_DISABLE_INPUT_INJECTION)
    //
    // We inject the left mouse button click immediately to reduce the latency
    //  between when we determine that we should trigger and when the trigger
    //  activation event occurs.
    //
    status = MouInjectButtonInput(
        g_Triggerbot.OverwatchContext.ProcessId,
        MOUSE_LEFT_BUTTON_DOWN,
        0);
    if (!status)
    {
        ERR_PRINT("MouInjectButtonInput failed: %u", GetLastError());
        goto exit;
    }
#endif

#if defined(CFG_TB_PRINT_TRIGGER_TICK_DATA)
    QueryPerformanceCounter(&ClickTime);
#endif

    //
    // Generate the duration of the release delay.
    //
    ReleaseDelay = RndUlongInRange(
        CFG_TB_TRIGGER_RELEASE_DELAY_MIN_US,
        CFG_TB_TRIGGER_RELEASE_DELAY_MAX_US);

    //
    // Stall execution until the release delay expires.
    //
    status = TmuStallExecution(ReleaseDelay);
    if (!status)
    {
        ERR_PRINT("TmuStallExecution failed. (Release)");
        goto exit;
    }

#if defined(CFG_TB_PRINT_TRIGGER_TICK_DATA)
    QueryPerformanceCounter(&ReleaseTime);
#endif

#if !defined(CFG_TB_DISABLE_INPUT_INJECTION)
    //
    // Inject the left mouse button release.
    //
    status = MouInjectButtonInput(
        g_Triggerbot.OverwatchContext.ProcessId,
        MOUSE_LEFT_BUTTON_UP,
        0);
    if (!status)
    {
        ERR_PRINT("MouInjectButtonInput failed: %u", GetLastError());
        goto exit;
    }
#endif

#if defined(CFG_TB_PRINT_TRIGGER_TICK_DATA)
    QueryPerformanceCounter(&ReleaseTime);
#endif

    //
    // Generate the duration of the cooldown delay.
    //
    CooldownDelay = RndUlongInRange(
        CFG_TB_TRIGGER_COOLDOWN_DELAY_MIN_US,
        CFG_TB_TRIGGER_COOLDOWN_DELAY_MAX_US);

    //
    // Stall execution until the cooldown delay expires.
    //
    status = TmuStallExecution(CooldownDelay);
    if (!status)
    {
        ERR_PRINT("TmuStallExecution failed. (Cooldown)");
        goto exit;
    }

#if defined(CFG_TB_PRINT_TRIGGER_TICK_DATA)
    QueryPerformanceCounter(&FinishTime);

    //
    // NOTE We print debug tick times at the end of the function so that the
    //  print routines do not impact the trigger timing window.
    //
    INF_PRINT("Trigger activated:");
    INF_PRINT("    Click     %016I64X", ClickTime.QuadPart);
    INF_PRINT("    Release   %016I64X  (%016I64Xus)",
        ReleaseTime.QuadPart,
        TmuTicksToMicroseconds(ReleaseTime.QuadPart - ClickTime.QuadPart));
    INF_PRINT("    Cooldown  %016I64X  (%016I64Xus)",
        FinishTime.QuadPart,
        TmuTicksToMicroseconds(FinishTime.QuadPart - ReleaseTime.QuadPart));
    INF_PRINT("    Total     %016I64X  (%016I64Xus)",
        FinishTime.QuadPart - ClickTime.QuadPart,
        TmuTicksToMicroseconds(FinishTime.QuadPart - ClickTime.QuadPart));
#endif

    //
    // Update the current round statistics.
    //
    g_Triggerbot.RoundContext.NumberOfTriggerEvents++;

exit:
    return status;
}
