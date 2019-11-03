/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "console.h"

#include "debug.h"
#include "log.h"


//=============================================================================
// Constants
//=============================================================================
#define INPUT_EVENT_BUFFER_SIZE 64


//=============================================================================
// Private Types
//=============================================================================
typedef struct _CONSOLE_DEVICE {
    HANDLE DeviceHandle;
    BOOL RestoreMode;
    DWORD OriginalMode;
} CONSOLE_DEVICE, *PCONSOLE_DEVICE;

typedef struct _CONSOLE_CONTEXT {
    CONSOLE_DEVICE InputDevice;
} CONSOLE_CONTEXT, *PCONSOLE_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
static CONSOLE_CONTEXT g_ConContext = {};


//=============================================================================
// Private Prototypes
//=============================================================================
_Check_return_
_Success_(return != FALSE)
static
BOOL
ConpInitializeDevice(
    _In_ DWORD StandardDeviceHandle,
    _Out_ PCONSOLE_DEVICE pConsoleDevice
);

static
VOID
ConpTerminateDevice(
    _In_ PCONSOLE_DEVICE pConsoleDevice
);


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
BOOL
ConInitialization()
{
    CONSOLE_DEVICE InputDevice = {};
    BOOL status = TRUE;

    status = ConpInitializeDevice(STD_INPUT_HANDLE, &InputDevice);
    if (!status)
    {
        goto exit;
    }

    //
    // Initialize the global context.
    //
    RtlCopyMemory(
        &g_ConContext.InputDevice,
        &InputDevice,
        sizeof(CONSOLE_DEVICE));

exit:
    return status;
}


VOID
ConTermination()
{
    ConpTerminateDevice(&g_ConContext.InputDevice);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
ConWaitForInputKey(
    WORD VirtualKeyCode
)
/*++

Description:

    Process all console device input events until the specified virtual key is
    issued by the user.

Parameters:

    VirtualKeyCode - The virtual key code to wait for.

Remarks:

    This routine blocks the calling thread until the specified virtual key is
    processed.

--*/
{
    INPUT_RECORD InputEvents[INPUT_EVENT_BUFFER_SIZE] = {};
    DWORD nEventsRead = 0;
    DWORD i = 0;
    BOOL fKeyProcessed = FALSE;
    BOOL status = TRUE;

    for (;;)
    {
        status = ReadConsoleInputW(
            g_ConContext.InputDevice.DeviceHandle,
            InputEvents,
            ARRAYSIZE(InputEvents),
            &nEventsRead);
        if (!status)
        {
            ERR_PRINT("ReadConsoleInputW failed: %u", GetLastError());
            goto exit;
        }

        for (i = 0; i < nEventsRead; ++i)
        {
            if (KEY_EVENT != InputEvents[i].EventType)
            {
                continue;
            }

            if (InputEvents[i].Event.KeyEvent.wVirtualKeyCode ==
                VirtualKeyCode)
            {
                fKeyProcessed = TRUE;
                break;
            }
        }

        if (fKeyProcessed)
        {
            break;
        }
    }

exit:
    return status;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
static
BOOL
ConpInitializeDevice(
    DWORD StandardDeviceHandle,
    PCONSOLE_DEVICE pConsoleDevice
)
{
    HANDLE DeviceHandle = NULL;
    DWORD Mode = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(pConsoleDevice, sizeof(*pConsoleDevice));

    DeviceHandle = GetStdHandle(StandardDeviceHandle);
    if (INVALID_HANDLE_VALUE == DeviceHandle || !DeviceHandle)
    {
        ERR_PRINT("GetStdHandle failed: %u", GetLastError());
        status = FALSE;
        goto exit;
    }

    status = GetConsoleMode(DeviceHandle, &Mode);
    if (!status)
    {
        ERR_PRINT("GetConsoleMode failed: %u", GetLastError());
        goto exit;
    }

    //
    // Set out parameters.
    //
    pConsoleDevice->DeviceHandle = DeviceHandle;
    pConsoleDevice->RestoreMode = FALSE;
    pConsoleDevice->OriginalMode = Mode;

exit:
    return status;
}


_Use_decl_annotations_
static
VOID
ConpTerminateDevice(
    PCONSOLE_DEVICE pConsoleDevice
)
{
    VERIFY(FlushConsoleInputBuffer(pConsoleDevice->DeviceHandle));

    if (pConsoleDevice->RestoreMode)
    {
        VERIFY(SetConsoleMode(
            pConsoleDevice->DeviceHandle,
            pConsoleDevice->OriginalMode));
    }
}
