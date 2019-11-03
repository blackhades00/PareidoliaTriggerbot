/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

_Check_return_
_Success_(return != 0)
ULONG
PeGetImageSize(
    _In_ PVOID pImageBase
);

_Check_return_
_Success_(return != FALSE)
BOOL
PeGetSectionsByCharacteristics(
    _In_ PVOID pImageBase,
    _In_ ULONG Characteristics,
    _Outptr_result_nullonfailure_ PIMAGE_SECTION_HEADER** pppSectionHeaders,
    _Out_ PULONG pnSectionHeaders
);

_Check_return_
_Success_(return != FALSE)
BOOL
PeGetExecutableSections(
    _In_ PVOID pImageBase,
    _Outptr_result_nullonfailure_ PIMAGE_SECTION_HEADER** pppSectionHeaders,
    _Out_ PULONG pnSectionHeaders
);
