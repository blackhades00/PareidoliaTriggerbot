/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "pe.h"

#include "debug.h"
#include "memory_util.h"
#include "ntdll.h"


_Use_decl_annotations_
ULONG
PeGetImageSize(
    PVOID pImageBase
)
{
    PIMAGE_NT_HEADERS pNtHeaders = NULL;
    ULONG cbImageSize = 0;

    pNtHeaders = RtlImageNtHeader(pImageBase);
    if (!pNtHeaders)
    {
        SetLastError(ERROR_BAD_FORMAT);
        goto exit;
    }

    cbImageSize = pNtHeaders->OptionalHeader.SizeOfImage;

exit:
    return cbImageSize;
}


_Use_decl_annotations_
BOOL
PeGetSectionsByCharacteristics(
    PVOID pImageBase,
    ULONG Characteristics,
    PIMAGE_SECTION_HEADER** pppSectionHeaders,
    PULONG pnSectionHeaders
)
/*++

Routine Description:

    Returns the image section header pointer of each section in the image with
    the specified characteristics.

Parameters:

    pImageBase - The base address of the target image.

    Characteristics - A bitmask of image section characteristics to match
        against.

    pppSectionHeaders - Returns a pointer to an allocated array of image
        section header pointers for sections with the specified
        characteristics.

    pnSectionHeaders - Returns the number of elements in the allocated array.

Remarks:

    If successful, the caller must free the returned array by calling
    MemFreeHeap.

--*/
{
    PIMAGE_NT_HEADERS pNtHeaders = NULL;
    PIMAGE_SECTION_HEADER pSectionHeader = NULL;
    USHORT i = 0;
    USHORT nSectionHeaders = 0;
    SIZE_T cbSectionHeaders = 0;
    USHORT j = 0;
    PIMAGE_SECTION_HEADER* ppSectionHeaders = NULL;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pppSectionHeaders = NULL;
    *pnSectionHeaders = 0;

    pNtHeaders = RtlImageNtHeader(pImageBase);
    if (!pNtHeaders)
    {
        SetLastError(ERROR_BAD_FORMAT);
        status = FALSE;
        goto exit;
    }

    //
    // Determine the number of sections which have the specified
    //  characteristics.
    //
    pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);

    for (i = 0;
        i < pNtHeaders->FileHeader.NumberOfSections;
        ++i, ++pSectionHeader)
    {
        if (pSectionHeader->Characteristics & Characteristics)
        {
            nSectionHeaders++;
        }
    }
    //
    if (!nSectionHeaders)
    {
        SetLastError(ERROR_NOT_FOUND);
        status = FALSE;
        goto exit;
    }

    //
    // Allocate and initialize the returned array.
    //
    cbSectionHeaders = nSectionHeaders * sizeof(*ppSectionHeaders);

    ppSectionHeaders =
        (PIMAGE_SECTION_HEADER*)MemAllocateHeap(cbSectionHeaders);
    if (!ppSectionHeaders)
    {
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
        status = FALSE;
        goto exit;
    }

    pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);

    for (i = 0, j = 0;
        i < pNtHeaders->FileHeader.NumberOfSections;
        ++i, ++pSectionHeader)
    {
        if (!(pSectionHeader->Characteristics & Characteristics))
        {
            continue;
        }

        if (j < nSectionHeaders)
        {
            ppSectionHeaders[j] = pSectionHeader;
        }

        j++;
    }
    //
    if (j != nSectionHeaders)
    {
        SetLastError(ERROR_INVALID_DATA);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pppSectionHeaders = ppSectionHeaders;
    *pnSectionHeaders = nSectionHeaders;

exit:
    if (!status)
    {
        if (ppSectionHeaders)
        {
            VERIFY(MemFreeHeap(ppSectionHeaders));
        }
    }

    return status;
}


_Use_decl_annotations_
BOOL
PeGetExecutableSections(
    PVOID pImageBase,
    PIMAGE_SECTION_HEADER** pppSectionHeaders,
    PULONG pnSectionHeaders
)
{
    return PeGetSectionsByCharacteristics(
        pImageBase,
        IMAGE_SCN_MEM_EXECUTE,
        pppSectionHeaders,
        pnSectionHeaders);
}
