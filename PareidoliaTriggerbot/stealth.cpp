/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "stealth.h"

#include "log.h"
#include "nt.h"


//=============================================================================
// Private Types
//=============================================================================
typedef struct _UNLINK_DRIVER_OBJECT_CONTEXT {
    NTSTATUS NtStatus;
    PDRIVER_OBJECT DriverObject;
    volatile POINTER_ALIGNMENT BOOLEAN OperationCompleted;
    _Interlocked_ volatile POINTER_ALIGNMENT ULONG TargetProcessorNumber;
    _Interlocked_ volatile POINTER_ALIGNMENT LONG NumberOfActiveProcessors;
} UNLINK_DRIVER_OBJECT_CONTEXT, *PUNLINK_DRIVER_OBJECT_CONTEXT;


//=============================================================================
// Private Prototypes
//=============================================================================
EXTERN_C
static
KIPI_BROADCAST_WORKER
StlpUnlinkDriverObjectIpiCallback;

_Check_return_
EXTERN_C
static
BOOLEAN
StlpIsListValid(
    _In_ PLIST_ENTRY pListHead
);


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
StlIsUnlinkDriverObjectSupported()
/*++

Description:

    Query support status for unlinking a DRIVER_OBJECT from
    nt!PsLoadedModuleList.

Remarks:

    WARNING This is only supported on Windows 7. nt!PsLoadedModuleList is
    protected by PatchGuard on Windows 8+.

--*/
{
    RTL_OSVERSIONINFOEXW VersionInfo = {};
    ULONG TypeMask = 0;
    ULONGLONG ConditionMask = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);

    VersionInfo.dwMajorVersion = 6;
    VersionInfo.dwMinorVersion = 1;
    VersionInfo.dwPlatformId = VER_PLATFORM_WIN32_NT;
    VersionInfo.wProductType = VER_NT_WORKSTATION;

    TypeMask =
        VER_MAJORVERSION |
        VER_MINORVERSION |
        VER_PLATFORMID |
        VER_PRODUCT_TYPE;

    VER_SET_CONDITION(ConditionMask, VER_MAJORVERSION, VER_EQUAL);
    VER_SET_CONDITION(ConditionMask, VER_MINORVERSION, VER_EQUAL);
    VER_SET_CONDITION(ConditionMask, VER_PLATFORMID, VER_EQUAL);
    VER_SET_CONDITION(ConditionMask, VER_PRODUCT_TYPE, VER_EQUAL);

    ntstatus = RtlVerifyVersionInfo(&VersionInfo, TypeMask, ConditionMask);
    if (!NT_SUCCESS(ntstatus))
    {
        goto exit;
    }

exit:
    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
StlUnlinkDriverObject(
    PDRIVER_OBJECT pDriverObject
)
/*++

Description:

    Unlink the DRIVER_OBJECT from nt!PsLoadedModuleList so that the driver does
    not appear in the loaded driver list.

Parameters:

    pDriverObject - Pointer to the driver object to be unlinked.

Remarks:

    We attempt to unlink the driver object in the context of an IPI callback
    because the ERESOURCE (nt!PsLoadedModuleResource) guarding
    nt!PsLoadedModuleList is not exported by ntoskrnl.

    NOTE The loaded driver list can be queried using NtQuerySystemInformation
    with the SystemModuleInformation and SystemModuleInformationEx information
    classes.

--*/
{
    PUNLINK_DRIVER_OBJECT_CONTEXT pContext = NULL;
    ULONG nProcessors = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Unlinking driver object. (DriverObject = %p)", pDriverObject);

    //
    // Allocate the IPI callback context.
    //
    pContext = (PUNLINK_DRIVER_OBJECT_CONTEXT)ExAllocatePool(
        NonPagedPool,
        sizeof(*pContext));
    if (!pContext)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    //
    // Initialize the IPI callback context.
    //
    nProcessors = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

    NT_ASSERT(nProcessors);

    pContext->NtStatus = STATUS_INTERNAL_ERROR;
    pContext->DriverObject = pDriverObject;
    pContext->OperationCompleted = FALSE;
    pContext->TargetProcessorNumber = KeGetCurrentProcessorNumberEx(NULL);
    pContext->NumberOfActiveProcessors = nProcessors - 1;

    //
    // Execute the IPI callback.
    //
    KeIpiGenericCall(StlpUnlinkDriverObjectIpiCallback, (ULONG_PTR)pContext);

    ntstatus = pContext->NtStatus;

    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("StlpUnlinkDriverObjectIpiCallback failed: 0x%X", ntstatus);
        goto exit;
    }

exit:
    if (pContext)
    {
        ExFreePool(pContext);
    }

    return ntstatus;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
ULONG_PTR
StlpUnlinkDriverObjectIpiCallback(
    ULONG_PTR Argument
)
/*++

Description:

    Attempt to safely unlink a DRIVER_OBJECT from nt!PsLoadedModuleList without
    acquiring nt!PsLoadedModuleResource.

Parameters:

    Argument - Pointer to the callback context.

Remarks:

    This routine implements a synchronization barrier so that only one
    processor in the system attempts to unlink the target DRIVER_OBJECT at
    IPI_LEVEL irql.

--*/
{
    PUNLINK_DRIVER_OBJECT_CONTEXT pContext = NULL;
    PLDR_DATA_TABLE_ENTRY pLdrDataTableEntry = NULL;

    pContext = (PUNLINK_DRIVER_OBJECT_CONTEXT)Argument;

    //
    // Trap all processors except the target processor in a spin loop until
    //  the target processor finishes the operation.
    //
    if (KeGetCurrentProcessorNumberEx(NULL) != pContext->TargetProcessorNumber)
    {
        InterlockedDecrement(&pContext->NumberOfActiveProcessors);

        while (!pContext->OperationCompleted)
        {
            YieldProcessor();
        }

        goto exit;
    }

    //
    // The target processor falls through to perform the unlink operation.
    //
    pLdrDataTableEntry =
        (PLDR_DATA_TABLE_ENTRY)pContext->DriverObject->DriverSection;

    //
    // Wait until all of the other processors are trapped in the spin loop.
    //
    while (pContext->NumberOfActiveProcessors)
    {
        YieldProcessor();
    }

    //
    // Walk nt!PsLoadedModuleList to determine if we can unlink the target
    //  driver object without corrupting the loaded module list.
    //
    // We can safely walk the loaded module list without acquiring
    //  nt!PsLoadedModuleResource because:
    //
    //      1. All processors are synchronized at IPI_LEVEL irql.
    //
    //      2. Every processor except the target processor is trapped in the
    //          spin loop above.
    //
    //      3. ERESOURCE locks can only be (safely) acquired inside a critical
    //          region at irql <= APC_LEVEL.
    //
    if (StlpIsListValid(&pLdrDataTableEntry->InLoadOrderLinks))
    {
        //
        // Unlink the driver object.
        //
        RemoveEntryList(&pLdrDataTableEntry->InLoadOrderLinks);

        //
        // The flink and blink pointers of the unlinked driver object's
        //  InLoadOrderLinks field still point to entries in
        //  nt!PsLoadedModuleList. Reinitialize this list entry so that
        //  ntoskrnl does not corrupt nt!PsLoadedModuleList when the driver
        //  object is unlinked (again) inside nt!MmUnloadSystemImage.
        //
        InitializeListHead(&pLdrDataTableEntry->InLoadOrderLinks);

        pContext->NtStatus = STATUS_SUCCESS;
    }
    else
    {
        pContext->NtStatus = STATUS_UNSUCCESSFUL;
    }

    //
    // Update the callback completion state so that the other processors exit
    //  the spin loop.
    //
    pContext->OperationCompleted = TRUE;

exit:
    return 0;
}


_Use_decl_annotations_
EXTERN_C
static
BOOLEAN
StlpIsListValid(
    PLIST_ENTRY pListHead
)
{
    PLIST_ENTRY pEntry = NULL;
    BOOLEAN status = TRUE;

    for (pEntry = pListHead->Flink;
        pEntry != pListHead;
        pEntry = pEntry->Flink)
    {
        if (pEntry->Flink->Blink != pEntry ||
            pEntry->Blink->Flink != pEntry)
        {
            status = FALSE;
            goto exit;
        }
    }

exit:
    return status;
}
