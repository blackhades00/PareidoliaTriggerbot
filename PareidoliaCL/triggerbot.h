/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include "overwatch.h"

//=============================================================================
// Meta Interface
//=============================================================================
VOID
TrgInitialization(
    _In_ POVERWATCH_CONTEXT pOverwatchContext
);

//=============================================================================
// Public Interface
//=============================================================================
VOID
TrgTickLoop();
