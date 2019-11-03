/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "process_util.h"

#include "log.h"
#include "nt.h"
#include "object_util.h"


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PsuGetImageBase(
    HANDLE ProcessId,
    PULONG_PTR pImageBase
)
{
    PEPROCESS pProcess = NULL;
    BOOLEAN fHasProcessReference = FALSE;
    PVOID pSectionBase = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pImageBase = 0;

    ntstatus = PsLookupProcessByProcessId(ProcessId, &pProcess);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsLookupProcessByProcessId failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fHasProcessReference = TRUE;

    pSectionBase = PsGetProcessSectionBaseAddress(pProcess);
    if (!pSectionBase)
    {
        ERR_PRINT("Unexpected process section base. (ProcessId = %Iu)",
            ProcessId);
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pImageBase = (ULONG_PTR)pSectionBase;

exit:
    if (fHasProcessReference)
    {
        ObDereferenceObject(pProcess);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PsuGetImageFilePath(
    HANDLE ProcessId,
    PWCHAR pwzImageFilePath,
    ULONG cbImageFilePath,
    PULONG pcbRequired
)
/*++

Description:

    Get the NT file path for the image backing the target process.

Parameters:

    ProcessId - The process id of the target process.

    pwzImageFilePath - Optionally returns the NT file path for the image
        backing the target process.

    cbImageFilePath - The size of the provided output buffer.

    pcbRequired - Optionally returns the size of NT file path.

Return Value:

    STATUS_INFO_LENGTH_MISMATCH - The provided buffer is not large enough to
        store the NT file path.

--*/
{
    PEPROCESS pProcess = NULL;
    BOOLEAN fHasProcessReference = FALSE;
    PFILE_OBJECT pFileObject = NULL;
    BOOLEAN fHasFileObjectReference = FALSE;
    POBJECT_NAME_INFORMATION pObjectNameInfo = NULL;
    ULONG cbRequired = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    if (ARGUMENT_PRESENT(pwzImageFilePath) && cbImageFilePath)
    {
        RtlSecureZeroMemory(pwzImageFilePath, cbImageFilePath);
    }

    if (ARGUMENT_PRESENT(pcbRequired))
    {
        *pcbRequired = 0;
    }

    ntstatus = PsLookupProcessByProcessId(ProcessId, &pProcess);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsLookupProcessByProcessId failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fHasProcessReference = TRUE;

    //
    // NOTE This routine acquires process exit synchronization.
    //
    ntstatus = PsReferenceProcessFilePointer(pProcess, (PVOID*)&pFileObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsReferenceProcessFilePointer failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fHasFileObjectReference = TRUE;

    ntstatus = ObuQueryNameString(pFileObject, &pObjectNameInfo);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ObuQueryNameString failed: 0x%X", ntstatus);
        goto exit;
    }

    //
    // NOTE The 'MaximumLength' field includes the null-terminator.
    //
    cbRequired = pObjectNameInfo->Name.MaximumLength;

    //
    // Set the required size parameter.
    //
    if (ARGUMENT_PRESENT(pcbRequired))
    {
        *pcbRequired = cbRequired;
    }

    //
    // Fail if the file object is unnamed.
    //
    if (!pObjectNameInfo->Name.Length ||
        !pObjectNameInfo->Name.MaximumLength ||
        !pObjectNameInfo->Name.Buffer)
    {
        ERR_PRINT("Process file object is unnamed. (ProcessId = %Iu)",
            ProcessId);
        ntstatus = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }

    //
    // Fail if the specified buffer is too small.
    //
    if (cbRequired > cbImageFilePath)
    {
        ntstatus = STATUS_INFO_LENGTH_MISMATCH;
        goto exit;
    }

    //
    // Set out parameters.
    //
    RtlCopyMemory(pwzImageFilePath, pObjectNameInfo->Name.Buffer, cbRequired);

exit:
    if (pObjectNameInfo)
    {
        ExFreePool(pObjectNameInfo);
    }

    if (fHasFileObjectReference)
    {
        ObDereferenceObject(pFileObject);
    }

    if (fHasProcessReference)
    {
        ObDereferenceObject(pProcess);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PsuReadVirtualMemory(
    HANDLE ProcessId,
    ULONG_PTR Address,
    PVOID pBuffer,
    ULONG cbBuffer,
    PULONG pcbRead
)
{
    PEPROCESS pProcess = NULL;
    BOOLEAN fHasProcessReference = FALSE;
    BOOLEAN fHasProcessExitSynchronization = FALSE;
    KAPC_STATE ApcState = {};
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    if (ARGUMENT_PRESENT(pcbRead))
    {
        *pcbRead = 0;
    }

    //
    // Fail if the target address range is outside of user space.
    //
    if (Address + cbBuffer < Address ||
        Address + cbBuffer > (ULONG_PTR)MmHighestUserAddress ||
        Address + cbBuffer > (ULONG_PTR)MmHighestUserAddress)
    {
        ntstatus = STATUS_ACCESS_VIOLATION;
        goto exit;
    }

    ntstatus = PsLookupProcessByProcessId(ProcessId, &pProcess);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsLookupProcessByProcessId failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fHasProcessReference = TRUE;

    ntstatus = PsAcquireProcessExitSynchronization(pProcess);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsAcquireProcessExitSynchronization failed: 0x%X",
            ntstatus);
        goto exit;
    }
    //
    fHasProcessExitSynchronization = TRUE;

    __try
    {
        __try
        {
            KeStackAttachProcess(pProcess, &ApcState);
            RtlCopyMemory(pBuffer, (PVOID)Address, cbBuffer);
        }
        __finally
        {
            KeUnstackDetachProcess(&ApcState);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ERR_PRINT("Unexpected exception: 0x%X", GetExceptionCode());
        ntstatus = STATUS_UNHANDLED_EXCEPTION;
        goto exit;
    }

    //
    // Set out parameters.
    //
    if (ARGUMENT_PRESENT(pcbRead))
    {
        *pcbRead = cbBuffer;
    }

exit:
    if (fHasProcessExitSynchronization)
    {
        PsReleaseProcessExitSynchronization(pProcess);
    }

    if (fHasProcessReference)
    {
        ObDereferenceObject(pProcess);
    }

    return ntstatus;
}
