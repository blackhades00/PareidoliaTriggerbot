/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

_Check_return_
_Success_(return != FALSE)
BOOL
PsuLookupProcessIdsByName(
    _In_z_ PCWSTR pwzProcessName,
    _Outptr_result_nullonfailure_ PULONG_PTR* ppProcessIds,
    _Out_ PSIZE_T pnProcessIds
);

_Check_return_
_Success_(return != FALSE)
BOOL
PsuLookupProcessIdByName(
    _In_z_ PCWSTR pwzProcessName,
    _Out_ PULONG_PTR pProcessId
);

_Check_return_
_Success_(return != FALSE)
BOOL
PsuGetProcessImageFilePath(
    _In_ ULONG_PTR ProcessId,
    _Outptr_result_nullonfailure_ PWCHAR* ppwzImageFilePath,
    _Out_ PULONG pcbImageFilePath
);
