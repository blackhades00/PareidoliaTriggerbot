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
TmuInitialization();

//=============================================================================
// Public Interface
//=============================================================================
LONGLONG
TmuMicrosecondsToTicks(
    _In_ LONGLONG Microseconds
);

LONGLONG
TmuTicksToMicroseconds(
    _In_ LONGLONG Ticks
);

_Check_return_
_Success_(return != FALSE)
BOOL
TmuStallExecution(
    _In_ ULONG Microseconds
);
