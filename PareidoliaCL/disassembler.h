/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#pragma once

#include <Windows.h>

#include "../VivienneVMM/common/arch_x64.h"

#include "../hde/hde.h"

//=============================================================================
// Public Types
//=============================================================================
#pragma warning(push)
#pragma warning(disable : 4201) // Nonstandard extension: nameless struct/union
typedef union _MODRM_BYTE {
    struct {
        UCHAR RM  : 3;
        UCHAR Reg : 3;
        UCHAR Mod : 2;
    };
    UCHAR Value;
} MODRM_BYTE, *PMODRM_BYTE;
#pragma warning(pop)

C_ASSERT(sizeof(MODRM_BYTE) == sizeof(UCHAR));

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
BOOL
DsmParseModRmOperands(
    _In_ MODRM_BYTE ModRm,
    _Out_ PX64_REGISTER pFirstOperand,
    _Out_ PX64_REGISTER pSecondOperand
);

PCSTR
DsmRegisterToString(
    _In_ X64_REGISTER Register
);

VOID
DsmPrintInstruction(
    _In_ PHDE_DISASSEMBLY pDisassembly
);
