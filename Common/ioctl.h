/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#if defined(_KERNEL_MODE)
#include <fltKernel.h>
#else
#include <Windows.h>
#endif

#include <devioctl.h>

#include "../Common/config.h"

//=============================================================================
// Names
//=============================================================================
#define PAREIDOLIA_DRIVER_NAME_U        CFG_DEVICE_NAME_PAREIDOLIA_TRIGGERBOT_U
#define PAREIDOLIA_LOCAL_DEVICE_PATH_U  (L"\\\\.\\" PAREIDOLIA_DRIVER_NAME_U)
#define PAREIDOLIA_NT_DEVICE_NAME_U     \
    (L"\\Device\\" PAREIDOLIA_DRIVER_NAME_U)
#define PAREIDOLIA_SYMBOLIC_LINK_NAME_U \
    (L"\\DosDevices\\" PAREIDOLIA_DRIVER_NAME_U)

//=============================================================================
// Ioctls
//=============================================================================
#define FILE_DEVICE_PAREIDOLIA_TRIGGERBOT   41499ul

#define IOCTL_RESTRICT_PROCESS_ACCESS       \
    CTL_CODE(                               \
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,  \
        3900,                               \
        METHOD_BUFFERED,                    \
        FILE_ANY_ACCESS)

#define IOCTL_DERESTRICT_PROCESS_ACCESS     \
    CTL_CODE(                               \
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,  \
        3901,                               \
        METHOD_BUFFERED,                    \
        FILE_ANY_ACCESS)

#define IOCTL_GET_PROCESS_IMAGE_BASE        \
    CTL_CODE(                               \
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,  \
        4000,                               \
        METHOD_BUFFERED,                    \
        FILE_ANY_ACCESS)

#define IOCTL_GET_PROCESS_IMAGE_FILE_PATH_SIZE  \
    CTL_CODE(                                   \
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,      \
        4001,                                   \
        METHOD_BUFFERED,                        \
        FILE_ANY_ACCESS)

#define IOCTL_GET_PROCESS_IMAGE_FILE_PATH   \
    CTL_CODE(                               \
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,  \
        4002,                               \
        METHOD_BUFFERED,                    \
        FILE_ANY_ACCESS)

#define IOCTL_READ_VIRTUAL_MEMORY           \
    CTL_CODE(                               \
        FILE_DEVICE_PAREIDOLIA_TRIGGERBOT,  \
        4100,                               \
        METHOD_BUFFERED,                    \
        FILE_ANY_ACCESS)

//=============================================================================
// IOCTL_RESTRICT_PROCESS_ACCESS
//=============================================================================
typedef struct _RESTRICT_PROCESS_ACCESS_REQUEST {
    ULONG_PTR ProcessId;
} RESTRICT_PROCESS_ACCESS_REQUEST, *PRESTRICT_PROCESS_ACCESS_REQUEST;

//=============================================================================
// IOCTL_DERESTRICT_PROCESS_ACCESS
//=============================================================================
typedef struct _DERESTRICT_PROCESS_ACCESS_REQUEST {
    ULONG_PTR ProcessId;
} DERESTRICT_PROCESS_ACCESS_REQUEST, *PDERESTRICT_PROCESS_ACCESS_REQUEST;

//=============================================================================
// IOCTL_GET_PROCESS_IMAGE_BASE
//=============================================================================
typedef struct _GET_PROCESS_IMAGE_BASE_REQUEST {
    ULONG_PTR ProcessId;
} GET_PROCESS_IMAGE_BASE_REQUEST, *PGET_PROCESS_IMAGE_BASE_REQUEST;

typedef struct _GET_PROCESS_IMAGE_BASE_REPLY {
    ULONG_PTR ImageBase;
} GET_PROCESS_IMAGE_BASE_REPLY, *PGET_PROCESS_IMAGE_BASE_REPLY;

//=============================================================================
// IOCTL_GET_PROCESS_IMAGE_FILE_PATH_SIZE
//=============================================================================
typedef struct _GET_PROCESS_IMAGE_FILE_PATH_SIZE_REQUEST {
    ULONG_PTR ProcessId;
} GET_PROCESS_IMAGE_FILE_PATH_SIZE_REQUEST,
*PGET_PROCESS_IMAGE_FILE_PATH_SIZE_REQUEST;

typedef struct _GET_PROCESS_IMAGE_FILE_PATH_SIZE_REPLY {
    ULONG Size;
} GET_PROCESS_IMAGE_FILE_PATH_SIZE_REPLY,
*PGET_PROCESS_IMAGE_FILE_PATH_SIZE_REPLY;

//=============================================================================
// IOCTL_GET_PROCESS_IMAGE_FILE_PATH
//=============================================================================
typedef struct _GET_PROCESS_IMAGE_FILE_PATH_REQUEST {
    ULONG_PTR ProcessId;
} GET_PROCESS_IMAGE_FILE_PATH_REQUEST, *PGET_PROCESS_IMAGE_FILE_PATH_REQUEST;

typedef struct _GET_PROCESS_IMAGE_FILE_PATH_REPLY {
    WCHAR NtFilePath[ANYSIZE_ARRAY];
} GET_PROCESS_IMAGE_FILE_PATH_REPLY, *PGET_PROCESS_IMAGE_FILE_PATH_REPLY;

//=============================================================================
// IOCTL_READ_VIRTUAL_MEMORY
//=============================================================================
typedef struct _READ_VIRTUAL_MEMORY_REQUEST {
    ULONG_PTR ProcessId;
    ULONG_PTR Address;
    ULONG Size;
} READ_VIRTUAL_MEMORY_REQUEST, *PREAD_VIRTUAL_MEMORY_REQUEST;
