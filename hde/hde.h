#pragma once

#if defined(_WIN64)
#include "hde64.h"

typedef hde64s HDE_DISASSEMBLY, *PHDE_DISASSEMBLY;

#define HDE_DISASM hde64_disasm
#else
#include "hde32.h"

typedef hde32s HDE_DISASSEMBLY, *PHDE_DISASSEMBLY;

#define HDE_DISASM hde32_disasm
#endif

//
// The maximum amount of bytes the disassembler will read from the code buffer
//  before failing.
//
// NOTE This number was taken from the 'Hacker Disassembler Engine 64 C 0.04
//  FINAL' manual and has not been verified.
//
#define HDE_INSTRUCTION_SIZE_MAX 26
