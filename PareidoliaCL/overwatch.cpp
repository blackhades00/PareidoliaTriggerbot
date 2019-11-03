/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "overwatch.h"

#include "debug.h"
#include "disassembler.h"
#include "driver.h"
#include "log.h"
#include "memory_util.h"
#include "ntdll.h"
#include "pattern.h"
#include "pe.h"
#include "process_util.h"

#include "../Common/config.h"

#include "../VivienneVMM/common/arch_x64.h"

#include "../hde/hde.h"


//=============================================================================
// Constants
//=============================================================================
static CHAR g_TraceStateInstructionPattern[] =
    "\x89\x87"
    "\x00\x00\x00\x00"
    "\x0F\x29\x87"
    "\x00\x00\x00\x00"
    "\x0F\x29\x87"
    "\x00\x00\x00\x00";

static CHAR g_TraceStateInstructionMask[] = "xx????xxx????xxx????";

C_ASSERT(
    ARRAYSIZE(g_TraceStateInstructionPattern) ==
    ARRAYSIZE(g_TraceStateInstructionMask));

#define TRACE_STATE_INSTRUCTION_MASK_WILDCARD '?'

#define TRACE_STATE_INSTRUCTION_DISASSEMBLY_FLAGS   (F_MODRM | F_DISP32)

#define TRACE_STATE_INSTRUCTION_OPCODE 0x89

#define TRACE_STATE_FILTER_FLAG_ENEMY_ENTITY                (1 << 31)
#define TRACE_STATE_FILTER_FLAG_DYNAMIC_NONPLAYER_ENTITY    (1 << 30)


//=============================================================================
// Private Prototypes
//=============================================================================
#if defined(CFG_USE_FIXED_TRACE_STATE_INSTRUCTION)
_Check_return_
_Success_(return != FALSE)
static
BOOL
OwpGetTraceStateInstructionFixed(
    _In_ ULONG_PTR ImageBase,
    _Out_ PTRACE_STATE_INSTRUCTION pTraceStateInstruction
);

#else

_Check_return_
_Success_(return != FALSE)
static
BOOL
OwpGetTraceStateInstructionDynamic(
    _In_ ULONG_PTR ProcessId,
    _In_ ULONG_PTR ImageBase,
    _Out_ PTRACE_STATE_INSTRUCTION pTraceStateInstruction
);
#endif

_Check_return_
_Success_(return != FALSE)
static
BOOL
OwpReadPeHeaderFromDisk(
    _In_ ULONG_PTR ProcessId,
    _Outptr_result_nullonfailure_ PVOID* ppPeHeader,
    _Out_ PULONG pcbPeHeader
);

_Check_return_
_Success_(return != FALSE)
static
BOOL
OwpFindRemoteTraceStateInstructionAddress(
    _In_ ULONG_PTR ProcessId,
    _In_ ULONG_PTR ImageBase,
    _In_ PVOID pPeHeader,
    _Out_ PULONG_PTR pRemoteInstruction
);

_Check_return_
_Success_(return != FALSE)
static
BOOL
OwpDisassembleTraceStateInstruction(
    _In_ ULONG_PTR ProcessId,
    _In_ ULONG_PTR RemoteInstruction,
    _Out_ PTRACE_STATE_INSTRUCTION pTraceStateInstruction
);


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
OwGetOverwatchContext(
    ULONG_PTR ProcessId,
    POVERWATCH_CONTEXT pContext
)
{
    ULONG_PTR ImageBase = 0;
    TRACE_STATE_INSTRUCTION TraceStateInstruction = {};
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(pContext, sizeof(*pContext));

    DBG_PRINT("Getting Overwatch context.");

    status = PareidoliaIoGetProcessImageBase(ProcessId, &ImageBase);
    if (!status)
    {
        ERR_PRINT("PareidoliaIoGetProcessImageBase failed: %u",
            GetLastError());
        goto exit;
    }

    DBG_PRINT("ImageBase: %p", ImageBase);

#if defined(CFG_USE_FIXED_TRACE_STATE_INSTRUCTION)
    status = OwpGetTraceStateInstructionFixed(&TraceStateInstruction);
    if (!status)
    {
        ERR_PRINT("OwpGetTraceStateInstructionFixed failed.");
        goto exit;
    }
#else
    status = OwpGetTraceStateInstructionDynamic(
        ProcessId,
        ImageBase,
        &TraceStateInstruction);
    if (!status)
    {
        ERR_PRINT("OwpGetTraceStateInstructionDynamic failed.");
        goto exit;
    }
#endif

    DBG_PRINT("Trace state instruction:");
    DBG_PRINT("    Address:         %p", TraceStateInstruction.Address);
    DBG_PRINT("    Register:        %s (%d)",
        DsmRegisterToString(TraceStateInstruction.Register),
        TraceStateInstruction.Register);
    DBG_PRINT("    Displacement:    0x%X", TraceStateInstruction.Displacement);

    //
    // Set out parameters.
    //
    pContext->ProcessId = ProcessId;
    pContext->ImageBase = ImageBase;
    RtlCopyMemory(
        &pContext->TraceStateInstruction,
        &TraceStateInstruction,
        sizeof(TRACE_STATE_INSTRUCTION));

exit:
    return status;
}


_Use_decl_annotations_
BOOL
OwIsTracingEnemyPlayerEntity(
    WIDOWMAKER_TRACE_STATE TraceState
)
/*++

Description:

    Analyze the trace state value to determine if it corresponds to an enemy
    player entity.

Parameters:

    TraceState - The trace state to be analyzed.

Remarks:

    The current strategy applies a mask to the trace state value to filter
    some of the dynamic, non-player entity values from the set of nonzero trace
    state values.

    This mask was created by analyzing the relationship between entity types
    and the trace state values they generate (when the local player is scoped,
    fully charged, and the crosshair is over that entity type). See the
    CFG_TB_PRINT_TRACE_STATE_VALUE_ON_TRIGGER_ACTIVATION config setting.

    TODO BUGBUG The mask occasionally filters out enemy McCree entities.

--*/
{
    return
        FlagOn(TraceState, TRACE_STATE_FILTER_FLAG_ENEMY_ENTITY) &&
        !FlagOn(TraceState, TRACE_STATE_FILTER_FLAG_DYNAMIC_NONPLAYER_ENTITY);
}


//=============================================================================
// Private Interface
//=============================================================================
#if defined(CFG_USE_FIXED_TRACE_STATE_INSTRUCTION)

_Use_decl_annotations_
static
BOOL
OwpGetTraceStateInstructionFixed(
    ULONG_PTR ImageBase,
    PTRACE_STATE_INSTRUCTION pTraceStateInstruction
)
/*++

Description:

    Returns the hard-coded trace state instruction.

Parameters:

    pTraceStateInstruction - Returns the hard-coded trace state instruction.

Remarks:

    This routine is an alternative to OwpGetTraceStateInstructionDynamic. Use
    this routine if an Overwatch patch breaks the trace state instruction
    signature.

--*/
{
    DBG_PRINT("Getting trace state instruction. (Fixed)");

    pTraceStateInstruction->Address =
        ImageBase + CFG_TRACE_STATE_INSTRUCTION_RELATIVE_ADDRESS;
    pTraceStateInstruction->Register = CFG_TRACE_STATE_INSTRUCTION_REGISTER;
    pTraceStateInstruction->Displacement =
        CFG_TRACE_STATE_INSTRUCTION_DISPLACEMENT;

    return TRUE;
}

#else

_Use_decl_annotations_
static
BOOL
OwpGetTraceStateInstructionDynamic(
    ULONG_PTR ProcessId,
    ULONG_PTR ImageBase,
    PTRACE_STATE_INSTRUCTION pTraceStateInstruction
)
/*++

Description:

    Search the text section of the target Overwatch process for the trace state
    instruction.

Parameters:

    ProcessId - The process id of the target Overwatch process.

    ImageBase - The image base of the target Overwatch process.

    pTraceStateInstruction - Returns the disassembly information for the trace
        state instruction in the target Overwatch process.

--*/
{
    PVOID pPeHeader = NULL;
    ULONG cbPeHeader = 0;
    ULONG cbImageSize = 0;
    ULONG_PTR RemoteInstruction = NULL;
    TRACE_STATE_INSTRUCTION TraceStateInstruction = {};
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(
        pTraceStateInstruction,
        sizeof(*pTraceStateInstruction));

    DBG_PRINT("Getting trace state instruction. (Dynamic)");

    status = OwpReadPeHeaderFromDisk(ProcessId, &pPeHeader, &cbPeHeader);
    if (!status)
    {
        ERR_PRINT("OwpReadPeHeaderFromDisk failed.");
        goto exit;
    }

    cbImageSize = PeGetImageSize(pPeHeader);
    if (!cbImageSize)
    {
        ERR_PRINT("Unexpected image size.");
        goto exit;
    }

    DBG_PRINT("ImageSize: 0x%X", cbImageSize);

    status = OwpFindRemoteTraceStateInstructionAddress(
        ProcessId,
        ImageBase,
        pPeHeader,
        &RemoteInstruction);
    if (!status)
    {
        ERR_PRINT("OwpFindRemoteTraceStateInstructionAddress failed.");
        goto exit;
    }

    status = OwpDisassembleTraceStateInstruction(
        ProcessId,
        RemoteInstruction,
        &TraceStateInstruction);
    if (!status)
    {
        ERR_PRINT("OwpDisassembleTraceStateInstruction failed.");
        goto exit;
    }

    //
    // Set out parameters.
    //
    RtlCopyMemory(
        pTraceStateInstruction,
        &TraceStateInstruction,
        sizeof(TRACE_STATE_INSTRUCTION));

exit:
    if (pPeHeader)
    {
        VERIFY(MemFreeHeap(pPeHeader));
    }

    return status;
}

#endif


_Use_decl_annotations_
static
BOOL
OwpReadPeHeaderFromDisk(
    ULONG_PTR ProcessId,
    PVOID* ppPeHeader,
    PULONG pcbPeHeader
)
/*++

Description:

    Read the PE header of the Overwatch image file on disk.

Parameters:

    ProcessId - The process id of the target Overwatch process.

    ppPeHeader - Returns a pointer to an allocated buffer which contains the
        contents of the PE header from the Overwatch image file on disk.

    pcbPeHeader - Returns the size of the allocated buffer.

Remarks:

    If successful, the caller must free the returned buffer by calling
    MemFreeHeap.

    We must read the PE header from the image file on disk because the
    Overwatch process intentionally destroys its mapped PE header during the
    unpacking phase.

--*/
{
    PWCHAR pwzImageFilePath = NULL;
    ULONG cbImageFilePath = 0;
    PVOID pPeHeader = NULL;
    ULONG cbPeHeader = 0;
    UNICODE_STRING usImageFilePath = {};
    ULONG FileOptions = 0;
    HANDLE FileHandle = NULL;
    OBJECT_ATTRIBUTES ObjectAttributes = {};
    IO_STATUS_BLOCK IoStatusBlock = {};
    BOOL fFileOpened = FALSE;
    PIMAGE_NT_HEADERS pNtHeaders = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *ppPeHeader = NULL;
    *pcbPeHeader = 0;

    status = PsuGetProcessImageFilePath(
        ProcessId,
        &pwzImageFilePath,
        &cbImageFilePath);
    if (!status)
    {
        ERR_PRINT("PsuGetProcessImageFilePath failed.");
        goto exit;
    }

    DBG_PRINT("ImageFilePath: %ls", pwzImageFilePath);

    cbPeHeader = PAGE_SIZE;

    pPeHeader = MemAllocateHeap(cbPeHeader);
    if (!pPeHeader)
    {
        ERR_PRINT("MemAllocateHeap failed.");
        status = FALSE;
        goto exit;
    }

    RtlInitUnicodeString(&usImageFilePath, pwzImageFilePath);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &usImageFilePath,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    FileOptions =
        FILE_SEQUENTIAL_ONLY |
        FILE_SYNCHRONOUS_IO_NONALERT |
        FILE_NON_DIRECTORY_FILE;

    ntstatus = NtOpenFile(
        &FileHandle,
        FILE_READ_DATA | SYNCHRONIZE,
        &ObjectAttributes,
        &IoStatusBlock,
        FILE_SHARE_READ,
        FileOptions);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("NtOpenFile failed: 0x%X", ntstatus);
        status = FALSE;
        goto exit;
    }
    //
    fFileOpened = TRUE;

    IoStatusBlock = {};

    ntstatus = NtReadFile(
        FileHandle,
        NULL,
        NULL,
        NULL,
        &IoStatusBlock,
        pPeHeader,
        cbPeHeader,
        NULL,
        NULL);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("NtReadFile failed: 0x%X", ntstatus);
        status = FALSE;
        goto exit;
    }
    //
    if (IoStatusBlock.Information > ULONG_MAX ||
        (ULONG)IoStatusBlock.Information != cbPeHeader)
    {
        ERR_PRINT("NtReadFile failed. (Unexpected read size)");
        status = FALSE;
        goto exit;
    }

    pNtHeaders = RtlImageNtHeader(pPeHeader);
    if (!pNtHeaders)
    {
        ERR_PRINT("RtlImageNtHeader failed. (Unexpected)");
        status = FALSE;
        goto exit;
    }

    if (pNtHeaders->OptionalHeader.SizeOfHeaders > cbPeHeader)
    {
        ERR_PRINT("Unexpected SizeOfHeaders: 0x%X",
            pNtHeaders->OptionalHeader.SizeOfHeaders);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *ppPeHeader = pPeHeader;
    *pcbPeHeader = cbPeHeader;

exit:
    if (fFileOpened)
    {
        VERIFY(NT_SUCCESS(NtClose(FileHandle)));
    }

    if (!status)
    {
        if (pPeHeader)
        {
            VERIFY(MemFreeHeap(pPeHeader));
        }
    }

    if (pwzImageFilePath)
    {
        VERIFY(MemFreeHeap(pwzImageFilePath));
    }

    return status;
}


_Use_decl_annotations_
static
BOOL
OwpFindRemoteTraceStateInstructionAddress(
    ULONG_PTR ProcessId,
    ULONG_PTR ImageBase,
    PVOID pPeHeader,
    PULONG_PTR pRemoteInstruction
)
/*++

Description:

    Search the text section of the target Overwatch process for the trace state
    instruction.

Parameters:

    ProcessId - The process id of the target Overwatch process.

    ImageBase - The image base of the target Overwatch process.

    pPeHeader - Pointer to a buffer which contains the contents of the PE
        header from the Overwatch image file on disk.

    pRemoteInstruction - Returns the virtual address of the trace state
        instruction in the target Overwatch process.

Remarks:

    This routine fails if the instruction is less than HDE_INSTRUCTION_SIZE_MAX
    bytes from the end of the remote text section.

--*/
{
    PIMAGE_SECTION_HEADER* ppExecutableSections = NULL;
    ULONG nExecutableSections = 0;
    ULONG_PTR TextSectionRemote = 0;
    ULONG cbTextSection = 0;
    PVOID pTextSectionLocal = NULL;
    ULONG_PTR InstructionLocal = 0;
    ULONG_PTR InstructionRemote = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pRemoteInstruction = 0;

    DBG_PRINT("Finding remote trace state instruction address.");

    status = PeGetExecutableSections(
        pPeHeader,
        &ppExecutableSections,
        &nExecutableSections);
    if (!status)
    {
        ERR_PRINT("PeGetExecutableSections failed: %u", GetLastError());
        goto exit;
    }
    //
    if (1 != nExecutableSections)
    {
        ERR_PRINT("Unexpected number of executable PE sections: %u",
            nExecutableSections);
        status = FALSE;
        goto exit;
    }

    //
    // Calculate the virtual address of the text section in the remote
    //  Overwatch process.
    //
    TextSectionRemote = ImageBase + ppExecutableSections[0]->VirtualAddress;
    cbTextSection = ppExecutableSections[0]->Misc.VirtualSize;

    DBG_PRINT("TextSectionRemote: %p (0x%X)",
        TextSectionRemote,
        cbTextSection);

    pTextSectionLocal = MemAllocateHeap(cbTextSection);
    if (!pTextSectionLocal)
    {
        ERR_PRINT("MemAllocateHeap failed.");
        status = FALSE;
        goto exit;
    }

    //
    // Read the contents of the remote text section.
    //
    status = PareidoliaIoReadVirtualMemory(
        ProcessId,
        TextSectionRemote,
        pTextSectionLocal,
        cbTextSection);
    if (!status)
    {
        ERR_PRINT(
            "PareidoliaIoReadVirtualMemory failed: %u"
            " (Address = %p, Size = 0x%X)",
            GetLastError(),
            TextSectionRemote,
            cbTextSection);
        goto exit;
    }

    //
    // Search the local copy of the text section for the trace state
    //  instruction signature.
    //
    status = PtnFindPatternUnique(
        pTextSectionLocal,
        cbTextSection,
        g_TraceStateInstructionPattern,
        g_TraceStateInstructionMask,
        ARRAYSIZE(g_TraceStateInstructionPattern),
        TRACE_STATE_INSTRUCTION_MASK_WILDCARD,
        &InstructionLocal);
    if (!status)
    {
        ERR_PRINT("Failed to find trace state instruction.");
        goto exit;
    }

    //
    // Calculate the virtual address of the trace state instruction in the
    //  remote Overwatch process.
    //
    InstructionRemote =
        TextSectionRemote + InstructionLocal - (ULONG_PTR)pTextSectionLocal;

    //
    // Verify that the remote trace state instruction is in the remote text
    //  section.
    //
    if (InstructionRemote + HDE_INSTRUCTION_SIZE_MAX <= InstructionRemote ||
        InstructionRemote < TextSectionRemote ||
        InstructionRemote + HDE_INSTRUCTION_SIZE_MAX >=
            TextSectionRemote + cbTextSection)
    {
        ERR_PRINT(
            "Unexpected remote trace state instruction address."
            " (Address = %p, TextSection = %p - %p)",
            InstructionRemote,
            TextSectionRemote,
            TextSectionRemote + cbTextSection);
        status = FALSE;
        goto exit;
    }

    DBG_PRINT("Found remote trace state instruction: %p (Rva = 0x%08IX)",
        InstructionRemote,
        InstructionRemote - ImageBase);

    //
    // Set out parameters.
    //
    *pRemoteInstruction = InstructionRemote;

exit:
    if (pTextSectionLocal)
    {
        VERIFY(MemFreeHeap(pTextSectionLocal));
    }

    if (ppExecutableSections)
    {
        VERIFY(MemFreeHeap(ppExecutableSections));
    }

    return status;
}


_Use_decl_annotations_
static
BOOL
OwpDisassembleTraceStateInstruction(
    ULONG_PTR ProcessId,
    ULONG_PTR RemoteInstruction,
    PTRACE_STATE_INSTRUCTION pTraceStateInstruction
)
/*++

Description:

    Initialize the disassembly information for the trace state instruction in
    the target Overwatch process.

Parameters:

    ProcessId - The process id of the target Overwatch process.

    RemoteInstruction - The virtual address of the trace state instruction in
        the target Overwatch process.

    pTraceStateInstruction - Returns the disassembly information for the trace
        state instruction in the target Overwatch process.

Remarks:

    This routine validates the remote trace state instruction.

--*/
{
    PVOID pBuffer = NULL;
    ULONG cbBuffer = 0;
    HDE_DISASSEMBLY Disassembly = {};
    UINT cbInstruction = 0;
    MODRM_BYTE ModRm = {};
    X64_REGISTER FirstOperand = {};
    X64_REGISTER SecondOperand = {};
    BOOL status = TRUE;

    //
    // Zero out parameters
    //
    RtlSecureZeroMemory(
        pTraceStateInstruction,
        sizeof(*pTraceStateInstruction));

    cbBuffer = HDE_INSTRUCTION_SIZE_MAX;

    pBuffer = MemAllocateHeap(cbBuffer);
    if (!pBuffer)
    {
        ERR_PRINT("MemAllocateHeap failed.");
        status = FALSE;
        goto exit;
    }

    //
    // Read the remote trace state instruction.
    //
    status = PareidoliaIoReadVirtualMemory(
        ProcessId,
        RemoteInstruction,
        pBuffer,
        cbBuffer);
    if (!status)
    {
        ERR_PRINT(
            "PareidoliaIoReadVirtualMemory failed: %u"
            " (Address = %p, Size = 0x%X)",
            GetLastError(),
            RemoteInstruction,
            cbBuffer);
        goto exit;
    }

    //
    // Validate the instruction's disassembly information.
    //
    cbInstruction = HDE_DISASM(pBuffer, &Disassembly);
    if (!cbInstruction)
    {
        ERR_PRINT("HDE_DISASM failed.");
        status = FALSE;
        goto exit;
    }
    //
    if (F_ERROR & Disassembly.flags)
    {
        ERR_PRINT("HDE_DISASM failed. (Flags = 0x%08X)", Disassembly.flags);
        status = FALSE;
        goto exit;
    }

    if (TRACE_STATE_INSTRUCTION_OPCODE != Disassembly.opcode)
    {
        ERR_PRINT(
            "Unexpected disassembly. (OpCode = %02hhX, Expected = %02hhX)",
            Disassembly.opcode,
            TRACE_STATE_INSTRUCTION_OPCODE);
        status = FALSE;
        goto exit;
    }

    if (TRACE_STATE_INSTRUCTION_DISASSEMBLY_FLAGS !=
        (TRACE_STATE_INSTRUCTION_DISASSEMBLY_FLAGS & Disassembly.flags))
    {
        ERR_PRINT("Unexpected disassembly. (Flags = 0x%08X)",
            Disassembly.flags);
        status = FALSE;
        goto exit;
    }

    ModRm.Value = Disassembly.modrm;

    status = DsmParseModRmOperands(ModRm, &FirstOperand, &SecondOperand);
    if (!status)
    {
        ERR_PRINT("DsmParseModRmOperands failed.");
        goto exit;
    }

    //
    // Set out parameters
    //
    pTraceStateInstruction->Address = RemoteInstruction;
    pTraceStateInstruction->Register = FirstOperand;
    pTraceStateInstruction->Displacement = Disassembly.disp.disp32;

exit:
    if (pBuffer)
    {
        VERIFY(MemFreeHeap(pBuffer));
    }

    return status;
}
