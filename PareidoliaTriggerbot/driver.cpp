/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include <fltKernel.h>

#include "debug.h"
#include "log.h"
#include "nt.h"
#include "object_util.h"
#include "pareidolia_triggerbot.h"
#include "stealth.h"
#include "system_util.h"

#include "../Common/config.h"
#include "../Common/ioctl.h"

#include "../MouClassInputInjection/Common/ioctl.h"
#include "../MouClassInputInjection/MouClassInputInjection/driver.h"

#include "../VivienneVMM/common/driver_io_types.h"
#include "../VivienneVMM/VivienneVMM/vivienne.h"
#include "../VivienneVMM/VivienneVMM/HyperPlatform/HyperPlatform/driver.h"


//=============================================================================
// Private Prototypes
//=============================================================================
EXTERN_C
DRIVER_INITIALIZE
DriverEntry;

EXTERN_C
static
DRIVER_UNLOAD
DriverUnload;

_Dispatch_type_(IRP_MJ_CREATE)
EXTERN_C
static
DRIVER_DISPATCH
DispatchCreate;

_Dispatch_type_(IRP_MJ_CLOSE)
EXTERN_C
static
DRIVER_DISPATCH
DispatchClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
EXTERN_C
static
DRIVER_DISPATCH
DispatchDeviceControl;


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    BOOLEAN fVivienneVmmLoaded = FALSE;
    BOOLEAN fPareidoliaTriggerbotLoaded = FALSE;
    BOOLEAN fMouClassInputInjectionLoaded = FALSE;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Loading %ls.", PAREIDOLIA_NT_DEVICE_NAME_U);

    //
    // Modify our driver object so that we can register privileged callbacks.
    //
    // NOTE This is only required if the driver is not signed.
    //
    SysSetSignedKernelImageFlag(pDriverObject);

    //
    // Load the driver subsystems.
    //
    // NOTE Each subsystem will create its own named device object and modify
    //  fields in the driver object.
    //
    // NOTE We must load the VivienneVMM subsystem first because the other
    //  subsystems use its log module.
    //
    ntstatus = HyperPlatformDriverEntry(pDriverObject, pRegistryPath);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("HyperPlatformDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fVivienneVmmLoaded = TRUE;

    ntstatus = PareidoliaTriggerbotDriverEntry(pDriverObject, pRegistryPath);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PareidoliaTriggerbotDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fPareidoliaTriggerbotLoaded = TRUE;

    ntstatus = MouClassInputInjectionDriverEntry(pDriverObject, pRegistryPath);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MouClassInputInjectionDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fMouClassInputInjectionLoaded = TRUE;

    //
    // Set the driver dispatch routines after the subsystems are loaded so that
    //  all IO requests are sent to the forward routines.
    //
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
        DispatchDeviceControl;
    pDriverObject->DriverUnload = DriverUnload;

#if defined(CFG_UNLINK_DRIVER_OBJECT)
    //
    // Unlink the driver object from the loaded modules list.
    //
    // NOTE We must unlink the driver object after the subsystems are loaded
    //  because privileged callback registration requires our driver to be in
    //  the loaded modules list.
    //
    ntstatus = StlIsUnlinkDriverObjectSupported();
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("StlIsUnlinkDriverObjectSupported failed: 0x%X", ntstatus);
        goto exit;
    }

    ntstatus = StlUnlinkDriverObject(pDriverObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("StlUnlinkDriverObject failed: 0x%X", ntstatus);
        goto exit;
    }
#endif

    DBG_PRINT("%ls loaded.", PAREIDOLIA_NT_DEVICE_NAME_U);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (fMouClassInputInjectionLoaded)
        {
            MouClassInputInjectionDriverUnload(pDriverObject);
        }

        if (fPareidoliaTriggerbotLoaded)
        {
            PareidoliaTriggerbotDriverUnload(pDriverObject);
        }

        if (fVivienneVmmLoaded)
        {
            HyperPlatformDriverUnload(pDriverObject);
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
VOID
DriverUnload(
    PDRIVER_OBJECT pDriverObject
)
{
    UNICODE_STRING usSymbolicLinkName = {};

    DBG_PRINT("Unloading %ls.", PAREIDOLIA_NT_DEVICE_NAME_U);

    //
    // Unload the driver subsystems in reverse-load order.
    //
    MouClassInputInjectionDriverUnload(pDriverObject);
    PareidoliaTriggerbotDriverUnload(pDriverObject);
    HyperPlatformDriverUnload(pDriverObject);

    DBG_PRINT("%ls unloaded.", PAREIDOLIA_NT_DEVICE_NAME_U);
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
DispatchCreate(
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
static
NTSTATUS
DispatchClose(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    POBJECT_NAME_INFORMATION pObjectNameInfo = NULL;
    UNICODE_STRING VivienneVmmDeviceName = {};
    UNICODE_STRING PareidoliaTriggerbotDeviceName = {};
    UNICODE_STRING MouClassInputInjectionDeviceName = {};
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Processing IRP_MJ_CLOSE.");

    //
    // Query the name of the device object to be closed.
    //
    ntstatus = ObuQueryNameString(pDeviceObject, &pObjectNameInfo);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ObuQueryNameString failed: 0x%X (DeviceObject = %p)",
            ntstatus,
            pDeviceObject);
        goto exit;
    }

    DBG_PRINT("Closing device object. (Name = %wZ)", &pObjectNameInfo->Name);

    //
    // Forward the close request to the subsystem whose device object name
    //  matches the name of the device object to be closed.
    //
    VivienneVmmDeviceName = RTL_CONSTANT_STRING(
        NT_PATH_PREFIX_DEVICE CFG_DEVICE_NAME_VIVIENNE_VMM_U);

    PareidoliaTriggerbotDeviceName = RTL_CONSTANT_STRING(
        NT_PATH_PREFIX_DEVICE CFG_DEVICE_NAME_PAREIDOLIA_TRIGGERBOT_U);

    MouClassInputInjectionDeviceName = RTL_CONSTANT_STRING(
        NT_PATH_PREFIX_DEVICE CFG_DEVICE_NAME_MOUCLASS_INPUT_INJECTION_U);

    if (RtlEqualUnicodeString(
            &pObjectNameInfo->Name,
            &VivienneVmmDeviceName,
            TRUE))
    {
        VERIFY(VivienneVmmDispatchClose(pDeviceObject, pIrp));
    }
    else if (RtlEqualUnicodeString(
                &pObjectNameInfo->Name,
                &PareidoliaTriggerbotDeviceName,
                TRUE))
    {
        VERIFY(PareidoliaTriggerbotDispatchClose(pDeviceObject, pIrp));
    }
    else if (RtlEqualUnicodeString(
                &pObjectNameInfo->Name,
                &MouClassInputInjectionDeviceName,
                TRUE))
    {
        VERIFY(MouClassInputInjectionDispatchClose(pDeviceObject, pIrp));
    }
    else
    {
        ERR_PRINT(
            "Unexpected device object name. (DeviceObject = %p, Name = %wZ)",
            pDeviceObject,
            &pObjectNameInfo->Name);

        ntstatus = STATUS_INTERNAL_ERROR;

        //
        // NOTE We only complete the irp in the unhandled case because each
        //  subsystem is responsible for completing every irp they receive.
        //
        pIrp->IoStatus.Status = ntstatus;

        IoCompleteRequest(pIrp, IO_NO_INCREMENT);

        DEBUG_BREAK;

        goto exit;
    }

exit:
    if (pObjectNameInfo)
    {
        ExFreePool(pObjectNameInfo);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
DispatchDeviceControl(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    PIO_STACK_LOCATION pIrpStack = NULL;
    ULONG IoControlCode = 0;
    ULONG DeviceType = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    IoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;

    DeviceType = DEVICE_TYPE_FROM_CTL_CODE(IoControlCode);

    //
    // Forward the request based on the device type of the ioctl code.
    //
    switch (DeviceType)
    {
        case FILE_DEVICE_VVMM:
            ntstatus = VivienneVmmDispatchDeviceControl(pDeviceObject, pIrp);
            if (!NT_SUCCESS(ntstatus))
            {
                goto exit;
            }

            break;

        case FILE_DEVICE_PAREIDOLIA_TRIGGERBOT:
            ntstatus = PareidoliaTriggerbotDispatchDeviceControl(
                pDeviceObject,
                pIrp);
            if (!NT_SUCCESS(ntstatus))
            {
                goto exit;
            }

            break;

        case FILE_DEVICE_MOUCLASS_INPUT_INJECTION:
            ntstatus = MouClassInputInjectionDispatchDeviceControl(
                pDeviceObject,
                pIrp);
            if (!NT_SUCCESS(ntstatus))
            {
                goto exit;
            }

            break;

        default:
            ERR_PRINT(
                "Unhandled device type."
                " (DeviceType = %u, IoControlCode = 0x%X)",
                DeviceType,
                IoControlCode);

            ntstatus = STATUS_UNSUCCESSFUL;

            //
            // NOTE We only complete the irp in the unhandled case because each
            //  subsystem is responsible for completing every irp they receive.
            //
            pIrp->IoStatus.Information = 0;
            pIrp->IoStatus.Status = ntstatus;

            IoCompleteRequest(pIrp, IO_NO_INCREMENT);

            goto exit;
    }

exit:
    return ntstatus;
}
