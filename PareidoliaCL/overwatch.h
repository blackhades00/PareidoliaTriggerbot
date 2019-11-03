/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include "../VivienneVMM/common/arch_x64.h"

//=============================================================================
// Constants
//=============================================================================
#define OVERWATCH_PROCESS_NAME_U       L"Overwatch.exe"

//=============================================================================
// Public Types
//=============================================================================
/*++

Name:

    WIDOWMAKER_TRACE_STATE

Description:

    The Widowmaker trace state is a ULONG-sized variable which has a nonzero
    value when:

        1. There is a Widowmaker player in a match.

        2. The Widowmaker player has the Widow's Kiss sniper rifle equipped.

        3. The Widowmaker player is scoped (i.e., zoomed in) and the Widow's
            Kiss is fully charged.

        4. The Widowmaker player's crosshair is over an enemy player entity or
            a dynamic, non-player entity (e.g., the payload, a closed spawn
            door, or the lid of a trash can).

    This variable exists as a field in the trace state object type. The
    following diagram depicts the trace state elements inside the Overwatch
    virtual address space:


              Low Memory
        +====================+
        |                    |       Trace State Object
        |                    |               ~
        |                    |        +=============+
        |                    |        |             |         Trace State
        |--------------------|        |-------------|              ~
        | Dynamic Allocation | -----> | Trace State | -----> [0, MAXULONG]
        |--------------------|        |   Variable  |
        |                    |        |-------------|
        |                    |        |             |
        |                    |        +=============+
        |                    |
        |                    |
        |                    |
        |                    |
        |                    |
        +====================+
              High Memory


    The game engine maintains one trace state object for each Widowmaker player
    in a match. A trace state object is created each time a non-Widowmaker
    player swaps to the Widowmaker hero. The object is destroyed when the
    player swaps to a non-Widowmaker hero, the round ends, or the player leaves
    the match.

    NOTE We do not fully understand the purpose of the trace state variable. We
    refer to this concept as the 'trace state' because the variable behaves as
    if it were the hit detection result of the game engine running a trace ray
    whose origin vector is the Widowmaker player's crosshair.

--*/
typedef ULONG WIDOWMAKER_TRACE_STATE, *PWIDOWMAKER_TRACE_STATE;

/*++

Name:

    TRACE_STATE_INSTRUCTION

Description:

    The address and disassembly information of an instruction in the remote
    Overwatch process which accesses a player's Widowmaker trace state variable
    when that player presses their 'zoom' keybind.

Fields:

    Address - The virtual address of the instruction in the remote Overwatch
        process.

    Register - The destination register in the instruction. This register
        contains the base address of the trace state object when the
        instruction is executed.

    Displacement - The field offset of the trace state variable in the trace
        state object type.

Remarks:

    The trace state mechanic provides all of the functionality required to
    write a triggerbot. In order to use this mechanic we need to be able to
    locate the address of our (local player) trace state object inside the
    remote Overwatch process. This is nontrivial for the following reasons:

        1. The address of the target trace state object changes when the object
            is destroyed and recreated.

        2. We cannot hook code in the Overwatch process because the anti-cheat
            frequently scans for code patches.

        3. Overwatch's code and data are significantly obfuscated.

    We can reliably recover the address of our trace state object using the
    VivienneVMM Capture Execution Context Register (CECR) request. Our target
    instruction is executed whenever a Widowmaker player presses their 'zoom'
    keybind. The following is the annotated assembly of the target instruction:

        Platform:   Windows 7 x64
        File Name:  Overwatch.exe
        Version:    1.41.1.0 - 63372
        SHA256:

            9d079af7df9ed332f32d303c1eec0aab886f300dc79489019b8bbae682cbdb99

        Assembly:

            89 87 FC 01 00 00       mov     [rdi+1FCh], eax

        rdi = Pointer to the base address of a trace state object.
        1FC = The field offset of the trace state variable.
        eax = The new trace state value.

    NOTE We found this instruction by scanning Overwatch's virtual memory for
    values which changed when the local player was scoped / not scoped.

    NOTE We do not fully understand the purpose of the trace state instruction
    or its containing function.

--*/
typedef struct _TRACE_STATE_INSTRUCTION {
    ULONG_PTR Address;
    X64_REGISTER Register;
    ULONG Displacement;
} TRACE_STATE_INSTRUCTION, *PTRACE_STATE_INSTRUCTION;

typedef struct _OVERWATCH_CONTEXT {
    ULONG_PTR ProcessId;
    ULONG_PTR ImageBase;
    TRACE_STATE_INSTRUCTION TraceStateInstruction;
} OVERWATCH_CONTEXT, *POVERWATCH_CONTEXT;

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
_Success_(return != FALSE)
BOOL
OwGetOverwatchContext(
    _In_ ULONG_PTR ProcessId,
    _Out_ POVERWATCH_CONTEXT pContext
);


_Check_return_
BOOL
OwIsTracingEnemyPlayerEntity(
    _In_ WIDOWMAKER_TRACE_STATE TraceState
);
