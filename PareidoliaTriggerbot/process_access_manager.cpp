/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

Module:

    process access manager

Description:

    This module implements an interface for restricting user mode handle access
    to target processes.

Remarks:

    This module registers a kernel object callback to prevent user mode handle
    access to 'restricted' (protected) processes. The caller must be executing
    in a signed image. See SysSetSignedKernelImageFlag.

    WARNING Protecting a process may cause undesirable behavior. e.g., If the
    caller restricts access to a process that is executing in a command prompt
    then the user will not be able to close the command prompt via the red 'X'
    button. See CFG_ENABLE_PROCESS_ACCESS_PROTECTION.

    WARNING This module does not prevent all types of handle access. TODO List
    access types.

--*/

#include "process_access_manager.h"

#include "debug.h"
#include "log.h"


//=============================================================================
// Constants
//=============================================================================
#define MODULE_TITLE    "Process Access Manager"

#define OBJECT_CALLBACK_ALTITUDE_U  L"maP "


//=============================================================================
// Private Enumerations
//=============================================================================
typedef enum _OBJECT_OPERATION_TYPE {
    ObjectOperationProcess = 0,
    ObjectOperationThread = 1,
    MaxObjectOperationType = 2
} OBJECT_OPERATION_TYPE, *POBJECT_OPERATION_TYPE;

C_ASSERT(MAXUSHORT >= MaxObjectOperationType);


//=============================================================================
// Private Types
//=============================================================================
typedef struct _PROTECTED_PROCESS_ENTRY {
    LIST_ENTRY ListEntry;
    HANDLE ProcessId;
} PROTECTED_PROCESS_ENTRY, *PPROTECTED_PROCESS_ENTRY;

typedef struct _PROCESS_ACCESS_MANAGER {
    POB_CALLBACK_REGISTRATION ObjectCallbackRegistration;
    PVOID ObjectCallbackRegistrationHandle;
    POINTER_ALIGNMENT ERESOURCE Resource;
    _Guarded_by_(Resource) LIST_ENTRY ProtectedProcessListHead;
} PROCESS_ACCESS_MANAGER, *PPROCESS_ACCESS_MANAGER;


//=============================================================================
// Module Globals
//=============================================================================
static PROCESS_ACCESS_MANAGER g_ProcessAccessManager = {};


//=============================================================================
// Private Prototypes
//=============================================================================
EXTERN_C
static
VOID
PampProcessNotifyRoutine(
    _In_ HANDLE ParentId,
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN fCreate
);

_Check_return_
EXTERN_C
static
OB_PREOP_CALLBACK_STATUS
PampPreOperationCallback(
    _In_ PVOID pRegistrationContext,
    _Inout_ POB_PRE_OPERATION_INFORMATION pOperationInformation
);

EXTERN_C
static
VOID
PampPostOperationCallback(
    _In_ PVOID pRegistrationContext,
    _In_ POB_POST_OPERATION_INFORMATION pOperationInformation
);

_Check_return_
EXTERN_C
static
NTSTATUS
PampCreateObjectCallbackRegistration(
    _In_ PVOID pContext,
    _Outptr_result_nullonfailure_ POB_CALLBACK_REGISTRATION* ppRegistration
);

_Requires_exclusive_lock_held_(g_ProcessAccessManager.Resource)
_Check_return_
EXTERN_C
static
PPROTECTED_PROCESS_ENTRY
PampGetProtectedProcess(
    _In_ HANDLE ProcessId
);

_Requires_lock_not_held_(g_ProcessAccessManager.Resource)
_IRQL_requires_max_(APC_LEVEL)
_Check_return_
EXTERN_C
static
VOID
PampFreeProtectedProcessList();


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
PamDriverEntry()
{
    BOOLEAN fResourceInitialized = FALSE;
    BOOLEAN fProcessNotifyCallbackRegistered = FALSE;
    POB_CALLBACK_REGISTRATION pObjectCallbackRegistration = NULL;
    PVOID ObjectCallbackRegistrationHandle = NULL;
    BOOLEAN fObjectCallbacksRegistered = FALSE;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Loading %s.", MODULE_TITLE);

    //
    // We must initialize the resource before registering callbacks because our
    //  callbacks use the resource.
    //
    ntstatus = ExInitializeResourceLite(&g_ProcessAccessManager.Resource);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ExInitializeResourceLite failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fResourceInitialized = TRUE;

    ntstatus =
        PsSetCreateProcessNotifyRoutine(PampProcessNotifyRoutine, FALSE);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsSetCreateProcessNotifyRoutine failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fProcessNotifyCallbackRegistered = TRUE;

    ntstatus = PampCreateObjectCallbackRegistration(
        &g_ProcessAccessManager,
        &pObjectCallbackRegistration);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PampCreateObjectCallbackRegistration failed: 0x%X",
            ntstatus);
        goto exit;
    }

    ntstatus = ObRegisterCallbacks(
        pObjectCallbackRegistration,
        &ObjectCallbackRegistrationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ObRegisterCallbacks failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fObjectCallbacksRegistered = TRUE;

    //
    // Initialize the global context.
    //
    g_ProcessAccessManager.ObjectCallbackRegistration =
        pObjectCallbackRegistration;
    g_ProcessAccessManager.ObjectCallbackRegistrationHandle =
        ObjectCallbackRegistrationHandle;
    InitializeListHead(&g_ProcessAccessManager.ProtectedProcessListHead);

    DBG_PRINT("%s loaded.", MODULE_TITLE);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (fObjectCallbacksRegistered)
        {
            ObUnRegisterCallbacks(ObjectCallbackRegistrationHandle);
        }

        if (pObjectCallbackRegistration)
        {
            ExFreePool(pObjectCallbackRegistration);
        }

        if (fProcessNotifyCallbackRegistered)
        {
            VERIFY(PsSetCreateProcessNotifyRoutine(
                PampProcessNotifyRoutine,
                TRUE));
        }

        if (fResourceInitialized)
        {
            VERIFY(ExDeleteResourceLite(&g_ProcessAccessManager.Resource));
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
PamDriverUnload()
{
    DBG_PRINT("Unloading %s.", MODULE_TITLE);

    PampFreeProtectedProcessList();

    ObUnRegisterCallbacks(
        g_ProcessAccessManager.ObjectCallbackRegistrationHandle);

    ExFreePool(g_ProcessAccessManager.ObjectCallbackRegistration);

    VERIFY(PsSetCreateProcessNotifyRoutine(PampProcessNotifyRoutine, TRUE));

    VERIFY(ExDeleteResourceLite(&g_ProcessAccessManager.Resource));

    DBG_PRINT("%s unloaded.", MODULE_TITLE);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
PamRestrictAccess(
    HANDLE ProcessId
)
/*++

Description:

    Register the specified process as a protected process so that user mode
    code cannot create or duplicate a handle to the specified process.

Parameters:

    ProcessId - The process to be registered.

Remarks:

    WARNING This module does not prevent all types of handle access. TODO List
    access types.

--*/
{
    PPROTECTED_PROCESS_ENTRY pProtectedProcessEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    ExEnterCriticalRegionAndAcquireResourceExclusive(
        &g_ProcessAccessManager.Resource);

    //
    // Fail if the specified process is already registered.
    //
    if (PampGetProtectedProcess(ProcessId))
    {
        ntstatus = STATUS_ALREADY_REGISTERED;
        goto exit;
    }

    //
    // Allocate and initialize a new protected process entry.
    //
    pProtectedProcessEntry = (PPROTECTED_PROCESS_ENTRY)ExAllocatePool(
        PagedPool,
        sizeof(*pProtectedProcessEntry));
    if (!pProtectedProcessEntry)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    pProtectedProcessEntry->ProcessId = ProcessId;

    //
    // Insert the new entry into the protected process list.
    //
    InsertTailList(
        &g_ProcessAccessManager.ProtectedProcessListHead,
        &pProtectedProcessEntry->ListEntry);

    INF_PRINT("Restricted process access. (ProcessId = %Iu)",
        pProtectedProcessEntry->ProcessId);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (pProtectedProcessEntry)
        {
            ExFreePool(pProtectedProcessEntry);
        }
    }

    ExReleaseResourceAndLeaveCriticalRegion(&g_ProcessAccessManager.Resource);

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PamDerestrictAccess(
    HANDLE ProcessId
)
/*++

Description:

    Unregister a protected process.

Parameters:

    ProcessId - The process to be unregistered.

--*/
{
    PPROTECTED_PROCESS_ENTRY pProtectedProcessEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    ExEnterCriticalRegionAndAcquireResourceExclusive(
        &g_ProcessAccessManager.Resource);

    //
    // Fail if the specified process is not registered.
    //
    pProtectedProcessEntry = PampGetProtectedProcess(ProcessId);
    if (!pProtectedProcessEntry)
    {
        ntstatus = STATUS_NOT_FOUND;
        goto exit;
    }

    //
    // Remove the entry from the protected process list.
    //
    RemoveEntryList(&pProtectedProcessEntry->ListEntry);

    INF_PRINT("Derestricted process access. (ProcessId = %Iu)",
        pProtectedProcessEntry->ProcessId);

    ExFreePool(pProtectedProcessEntry);

exit:
    ExReleaseResourceAndLeaveCriticalRegion(&g_ProcessAccessManager.Resource);

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
PamReset()
/*++

Description:

    Unregister all protected processes.

--*/
{
    INF_PRINT("Resetting %s.", MODULE_TITLE);

    PampFreeProtectedProcessList();
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
VOID
PampProcessNotifyRoutine(
    HANDLE ParentId,
    HANDLE ProcessId,
    BOOLEAN fCreate
)
{
    BOOLEAN fResourceAcquired = FALSE;
    PPROTECTED_PROCESS_ENTRY pProtectedProcessEntry = NULL;

    UNREFERENCED_PARAMETER(ParentId);

    //
    // Ignore process creation.
    //
    if (fCreate)
    {
        goto exit;
    }

    ExEnterCriticalRegionAndAcquireResourceExclusive(
        &g_ProcessAccessManager.Resource);
    fResourceAcquired = TRUE;

    //
    // If a registered process is terminating then unregister it.
    //
    pProtectedProcessEntry = PampGetProtectedProcess(ProcessId);
    if (!pProtectedProcessEntry)
    {
        goto exit;
    }

    RemoveEntryList(&pProtectedProcessEntry->ListEntry);

    INF_PRINT("Derestricted process access. (ProcessId = %Iu)",
        pProtectedProcessEntry->ProcessId);

    ExFreePool(pProtectedProcessEntry);

exit:
    if (fResourceAcquired)
    {
        ExReleaseResourceAndLeaveCriticalRegion(
            &g_ProcessAccessManager.Resource);
    }
}


_Use_decl_annotations_
EXTERN_C
static
OB_PREOP_CALLBACK_STATUS
PampPreOperationCallback(
    PVOID pRegistrationContext,
    POB_PRE_OPERATION_INFORMATION pOperationInformation
)
{
    PPROCESS_ACCESS_MANAGER pProcessAccessManager = NULL;
    BOOLEAN fResourceAcquired = FALSE;
    HANDLE ProcessId = NULL;
    POB_PRE_CREATE_HANDLE_INFORMATION pCreateHandleInfo = NULL;
    POB_PRE_DUPLICATE_HANDLE_INFORMATION pDuplicateHandleInfo = NULL;
    PACCESS_MASK pDesiredAccess = NULL;
    ACCESS_MASK OriginalDesiredAccess = NULL;
    OB_PREOP_CALLBACK_STATUS status = OB_PREOP_SUCCESS;

    //
    // Always allow the requested access for kernel handles.
    //
    if (pOperationInformation->KernelHandle)
    {
        goto exit;
    }

    //
    // Determine the effective process id for the request.
    //
    if (pOperationInformation->ObjectType == (*PsProcessType))
    {
        ProcessId = PsGetProcessId((PEPROCESS)pOperationInformation->Object);
    }
    else if (pOperationInformation->ObjectType == (*PsThreadType))
    {
        ProcessId =
            PsGetThreadProcessId((PETHREAD)pOperationInformation->Object);
    }
    else
    {
        ERR_PRINT("Unexpected object type. (ObjectType = %p)",
            pOperationInformation->ObjectType);
        DEBUG_BREAK;
        goto exit;
    }

    pProcessAccessManager = (PPROCESS_ACCESS_MANAGER)pRegistrationContext;

    ExEnterCriticalRegionAndAcquireResourceExclusive(
        &pProcessAccessManager->Resource);
    fResourceAcquired = TRUE;

    //
    // Allow the requested access if the process is not protected.
    //
    if (!PampGetProtectedProcess(ProcessId))
    {
        goto exit;
    }

    switch (pOperationInformation->Operation)
    {
        case OB_OPERATION_HANDLE_CREATE:
            pCreateHandleInfo =
                &pOperationInformation->Parameters->CreateHandleInformation;
            pDesiredAccess = &pCreateHandleInfo->DesiredAccess;
            OriginalDesiredAccess = pCreateHandleInfo->OriginalDesiredAccess;
            break;

        case OB_OPERATION_HANDLE_DUPLICATE:
            pDuplicateHandleInfo =
                &pOperationInformation->Parameters->DuplicateHandleInformation;
            pDesiredAccess = &pDuplicateHandleInfo->DesiredAccess;
            OriginalDesiredAccess =
                pDuplicateHandleInfo->OriginalDesiredAccess;
            break;

        default:
            ERR_PRINT("Unexpected operation. (Operation = %p)",
                pOperationInformation->Operation);
            DEBUG_BREAK;
            goto exit;
    }

    //
    // Prevent the requested access.
    //
    *pDesiredAccess = 0;

exit:
    if (fResourceAcquired)
    {
        ExReleaseResourceAndLeaveCriticalRegion(
            &pProcessAccessManager->Resource);
    }

    return status;
}


_Use_decl_annotations_
EXTERN_C
static
VOID
PampPostOperationCallback(
    PVOID pRegistrationContext,
    POB_POST_OPERATION_INFORMATION pOperationInformation
)
{
    //
    // TODO Verify that we prevented access to protected processes.
    //
    UNREFERENCED_PARAMETER(pRegistrationContext);
    UNREFERENCED_PARAMETER(pOperationInformation);
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
PampCreateObjectCallbackRegistration(
    PVOID pContext,
    POB_CALLBACK_REGISTRATION* ppRegistration
)
/*++

Description:

    Allocate and initialize an object callback registration object.

Parameters:

    pContext - Pointer to the registration context.

    ppRegistration - Returns an initialized object callback registration
        object. This object is allocated from the NonPaged pool.

Remarks:

    If successful, the caller must free the returned registration object by
    calling ExFreePool.

--*/
{
    USHORT nOperationEntries = 0;
    SIZE_T cbCallbackRegistration = 0;
    POB_CALLBACK_REGISTRATION pCallbackRegistration = NULL;
    POB_OPERATION_REGISTRATION pOperationRegistration = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(ppRegistration, sizeof(*ppRegistration));

    DBG_PRINT("Creating object callback registration.");

    //
    // NOTE The OB_CALLBACK_REGISTRATION object and its
    //  OB_OPERATION_REGISTRATION array must be contiguous.
    //
    nOperationEntries = MaxObjectOperationType;

    cbCallbackRegistration =
        sizeof(*pCallbackRegistration) +
        (sizeof(OB_OPERATION_REGISTRATION) * nOperationEntries);

    pCallbackRegistration = (POB_CALLBACK_REGISTRATION)ExAllocatePool(
        NonPagedPool,
        cbCallbackRegistration);
    if (!pCallbackRegistration)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    pOperationRegistration = (POB_OPERATION_REGISTRATION)(
        (ULONG_PTR)pCallbackRegistration + sizeof(*pCallbackRegistration));

    //
    // Initialize the process object operation entry.
    //
    pOperationRegistration[ObjectOperationProcess].ObjectType = PsProcessType;
    pOperationRegistration[ObjectOperationProcess].Operations =
        OB_OPERATION_HANDLE_CREATE |
        OB_OPERATION_HANDLE_DUPLICATE;
    pOperationRegistration[ObjectOperationProcess].PreOperation =
        PampPreOperationCallback;
    pOperationRegistration[ObjectOperationProcess].PostOperation =
        PampPostOperationCallback;

    //
    // Initialize the thread object operation entry.
    //
    pOperationRegistration[ObjectOperationThread].ObjectType = PsThreadType;
    pOperationRegistration[ObjectOperationThread].Operations =
        OB_OPERATION_HANDLE_CREATE |
        OB_OPERATION_HANDLE_DUPLICATE;
    pOperationRegistration[ObjectOperationThread].PreOperation =
        PampPreOperationCallback;
    pOperationRegistration[ObjectOperationThread].PostOperation =
        PampPostOperationCallback;

    //
    // Initialize the callback registration object.
    //
    pCallbackRegistration->Version = OB_FLT_REGISTRATION_VERSION;
    pCallbackRegistration->OperationRegistrationCount = nOperationEntries;
    pCallbackRegistration->Altitude =
        RTL_CONSTANT_STRING(OBJECT_CALLBACK_ALTITUDE_U);
    pCallbackRegistration->RegistrationContext = pContext;
    pCallbackRegistration->OperationRegistration = pOperationRegistration;

    //
    // Set out parameters.
    //
    *ppRegistration = pCallbackRegistration;

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (pCallbackRegistration)
        {
            ExFreePool(pCallbackRegistration);
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
PPROTECTED_PROCESS_ENTRY
PampGetProtectedProcess(
    HANDLE ProcessId
)
{
    PLIST_ENTRY pListEntry = NULL;
    PPROTECTED_PROCESS_ENTRY pEntry = NULL;
    PPROTECTED_PROCESS_ENTRY pProtectedProcessEntry = NULL;

    for (pListEntry = g_ProcessAccessManager.ProtectedProcessListHead.Flink;
        pListEntry != &g_ProcessAccessManager.ProtectedProcessListHead;
        pListEntry = pListEntry->Flink)
    {
        pEntry =
            CONTAINING_RECORD(pListEntry, PROTECTED_PROCESS_ENTRY, ListEntry);

        if (pEntry->ProcessId == ProcessId)
        {
            pProtectedProcessEntry = pEntry;
            break;
        }
    }

    return pProtectedProcessEntry;
}


_Use_decl_annotations_
EXTERN_C
static
VOID
PampFreeProtectedProcessList()
{
    PLIST_ENTRY pListHead = NULL;
    PLIST_ENTRY pListEntry = NULL;
    PPROTECTED_PROCESS_ENTRY pEntry = NULL;

    pListHead = &g_ProcessAccessManager.ProtectedProcessListHead;

    ExEnterCriticalRegionAndAcquireResourceExclusive(
        &g_ProcessAccessManager.Resource);

    while (!IsListEmpty(pListHead))
    {
        pListEntry = RemoveHeadList(pListHead);
        pEntry =
            CONTAINING_RECORD(pListEntry, PROTECTED_PROCESS_ENTRY, ListEntry);

        INF_PRINT("Derestricted process access. (ProcessId = %Iu)",
            pEntry->ProcessId);

        ExFreePool(pEntry);
    }

    ExReleaseResourceAndLeaveCriticalRegion(&g_ProcessAccessManager.Resource);
}
