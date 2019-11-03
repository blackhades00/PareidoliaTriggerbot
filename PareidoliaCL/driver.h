/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include "../Common/ioctl.h"

//=============================================================================
// Meta Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoInitialization();

VOID
PareidoliaIoTermination();

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoRestrictProcessAccess(
    _In_ ULONG_PTR ProcessId
);

_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoDerestrictProcessAccess(
    _In_ ULONG_PTR ProcessId
);

_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoGetProcessImageBase(
    _In_ ULONG_PTR ProcessId,
    _Out_ PULONG_PTR pImageBase
);

_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoGetProcessImageFilePathSize(
    _In_ ULONG_PTR ProcessId,
    _Out_ PULONG pcbSize
);

_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoGetProcessImageFilePath(
    _In_ ULONG_PTR ProcessId,
    _Out_writes_bytes_(cbImageFilePath) PWCHAR pwzImageFilePath,
    _In_ ULONG cbImageFilePath
);

_Check_return_
_Success_(return != FALSE)
BOOL
PareidoliaIoReadVirtualMemory(
    _In_ ULONG_PTR ProcessId,
    _In_ ULONG_PTR Address,
    _Out_writes_bytes_(cbBuffer) PVOID pBuffer,
    _In_ ULONG cbBuffer
);
