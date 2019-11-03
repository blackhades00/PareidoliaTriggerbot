/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "system_util.h"

#include "nt.h"


_Use_decl_annotations_
EXTERN_C
VOID
SysSetSignedKernelImageFlag(
    PDRIVER_OBJECT pDriverObject
)
/*++

Description:

    Set the 'signed kernel image' flag in the loader data table entry for the
    specified driver object.

Parameters:

    pDriverObject - The target driver object.

Remarks:

    Certain NT callbacks (e.g., object callbacks) require the calling driver to
    have the undocumented 'LDRP_SIGNED_KERNEL_IMAGE' flag set.

--*/
{
    PLDR_DATA_TABLE_ENTRY pEntry = NULL;

    pEntry = (PLDR_DATA_TABLE_ENTRY)pDriverObject->DriverSection;

    pEntry->Flags |= LDRP_SIGNED_KERNEL_IMAGE;
}
