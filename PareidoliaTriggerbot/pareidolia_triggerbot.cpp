/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "pareidolia_triggerbot.h"

#include "debug.h"
#include "log.h"
#include "process_access_manager.h"
#include "process_util.h"

#include "../Common/config.h"
#include "../Common/ioctl.h"


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
PareidoliaTriggerbotDriverEntry(
    PDRIVER_OBJECT pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    PDEVICE_OBJECT pDeviceObject = NULL;
    UNICODE_STRING usDeviceName = {};
    UNICODE_STRING usSymbolicLinkName = {};
    BOOLEAN fSymbolicLinkCreated = FALSE;
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    BOOLEAN fPamLoaded = FALSE;
#endif
    NTSTATUS ntstatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(pRegistryPath);

    DBG_PRINT("Loading %ls. (Subsystem)", PAREIDOLIA_NT_DEVICE_NAME_U);

    usDeviceName = RTL_CONSTANT_STRING(PAREIDOLIA_NT_DEVICE_NAME_U);

    ntstatus = IoCreateDevice(
        pDriverObject,
        0,
        &usDeviceName,
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,
        FILE_DEVICE_SECURE_OPEN,
        TRUE,
        &pDeviceObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IoCreateDevice failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    pDriverObject->MajorFunction[IRP_MJ_CREATE] =
        PareidoliaTriggerbotDispatchCreate;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] =
        PareidoliaTriggerbotDispatchClose;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
        PareidoliaTriggerbotDispatchDeviceControl;
    pDriverObject->DriverUnload = PareidoliaTriggerbotDriverUnload;

    //
    // Create a symbolic link for the user mode client.
    //
    usSymbolicLinkName = RTL_CONSTANT_STRING(PAREIDOLIA_SYMBOLIC_LINK_NAME_U);

    ntstatus = IoCreateSymbolicLink(&usSymbolicLinkName, &usDeviceName);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IoCreateSymbolicLink failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fSymbolicLinkCreated = TRUE;

    //
    // Load the driver modules.
    //
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    ntstatus = PamDriverEntry();
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PamDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fPamLoaded = TRUE;
#endif

    DBG_PRINT("%ls loaded. (Subsystem)", PAREIDOLIA_NT_DEVICE_NAME_U);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
        if (fPamLoaded)
        {
            PamDriverUnload();
        }
#endif

        if (fSymbolicLinkCreated)
        {
            VERIFY(IoDeleteSymbolicLink(&usSymbolicLinkName));
        }

        if (pDeviceObject)
        {
            IoDeleteDevice(pDeviceObject);
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
PareidoliaTriggerbotDriverUnload(
    PDRIVER_OBJECT pDriverObject
)
{
    UNICODE_STRING usSymbolicLinkName = {};

    DBG_PRINT("Unloading %ls. (Subsystem)", PAREIDOLIA_NT_DEVICE_NAME_U);

    //
    // Unload the driver modules.
    //
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    PamDriverUnload();
#endif

    //
    // Release driver resources.
    //
    usSymbolicLinkName = RTL_CONSTANT_STRING(PAREIDOLIA_SYMBOLIC_LINK_NAME_U);

    VERIFY(IoDeleteSymbolicLink(&usSymbolicLinkName));

    if (pDriverObject->DeviceObject)
    {
        IoDeleteDevice(pDriverObject->DeviceObject);
    }

    DBG_PRINT("%ls unloaded. (Subsystem)", PAREIDOLIA_NT_DEVICE_NAME_U);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
PareidoliaTriggerbotDispatchCreate(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    DBG_PRINT("Processing IRP_MJ_CREATE.");
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PareidoliaTriggerbotDispatchClose(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);

    DBG_PRINT("Processing IRP_MJ_CLOSE.");

    //
    // Remove access protection from all protected processes.
    //
    PamReset();

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PareidoliaTriggerbotDispatchDeviceControl(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    PIO_STACK_LOCATION pIrpStack = NULL;
    PVOID pSystemBuffer = NULL;
    ULONG cbInput = 0;
    ULONG cbOutput = 0;
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
    PRESTRICT_PROCESS_ACCESS_REQUEST pRestrictProcessAccessRequest = NULL;
    PDERESTRICT_PROCESS_ACCESS_REQUEST pDerestrictProcessAccessRequest = NULL;
#endif
    PGET_PROCESS_IMAGE_BASE_REQUEST pGetProcessImageBaseRequest = NULL;
    PGET_PROCESS_IMAGE_BASE_REPLY pGetProcessImageBaseReply = NULL;
    PGET_PROCESS_IMAGE_FILE_PATH_SIZE_REQUEST
        pGetProcessImageFilePathSizeRequest = NULL;
    PGET_PROCESS_IMAGE_FILE_PATH_SIZE_REPLY
        pGetProcessImageFilePathSizeReply = NULL;
    PGET_PROCESS_IMAGE_FILE_PATH_REQUEST
        pGetProcessImageFilePathRequest = NULL;
    PGET_PROCESS_IMAGE_FILE_PATH_REPLY pGetProcessImageFilePathReply = NULL;
    ULONG cbRequired = 0;
    PREAD_VIRTUAL_MEMORY_REQUEST pReadVirtualMemoryRequest = NULL;
    ULONG cbRead = 0;
    ULONG_PTR Information = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    pSystemBuffer = pIrp->AssociatedIrp.SystemBuffer;
    cbInput = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;
    cbOutput = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

    UNREFERENCED_PARAMETER(pDeviceObject);

    switch (pIrpStack->Parameters.DeviceIoControl.IoControlCode)
    {
#if defined(CFG_ENABLE_PROCESS_ACCESS_PROTECTION)
        case IOCTL_RESTRICT_PROCESS_ACCESS:
            DBG_PRINT("Processing IOCTL_RESTRICT_PROCESS_ACCESS.");

            pRestrictProcessAccessRequest =
                (PRESTRICT_PROCESS_ACCESS_REQUEST)pSystemBuffer;
            if (!pRestrictProcessAccessRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pRestrictProcessAccessRequest) != cbInput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            ntstatus = PamRestrictAccess(
                (HANDLE)pRestrictProcessAccessRequest->ProcessId);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("PamRestrictAccess failed: 0x%X", ntstatus);
                goto exit;
            }

            break;

        case IOCTL_DERESTRICT_PROCESS_ACCESS:
            DBG_PRINT("Processing IOCTL_DERESTRICT_PROCESS_ACCESS.");

            pDerestrictProcessAccessRequest =
                (PDERESTRICT_PROCESS_ACCESS_REQUEST)pSystemBuffer;
            if (!pDerestrictProcessAccessRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pDerestrictProcessAccessRequest) != cbInput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            ntstatus = PamDerestrictAccess(
                (HANDLE)pDerestrictProcessAccessRequest->ProcessId);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("PamDerestrictAccess failed: 0x%X", ntstatus);
                goto exit;
            }

            break;
#endif

        case IOCTL_GET_PROCESS_IMAGE_BASE:
            DBG_PRINT("Processing IOCTL_GET_PROCESS_IMAGE_BASE.");

            pGetProcessImageBaseRequest =
                (PGET_PROCESS_IMAGE_BASE_REQUEST)pSystemBuffer;
            if (!pGetProcessImageBaseRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pGetProcessImageBaseRequest) != cbInput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            pGetProcessImageBaseReply =
                (PGET_PROCESS_IMAGE_BASE_REPLY)pSystemBuffer;
            if (!pGetProcessImageBaseReply)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pGetProcessImageBaseReply) != cbOutput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            ntstatus = PsuGetImageBase(
                (HANDLE)pGetProcessImageBaseRequest->ProcessId,
                &pGetProcessImageBaseReply->ImageBase);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("PsuGetImageBase failed: 0x%X", ntstatus);
                goto exit;
            }

            Information = sizeof(*pGetProcessImageBaseReply);

            break;

        case IOCTL_GET_PROCESS_IMAGE_FILE_PATH_SIZE:
            DBG_PRINT("Processing IOCTL_GET_PROCESS_IMAGE_FILE_PATH_SIZE.");

            pGetProcessImageFilePathSizeRequest =
                (PGET_PROCESS_IMAGE_FILE_PATH_SIZE_REQUEST)pSystemBuffer;
            if (!pGetProcessImageFilePathSizeRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pGetProcessImageFilePathSizeRequest) != cbInput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            pGetProcessImageFilePathSizeReply =
                (PGET_PROCESS_IMAGE_FILE_PATH_SIZE_REPLY)pSystemBuffer;
            if (!pGetProcessImageFilePathSizeReply)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pGetProcessImageFilePathSizeReply) != cbOutput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            ntstatus = PsuGetImageFilePath(
                (HANDLE)pGetProcessImageFilePathSizeRequest->ProcessId,
                NULL,
                0,
                &pGetProcessImageFilePathSizeReply->Size);
            if (STATUS_INFO_LENGTH_MISMATCH != ntstatus)
            {
                ERR_PRINT("PsuGetImageFilePath failed: 0x%X (Unexpected)",
                    ntstatus);
                goto exit;
            }

            Information = sizeof(*pGetProcessImageFilePathSizeReply);

            ntstatus = STATUS_SUCCESS;

            break;

        case IOCTL_GET_PROCESS_IMAGE_FILE_PATH:
            DBG_PRINT("Processing IOCTL_GET_PROCESS_IMAGE_FILE_PATH.");

            pGetProcessImageFilePathRequest =
                (PGET_PROCESS_IMAGE_FILE_PATH_REQUEST)pSystemBuffer;
            if (!pGetProcessImageFilePathRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pGetProcessImageFilePathRequest) != cbInput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            pGetProcessImageFilePathReply =
                (PGET_PROCESS_IMAGE_FILE_PATH_REPLY)pSystemBuffer;
            if (!pGetProcessImageFilePathReply)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pGetProcessImageFilePathReply) > cbOutput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            ntstatus = PsuGetImageFilePath(
                (HANDLE)pGetProcessImageFilePathRequest->ProcessId,
                (PWCHAR)&pGetProcessImageFilePathReply->NtFilePath,
                cbOutput,
                &cbRequired);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("PsuGetImageFilePath failed: 0x%X", ntstatus);
                goto exit;
            }

            Information = cbRequired;

            break;

        case IOCTL_READ_VIRTUAL_MEMORY:
            DBG_PRINT("Processing IOCTL_READ_VIRTUAL_MEMORY.");

            pReadVirtualMemoryRequest =
                (PREAD_VIRTUAL_MEMORY_REQUEST)pSystemBuffer;
            if (!pReadVirtualMemoryRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER;
                goto exit;
            }

            if (sizeof(*pReadVirtualMemoryRequest) != cbInput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            if (pReadVirtualMemoryRequest->Size > cbOutput)
            {
                ntstatus = STATUS_INFO_LENGTH_MISMATCH;
                goto exit;
            }

            ntstatus = PsuReadVirtualMemory(
                (HANDLE)pReadVirtualMemoryRequest->ProcessId,
                pReadVirtualMemoryRequest->Address,
                pSystemBuffer,
                pReadVirtualMemoryRequest->Size,
                &cbRead);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("PsuReadVirtualMemory failed: 0x%X", ntstatus);
                goto exit;
            }

            Information = cbRead;

            break;

        default:
            ERR_PRINT(
                "Unhandled IOCTL."
                " (MajorFunction = %hhu, MinorFunction = %hhu)",
                pIrpStack->MajorFunction,
                pIrpStack->MinorFunction);
            ntstatus = STATUS_UNSUCCESSFUL;
            goto exit;
    }

exit:
    pIrp->IoStatus.Information = Information;
    pIrp->IoStatus.Status = ntstatus;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return ntstatus;
}
