/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "driver.h"

#include "debug.h"
#include "ntdll.h"

#include "../Common/ioctl.h"


//=============================================================================
// Private Types
//=============================================================================
typedef struct _DRIVER_CONTEXT {
    HANDLE DeviceHandle;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
static DRIVER_CONTEXT g_DriverContext = {};


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
BOOL
PareidoliaIoInitialization()
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    BOOL status = TRUE;

    hDevice = CreateFileW(
        PAREIDOLIA_LOCAL_DEVICE_PATH_U,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (INVALID_HANDLE_VALUE == hDevice)
    {
        status = FALSE;
        goto exit;
    }

    //
    // Initialize the global context.
    //
    g_DriverContext.DeviceHandle = hDevice;

exit:
    if (!status)
    {
        if (INVALID_HANDLE_VALUE != hDevice)
        {
            VERIFY(CloseHandle(hDevice));
        }
    }

    return status;
}


VOID
PareidoliaIoTermination()
{
    VERIFY(CloseHandle(g_DriverContext.DeviceHandle));
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
PareidoliaIoRestrictProcessAccess(
    ULONG_PTR ProcessId
)
{
    RESTRICT_PROCESS_ACCESS_REQUEST Request = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_RESTRICT_PROCESS_ACCESS,
        &Request,
        sizeof(Request),
        NULL,
        0,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
PareidoliaIoDerestrictProcessAccess(
    ULONG_PTR ProcessId
)
{
    DERESTRICT_PROCESS_ACCESS_REQUEST Request = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_DERESTRICT_PROCESS_ACCESS,
        &Request,
        sizeof(Request),
        NULL,
        0,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
PareidoliaIoGetProcessImageBase(
    ULONG_PTR ProcessId,
    PULONG_PTR pImageBase
)
{
    GET_PROCESS_IMAGE_BASE_REQUEST Request = {};
    GET_PROCESS_IMAGE_BASE_REPLY Reply = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pImageBase = 0;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_GET_PROCESS_IMAGE_BASE,
        &Request,
        sizeof(Request),
        &Reply,
        sizeof(Reply),
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pImageBase = Reply.ImageBase;

exit:
    return status;
}


_Use_decl_annotations_
BOOL
PareidoliaIoGetProcessImageFilePathSize(
    ULONG_PTR ProcessId,
    PULONG pcbSize
)
{
    GET_PROCESS_IMAGE_FILE_PATH_SIZE_REQUEST Request = {};
    GET_PROCESS_IMAGE_FILE_PATH_SIZE_REPLY Reply = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pcbSize = 0;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_GET_PROCESS_IMAGE_FILE_PATH_SIZE,
        &Request,
        sizeof(Request),
        &Reply,
        sizeof(Reply),
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pcbSize = Reply.Size;

exit:
    return status;
}


_Use_decl_annotations_
BOOL
PareidoliaIoGetProcessImageFilePath(
    ULONG_PTR ProcessId,
    PWCHAR pwzImageFilePath,
    ULONG cbImageFilePath
)
{
    GET_PROCESS_IMAGE_FILE_PATH_REQUEST Request = {};
    PGET_PROCESS_IMAGE_FILE_PATH_REPLY pReply = NULL;
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(pwzImageFilePath, cbImageFilePath);

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;

    //
    // Initialize the reply.
    //
    pReply = (PGET_PROCESS_IMAGE_FILE_PATH_REPLY)pwzImageFilePath;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_GET_PROCESS_IMAGE_FILE_PATH,
        &Request,
        sizeof(Request),
        pReply,
        cbImageFilePath,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
PareidoliaIoReadVirtualMemory(
    ULONG_PTR ProcessId,
    ULONG_PTR Address,
    PVOID pBuffer,
    ULONG cbBuffer
)
{
    READ_VIRTUAL_MEMORY_REQUEST Request = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(pBuffer, cbBuffer);

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;
    Request.Address = Address;
    Request.Size = cbBuffer;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_READ_VIRTUAL_MEMORY,
        &Request,
        sizeof(Request),
        pBuffer,
        cbBuffer,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}
