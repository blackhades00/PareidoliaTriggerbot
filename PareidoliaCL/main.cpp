/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include <Windows.h>

#include "console.h"
#include "debug.h"
#include "driver.h"
#include "keyboard.h"
#include "log.h"
#include "ntdll.h"
#include "overwatch.h"
#include "process_util.h"
#include "random.h"
#include "time_util.h"
#include "triggerbot.h"

#include "../Common/config.h"

#include "../MouClassInputInjection/MouiiCL/driver.h"
#include "../MouClassInputInjection/MouiiCL/mouse_input_injection.h"

#include "../VivienneVMM/VivienneCL/driver_io.h"


//=============================================================================
// Private Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
static
BOOL
WaitForOverwatchProcess(
    _Out_ POVERWATCH_CONTEXT pOverwatchContext
)
/*++

Description:

    This routine performs the following actions:

        1. Block the calling thread until a single, active Overwatch process
            is detected.

        2. Wait for the user to press the CFG_KEY_GET_OVERWATCH_CONTEXT
            keybind. The user should press this key once the Overwatch process
            has loaded the main menu to guarantee that the process has finished
            unpacking itself.

        3. Create the Overwatch context for the target Overwatch process.

Parameters:

    pOverwatchContext - Returns the Overwatch context for the target Overwatch
        process.

Remarks:

    The user can exit the wait loops by pressing the CFG_KEY_EXIT_CLIENT
    keybind.

--*/
{
    ULONG_PTR ProcessId = 0;
    OVERWATCH_CONTEXT OverwatchContext = {};
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(pOverwatchContext, sizeof(*pOverwatchContext));

    INF_PRINT("Waiting for Overwatch process. (Press %s to exit client)",
        CFG_KEY_EXIT_CLIENT_TEXT);

    //
    // Search the active process list until a single Overwatch process is
    //  detected or the user presses the CFG_KEY_EXIT_CLIENT keybind.
    //
    for (;;)
    {
        status =
            PsuLookupProcessIdByName(OVERWATCH_PROCESS_NAME_U, &ProcessId);
        if (status)
        {
            break;
        }

        if (IS_KEY_DOWN_ASYNC(CFG_KEY_EXIT_CLIENT))
        {
            INF_PRINT("Exiting client.");
            status = FALSE;
            goto exit;
        }

        Sleep(CFG_PROCESS_QUERY_INTERVAL_MS);
    }

    INF_PRINT("Found Overwatch process. (ProcessId = %Iu)", ProcessId);

    INF_PRINT("Press %s when Overwatch is at the main menu...",
        CFG_KEY_GET_OVERWATCH_CONTEXT_TEXT);

    status = ConWaitForInputKey(CFG_KEY_GET_OVERWATCH_CONTEXT);
    if (!status)
    {
        ERR_PRINT("ConWaitForInputKey failed.");
        goto exit;
    }

    status = OwGetOverwatchContext(ProcessId, &OverwatchContext);
    if (!status)
    {
        ERR_PRINT("OwGetOverwatchContext failed.");
        goto exit;
    }

    //
    // Set out parameters.
    //
    RtlCopyMemory(
        pOverwatchContext,
        &OverwatchContext,
        sizeof(OVERWATCH_CONTEXT));

exit:
    return status;
}


//=============================================================================
// Meta Interface
//=============================================================================
int
wmain(
    _In_ int argc,
    _In_ wchar_t* argv[]
)
{
    BOOL fConsoleInitialized = FALSE;
    BOOL fVivienneIoInitialized = FALSE;
    BOOL fPareidoliaIoInitialized = FALSE;
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    BOOL fProcessAccessRestricted = FALSE;
#endif
    BOOL fMouiiIoInitialized = FALSE;
    OVERWATCH_CONTEXT OverwatchContext = {};
    int mainstatus = EXIT_SUCCESS;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    //
    // Initialize the client modules.
    //
    if (!LogInitialization(LOG_CONFIG_STDOUT | LOG_CONFIG_TIMESTAMP_PREFIX))
    {
        ERR_PRINT("LogInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    if (!TmuInitialization())
    {
        ERR_PRINT("TmuInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    if (!RndInitialization())
    {
        ERR_PRINT("RndInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    if (!ConInitialization())
    {
        ERR_PRINT("ConInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }
    //
    fConsoleInitialized = TRUE;

    //
    // Initialize the client devices.
    //
    if (!VivienneIoInitialization())
    {
        ERR_PRINT("VivienneIoInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }
    //
    fVivienneIoInitialized = TRUE;

    if (!PareidoliaIoInitialization())
    {
        ERR_PRINT("PareidoliaIoInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }
    //
    fPareidoliaIoInitialized = TRUE;

#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    if (!PareidoliaIoRestrictProcessAccess(GetCurrentProcessId()))
    {
        ERR_PRINT("PareidoliaIoRestrictProcessAccess failed: %u",
            GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }
    //
    fProcessAccessRestricted = TRUE;
#endif

    if (!MouiiIoInitialization())
    {
        ERR_PRINT("MouiiIoInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }
    //
    fMouiiIoInitialized = TRUE;

    if (!MouInitializeDeviceStackContext(NULL))
    {
        ERR_PRINT("MouInitializeDeviceStackContext failed: %u",
            GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    //
    // Initialize the triggerbot.
    //
    if (!WaitForOverwatchProcess(&OverwatchContext))
    {
        ERR_PRINT("WaitForOverwatchProcess failed.");
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    TrgInitialization(&OverwatchContext);

    //
    // Execute the triggerbot tick loop.
    //
    TrgTickLoop();

exit:
    if (fMouiiIoInitialized)
    {
        MouiiIoTermination();
    }

#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    if (fProcessAccessRestricted)
    {
        VERIFY(PareidoliaIoDerestrictProcessAccess(GetCurrentProcessId()));
    }
#endif

    if (fPareidoliaIoInitialized)
    {
        PareidoliaIoTermination();
    }

    if (fVivienneIoInitialized)
    {
        VivienneIoTermination();
    }

    if (fConsoleInitialized)
    {
        ConTermination();
    }

    return mainstatus;
}
