/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "process_util.h"

#include "debug.h"
#include "driver.h"
#include "log.h"
#include "memory_util.h"
#include "ntdll.h"


_Use_decl_annotations_
BOOL
PsuLookupProcessIdsByName(
    PCWSTR pwzProcessName,
    PULONG_PTR* ppProcessIds,
    PSIZE_T pnProcessIds
)
/*++

Routine Description:

    Returns an array of process ids for all processes whose names match the
    specified process name.

Parameters:

    pwzProcessName - The target process name.

    ppProcessIds - Returns a pointer to an allocated array of process ids for
        all processes whose names match the specified process name.

    pnProcessIds - Returns the number of elements in the allocated array.

Remarks:

    If successful, the caller must free the returned array by calling
    MemFreeHeap.

--*/
{
    PSYSTEM_PROCESS_INFORMATION pSystemProcessInfo = NULL;
    ULONG cbSystemProcessInfo = NULL;
    UNICODE_STRING usProcessName = {};
    PSYSTEM_PROCESS_INFORMATION pEntry = NULL;
    BOOL fHasMoreEntries = FALSE;
    SIZE_T nProcessIds = 0;
    PULONG_PTR pProcessIds = NULL;
    SIZE_T i = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *ppProcessIds = NULL;
    *pnProcessIds = 0;

    for (cbSystemProcessInfo = sizeof(*pSystemProcessInfo);;)
    {
        pSystemProcessInfo = (PSYSTEM_PROCESS_INFORMATION)MemAllocateHeap(
            cbSystemProcessInfo);
        if (!pSystemProcessInfo)
        {
            ERR_PRINT("MemAllocateHeap failed.");
            status = FALSE;
            goto exit;
        }

        ntstatus = NtQuerySystemInformation(
            SystemProcessInformation,
            pSystemProcessInfo,
            cbSystemProcessInfo,
            &cbSystemProcessInfo);
        if (NT_SUCCESS(ntstatus))
        {
            break;
        }
        else if (STATUS_INFO_LENGTH_MISMATCH != ntstatus)
        {
            ERR_PRINT("NtQuerySystemInformation failed: 0x%X", ntstatus);
            status = FALSE;
            goto exit;
        }

        if (!MemFreeHeap(pSystemProcessInfo))
        {
            ERR_PRINT("MemFreeHeap failed: %u", GetLastError());
            status = FALSE;
            goto exit;
        }
    }

    RtlInitUnicodeString(&usProcessName, pwzProcessName);

    //
    // Determine the number of processes which have the specified process name.
    //
    fHasMoreEntries = TRUE;

    for (pEntry = pSystemProcessInfo;
        fHasMoreEntries;
        pEntry = OFFSET_POINTER(
            pEntry,
            pEntry->NextEntryOffset,
            SYSTEM_PROCESS_INFORMATION))
    {
        if (!pEntry->NextEntryOffset)
        {
            fHasMoreEntries = FALSE;
        }

        if (RtlEqualUnicodeString(&usProcessName, &pEntry->ImageName, TRUE))
        {
            nProcessIds++;
        }
    }
    //
    if (!nProcessIds)
    {
        SetLastError(ERROR_NOT_FOUND);
        status = FALSE;
        goto exit;
    }

    //
    // Allocate and fill the returned buffer.
    //
    pProcessIds =
        (PULONG_PTR)MemAllocateHeap(sizeof(*pProcessIds) * nProcessIds);
    if (!pProcessIds)
    {
        ERR_PRINT("MemAllocateHeap failed.");
        status = FALSE;
        goto exit;
    }

    fHasMoreEntries = TRUE;

    for (pEntry = pSystemProcessInfo;
        fHasMoreEntries;
        pEntry = OFFSET_POINTER(
            pEntry,
            pEntry->NextEntryOffset,
            SYSTEM_PROCESS_INFORMATION))
    {
        if (!pEntry->NextEntryOffset)
        {
            fHasMoreEntries = FALSE;
        }

        if (RtlEqualUnicodeString(&usProcessName, &pEntry->ImageName, TRUE))
        {
            pProcessIds[i] = (ULONG_PTR)pEntry->UniqueProcessId;
            i++;
        }
    }

    //
    // Set out parameters.
    //
    *ppProcessIds = pProcessIds;
    *pnProcessIds = nProcessIds;

exit:
    if (!status)
    {
        if (pProcessIds)
        {
            VERIFY(MemFreeHeap(pProcessIds));
        }
    }

    if (pSystemProcessInfo)
    {
        VERIFY(MemFreeHeap(pSystemProcessInfo));
    }

    return status;
}


_Use_decl_annotations_
BOOL
PsuLookupProcessIdByName(
    PCWSTR pwzProcessName,
    PULONG_PTR pProcessId
)
{
    PULONG_PTR pProcessIds = NULL;
    SIZE_T nProcessIds = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pProcessId = 0;

    status =
        PsuLookupProcessIdsByName(pwzProcessName, &pProcessIds, &nProcessIds);
    if (!status)
    {
        goto exit;
    }
    //
    if (1 != nProcessIds)
    {
        ERR_PRINT("Process name collision. (ProcessName = %ls)",
            pwzProcessName);
        SetLastError(ERROR_TOO_MANY_NAMES);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pProcessId = *pProcessIds;

exit:
    if (pProcessIds)
    {
        VERIFY(MemFreeHeap(pProcessIds));
    }

    return status;
}


_Use_decl_annotations_
BOOL
PsuGetProcessImageFilePath(
    ULONG_PTR ProcessId,
    PWCHAR* ppwzImageFilePath,
    PULONG pcbImageFilePath
)
/*++

Description:

    Returns the NT file path of the image backing the target process.

Parameters:

    ProcessId - The process id of the target process.

    ppwzImageFilePath - Returns a pointer to an allocated buffer for the NT
        file path of the image backing the target process.

    pcbImageFilePath - Returns the size of the allocated buffer.

Remarks:

    If successful, the caller must free the returned buffer by calling
    MemFreeHeap.

--*/
{
    ULONG cbImageFilePath = 0;
    PWCHAR pwzImageFilePath = NULL;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *ppwzImageFilePath = NULL;
    *pcbImageFilePath = 0;

    status =
        PareidoliaIoGetProcessImageFilePathSize(ProcessId, &cbImageFilePath);
    if (!status)
    {
        ERR_PRINT("PareidoliaIoGetProcessImageFilePathSize failed: %u",
            GetLastError());
        goto exit;
    }

    pwzImageFilePath = (PWCHAR)MemAllocateHeap(cbImageFilePath);
    if (!pwzImageFilePath)
    {
        ERR_PRINT("MemAllocateHeap failed.");
        status = FALSE;
        goto exit;
    }

    status = PareidoliaIoGetProcessImageFilePath(
        ProcessId,
        pwzImageFilePath,
        cbImageFilePath);
    if (!status)
    {
        ERR_PRINT("PareidoliaIoGetProcessImageFilePath failed: %u",
            GetLastError());
        goto exit;
    }

    //
    // Set out parameters.
    //
    *ppwzImageFilePath = pwzImageFilePath;
    *pcbImageFilePath = cbImageFilePath;

exit:
    if (!status)
    {
        if (pwzImageFilePath)
        {
            VERIFY(MemFreeHeap(pwzImageFilePath));
        }
    }

    return status;
}
