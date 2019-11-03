/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

//=============================================================================
// Meta Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
PamDriverEntry();

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
PamDriverUnload();

//=============================================================================
// Public Interface
//=============================================================================
_IRQL_requires_max_(APC_LEVEL)
_Check_return_
EXTERN_C
NTSTATUS
PamRestrictAccess(
    _In_ HANDLE ProcessId
);

_IRQL_requires_max_(APC_LEVEL)
_Check_return_
EXTERN_C
NTSTATUS
PamDerestrictAccess(
    _In_ HANDLE ProcessId
);

_IRQL_requires_max_(APC_LEVEL)
EXTERN_C
VOID
PamReset();
