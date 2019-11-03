/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

_IRQL_requires_max_(APC_LEVEL)
_Check_return_
EXTERN_C
NTSTATUS
PsuGetImageBase(
    _In_ HANDLE ProcessId,
    _Out_ PULONG_PTR pImageBase
);

_IRQL_requires_max_(APC_LEVEL)
_Check_return_
EXTERN_C
NTSTATUS
PsuGetImageFilePath(
    _In_ HANDLE ProcessId,
    _Out_writes_bytes_opt_(cbImageFilePath) PWCHAR pwzImageFilePath,
    _In_ ULONG cbImageFilePath,
    _Out_opt_ PULONG pcbRequired
);

_IRQL_requires_max_(APC_LEVEL)
_Check_return_
EXTERN_C
NTSTATUS
PsuReadVirtualMemory(
    _In_ HANDLE ProcessId,
    _In_ ULONG_PTR Address,
    _Out_writes_(cbBuffer) PVOID pBuffer,
    _In_ ULONG cbBuffer,
    _Out_opt_ PULONG pcbRead
);
