/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

_IRQL_requires_max_(PASSIVE_LEVEL)
_Check_return_
EXTERN_C
NTSTATUS
StlIsUnlinkDriverObjectSupported();

_Check_return_
EXTERN_C
NTSTATUS
StlUnlinkDriverObject(
    _Inout_ PDRIVER_OBJECT pDriverObject
);
