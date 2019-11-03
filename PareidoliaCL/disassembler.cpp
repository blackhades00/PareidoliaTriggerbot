/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "disassembler.h"

#include "log.h"

#include "../hde/hde.h"


//=============================================================================
// Constants
//=============================================================================
/*
    See 'Table 2-2. 32-Bit Addressing Forms with the ModR/M Byte' in the Intel
    instruction manual.
*/
static CONST X64_REGISTER g_ModRmEffectiveAddressTable[4][8] = {
    //
    // Mod: 00
    //
    {
        REGISTER_RAX,       REGISTER_RCX,       REGISTER_RDX,   REGISTER_RBX,
        REGISTER_INVALID,   REGISTER_INVALID,   REGISTER_RSI,   REGISTER_RDI
    },

    //
    // Mod: 01
    //
    {
        REGISTER_RAX,       REGISTER_RCX,       REGISTER_RDX,   REGISTER_RBX,
        REGISTER_INVALID,   REGISTER_RBP,       REGISTER_RSI,   REGISTER_RDI
    },

    //
    // Mod: 10
    //
    {
        REGISTER_RAX,       REGISTER_RCX,       REGISTER_RDX,   REGISTER_RBX,
        REGISTER_INVALID,   REGISTER_RBP,       REGISTER_RSI,   REGISTER_RDI
    },

    //
    // Mod: 11
    //
    {
        REGISTER_RAX,       REGISTER_RCX,       REGISTER_RDX,   REGISTER_RBX,
        REGISTER_RSP,       REGISTER_RBP,       REGISTER_RSI,   REGISTER_RDI
    },
};

static CONST X64_REGISTER g_ModRmSecondOperandTable32[8] = {
    REGISTER_RAX,
    REGISTER_RCX,
    REGISTER_RDX,
    REGISTER_RBX,
    REGISTER_RSP,
    REGISTER_RBP,
    REGISTER_RSI,
    REGISTER_RDI
};


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
DsmParseModRmOperands(
    MODRM_BYTE ModRm,
    PX64_REGISTER pFirstOperand,
    PX64_REGISTER pSecondOperand
)
/*++

Description:

    Return the two registers used for the specified ModRM byte.

Parameters:

    ModRm - The ModRM byte to be parsed.

    pFirstOperand - Returns the first operand for the specified ModRM byte.

    pSecondOperand - Returns the second operand for the specified ModRM byte.

Remarks:

    Returns FALSE if the specified ModRM byte requires a SIB byte.

--*/
{
    X64_REGISTER FirstOperand = {};
    X64_REGISTER SecondOperand = {};
    BOOL status = TRUE;

    //
    // Zero out parameters
    //
    *pFirstOperand = {};
    *pSecondOperand = {};

    FirstOperand = g_ModRmEffectiveAddressTable[ModRm.Mod][ModRm.RM];
    if (REGISTER_INVALID == FirstOperand)
    {
        ERR_PRINT("Unhandled first operand element. (Mod = %u, RM = %u)",
            ModRm.Mod,
            ModRm.RM);
        status = FALSE;
        goto exit;
    }

    SecondOperand = g_ModRmSecondOperandTable32[ModRm.Reg];
    if (REGISTER_INVALID == SecondOperand)
    {
        ERR_PRINT("Unhandled second operand element. (Reg = %u)", ModRm.Reg);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters
    //
    *pFirstOperand = FirstOperand;
    *pSecondOperand = SecondOperand;

exit:
    return status;
}


_Use_decl_annotations_
PCSTR
DsmRegisterToString(
    X64_REGISTER Register
)
{
    switch (Register)
    {
        case REGISTER_RIP: return "rip";
        case REGISTER_RAX: return "rax";
        case REGISTER_RCX: return "rcx";
        case REGISTER_RDX: return "rdx";
        case REGISTER_RDI: return "rdi";
        case REGISTER_RSI: return "rsi";
        case REGISTER_RBX: return "rbx";
        case REGISTER_RBP: return "rbp";
        case REGISTER_RSP: return "rsp";
        case REGISTER_R8:  return "r8";
        case REGISTER_R9:  return "r9";
        case REGISTER_R10: return "r10";
        case REGISTER_R11: return "r11";
        case REGISTER_R12: return "r12";
        case REGISTER_R13: return "r13";
        case REGISTER_R14: return "r14";
        case REGISTER_R15: return "r15";
        default:
            break;
    }

    return "(INVALID)";
}


_Use_decl_annotations_
VOID
DsmPrintInstruction(
    PHDE_DISASSEMBLY pDisassembly
)
{
    INF_PRINT("Disassembly:");
    INF_PRINT("    len:             %hhu", pDisassembly->len);
    INF_PRINT("    p_rep:           0x%02hhX", pDisassembly->p_rep);
    INF_PRINT("    p_lock:          0x%02hhX", pDisassembly->p_lock);
    INF_PRINT("    p_seg:           0x%02hhX", pDisassembly->p_seg);
    INF_PRINT("    p_66:            0x%02hhX", pDisassembly->p_66);
    INF_PRINT("    p_67:            0x%02hhX", pDisassembly->p_67);
    INF_PRINT("    rex:             0x%02hhX", pDisassembly->rex);
    INF_PRINT("    rex_w:           0x%02hhX", pDisassembly->rex_w);
    INF_PRINT("    rex_r:           0x%02hhX", pDisassembly->rex_r);
    INF_PRINT("    rex_x:           0x%02hhX", pDisassembly->rex_x);
    INF_PRINT("    rex_b:           0x%02hhX", pDisassembly->rex_b);
    INF_PRINT("    opcode:          0x%02hhX", pDisassembly->opcode);
    INF_PRINT("    opcode2:         0x%02hhX", pDisassembly->opcode2);
    INF_PRINT("    modrm:           0x%02hhX", pDisassembly->modrm);
    INF_PRINT("    modrm_mod:       0x%02hhX", pDisassembly->modrm_mod);
    INF_PRINT("    modrm_reg:       0x%02hhX", pDisassembly->modrm_reg);
    INF_PRINT("    modrm_rm:        0x%02hhX", pDisassembly->modrm_rm);
    INF_PRINT("    sib:             0x%02hhX", pDisassembly->sib);
    INF_PRINT("    sib_scale:       0x%02hhX", pDisassembly->sib_scale);
    INF_PRINT("    sib_index:       0x%02hhX", pDisassembly->sib_index);
    INF_PRINT("    sib_base:        0x%02hhX", pDisassembly->sib_base);
    INF_PRINT("    imm:");
    INF_PRINT("        imm8:        0x%02hhX", pDisassembly->imm.imm8);
    INF_PRINT("        imm16:       0x%04hX", pDisassembly->imm.imm16);
    INF_PRINT("        imm32:       0x%08X", pDisassembly->imm.imm32);
    INF_PRINT("        imm64:       0x%016llX", pDisassembly->imm.imm64);
    INF_PRINT("    disp:");
    INF_PRINT("        disp8:       0x%02hhX", pDisassembly->disp.disp8);
    INF_PRINT("        disp16:      0x%04hX", pDisassembly->disp.disp16);
    INF_PRINT("        disp32:      0x%08X", pDisassembly->disp.disp32);
    INF_PRINT("    flags:           0x%08X", pDisassembly->flags);
}
