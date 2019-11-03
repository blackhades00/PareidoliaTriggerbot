/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#define IS_KEY_DOWN_ASYNC(VirtualKey) \
    ((BOOLEAN)(GetAsyncKeyState(VirtualKey) & 0x8001))
