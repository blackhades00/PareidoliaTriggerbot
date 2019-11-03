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
PtnFindPatternUnique(
    _In_ PVOID pBaseAddress,
    _In_ SIZE_T cbSearchSize,
    _In_z_ PCHAR pszPattern,
    _In_z_ PCHAR pszMask,
    _In_ SIZE_T cchPattern,
    _In_ CHAR Wildcard,
    _Out_ PULONG_PTR pAddress
);
