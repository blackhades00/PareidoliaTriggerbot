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
ConInitialization();

VOID
ConTermination();

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
BOOL
ConWaitForInputKey(
    _In_ WORD VirtualKeyCode
);
