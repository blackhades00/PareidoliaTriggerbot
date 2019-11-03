/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#if defined(_KERNEL_MODE)
#include <fltKernel.h>
#else
#include <Windows.h>
#endif

#include "../VivienneVMM/common/arch_x64.h"

//=============================================================================
// Names
//=============================================================================
/*
    The symbolic link names for the DEVICE_OBJECTs created by the driver.

    NOTE Users should use unique names because these symbolic links can be used
    as a detection vector by anti-cheat software.
*/
#define CFG_DEVICE_NAME_PAREIDOLIA_TRIGGERBOT_U         L"pareidolia"
#define CFG_DEVICE_NAME_MOUCLASS_INPUT_INJECTION_U      L"mouii"
#define CFG_DEVICE_NAME_VIVIENNE_VMM_U                  L"vivienne"

/*
    The VivienneVMM ULONG-sized signature returned by cpuid.

    NOTE Users should use a unique name because this value can be used as a
    detection vector by anti-cheat software.
*/
#define CFG_VIVIENNE_VMM_SIGNATURE                      ((ULONG)'prdl')

//=============================================================================
// Access Protection
//=============================================================================
/*
    Prevent processes from opening user mode handles to the client process.
*/
#define CFG_ENABLE_PROCESS_ACCESS_PROTECTION

#if defined(_KERNEL_MODE)
//=============================================================================
// Log
//=============================================================================
/*
    The NT file path used for the VivienneVMM log file.

    NOTE Users should use a unique name because this string can be used as a
    detection vector by anti-cheat software.
*/
#define CFG_VIVIENNE_VMM_LOG_FILE_NT_PATH_U \
    (L"\\SystemRoot\\" CFG_DEVICE_NAME_PAREIDOLIA_TRIGGERBOT_U ".log")

//=============================================================================
// Stealth
//=============================================================================
/*
    Unlink the PareidoliaTriggerbot DRIVER_OBJECT from nt!PsLoadedModuleList so
    that the driver does not appear in the loaded driver list.

    WARNING This option is only supported on Windows 7. nt!PsLoadedModuleList
    is protected by PatchGuard on Windows 8+.
*/
#define CFG_UNLINK_DRIVER_OBJECT

#else
//=============================================================================
// Keybinds
//=============================================================================
/*
    Exit the client.
*/
#define CFG_KEY_EXIT_CLIENT                             (VK_F11)
#define CFG_KEY_EXIT_CLIENT_TEXT                        "F11"

/*
    Get the Overwatch context for a fully loaded Overwatch process.
*/
#define CFG_KEY_GET_OVERWATCH_CONTEXT                   (VK_RETURN)
#define CFG_KEY_GET_OVERWATCH_CONTEXT_TEXT              "ENTER"

/*
    Initialize a new triggerbot round context.

    This keybind issues a VivienneVMM Capture Execution Context Register (CECR)
    request to obtain the player's trace state address. The player must be
    scoped as the Widowmaker class while the request is active.
*/
#define CFG_KEY_INITIALIZE_NEW_ROUND                    (VK_F5)

/*
    Decrease / increase the duration of the CECR request when initializing a
    new triggerbot round context.
*/
#define CFG_KEY_DECREASE_NEW_ROUND_REQUEST_DURATION     (VK_F8)
#define CFG_KEY_INCREASE_NEW_ROUND_REQUEST_DURATION     (VK_F9)

/*
    Enable / disable the triggerbot.
*/
#define CFG_KEY_TOGGLE_TRIGGERBOT                       (VK_MBUTTON)

//=============================================================================
// Client
//=============================================================================
/*
    The duration in milliseconds between active process queries during client
    initialization.
*/
#define CFG_PROCESS_QUERY_INTERVAL_MS                   3000

//=============================================================================
// Overwatch
//=============================================================================
/*
    Use hard-coded values for the 'TRACE_STATE_INSTRUCTION' object instead of
    resolving it at runtime.
*/
///#define CFG_USE_FIXED_TRACE_STATE_INSTRUCTION

#if defined(CFG_USE_FIXED_TRACE_STATE_INSTRUCTION)
#define CFG_TRACE_STATE_INSTRUCTION_RELATIVE_ADDRESS    0x00B8C6F3
#define CFG_TRACE_STATE_INSTRUCTION_REGISTER            REGISTER_RDI
#define CFG_TRACE_STATE_INSTRUCTION_DISPLACEMENT        0x000001FC
#endif

//=============================================================================
// Triggerbot
//=============================================================================
/*
    The duration in milliseconds between tick loop iterations when the
    triggerbot is active.
*/
#define CFG_TB_TICK_LOOP_INTERVAL_ACTIVE_MS             1

/*
    The duration in milliseconds between tick loop iterations when the
    triggerbot is inactive.
*/
#define CFG_TB_TICK_LOOP_INTERVAL_INACTIVE_MS           1000

/*
    The default duration in milliseconds of the CECR request when initializing
    a new triggerbot round context. This value can be dynamically adjusted
    using the CFG_KEY_DECREASE_NEW_ROUND_REQUEST_DURATION and
    CFG_KEY_INCREASE_NEW_ROUND_REQUEST_DURATION keybinds.

    NOTE This value is bounded by the range: [50, 10000]
*/
#define CFG_TB_NEW_ROUND_REQUEST_DURATION_MS            2000

/*
    The amount of time in milliseconds added to or subtracted from the duration
    of the new round CECR request.

    See: CFG_KEY_DECREASE_NEW_ROUND_REQUEST_DURATION
*/
#define CFG_TB_NEW_ROUND_REQUEST_DURATION_STEP_MS       1000

/*
    The range of time in microseconds for the randomly generated delay between
    trigger activation and trigger release.

    NOTE These default time values were obtained by observing the average delay
    between mouse button click and release. Users should update these values to
    reflect the average delay for their mouse.

        See: https://github.com/changeofpace/MouHidInputHook
*/
#define CFG_TB_TRIGGER_RELEASE_DELAY_MIN_US             32000
#define CFG_TB_TRIGGER_RELEASE_DELAY_MAX_US             55000

C_ASSERT(CFG_TB_TRIGGER_RELEASE_DELAY_MIN_US < ULONG_MAX);
C_ASSERT(CFG_TB_TRIGGER_RELEASE_DELAY_MAX_US < ULONG_MAX);

/*
    The range of time in microseconds for the randomly generated delay after
    trigger release.
*/
#define CFG_TB_TRIGGER_COOLDOWN_DELAY_MIN_US            550000
#define CFG_TB_TRIGGER_COOLDOWN_DELAY_MAX_US            750000

C_ASSERT(CFG_TB_TRIGGER_COOLDOWN_DELAY_MIN_US < ULONG_MAX);
C_ASSERT(CFG_TB_TRIGGER_COOLDOWN_DELAY_MAX_US < ULONG_MAX);

/*
    Print the time statistics for each trigger activation.
*/
///#define CFG_TB_PRINT_TRIGGER_TICK_DATA

/*
    Disable input injection during trigger activation.

    NOTE This option is intended to be used with CFG_TB_PRINT_TRIGGER_TICK_DATA
    to debug the trigger release and trigger cooldown settings.
*/
///#define CFG_TB_DISABLE_INPUT_INJECTION

/*
    Print the value of the local player's trace state variable for each trigger
    activation event.
*/
///#define CFG_TB_PRINT_TRACE_STATE_ON_TRIGGER_ACTIVATION

#endif
