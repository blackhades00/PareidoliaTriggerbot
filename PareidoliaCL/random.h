/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

//=============================================================================
// Meta Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
BOOL
RndInitialization();

//=============================================================================
// Public Interface
//=============================================================================
ULONG
RndUlong();

ULONG
RndUlongInRange(
    _In_ ULONG Minimum,
    _In_ ULONG Maximum
);
