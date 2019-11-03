/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "pattern.h"

#include "log.h"
#include "ntdll.h"


_Use_decl_annotations_
BOOL
PtnFindPatternUnique(
    PVOID pBaseAddress,
    SIZE_T cbSearchSize,
    PCHAR pszPattern,
    PCHAR pszMask,
    SIZE_T cchPattern,
    CHAR Wildcard,
    PULONG_PTR pAddress
)
/*++

Description:

    Search the specified region of memory for a unique series of bytes which
    match the specified pattern and mask.

Parameters:

    pBaseAddress - The base address of the region of memory to be searched.

    cbSearchSize - The size of the region of memory to be searched.

    pszPattern - A null-terminated string representing the desired series of
        bytes to be matched.

    pszMask - A null-terminated string that specifies the indices of the
        wildcard characters in the pattern string.

    cchPattern - The number of characters, including the null-terminator, in
        the pattern string and the mask string.

    WildCard - The wildcard character used in the mask string.

    pAddress - Returns the address of the matched series of bytes.

Remarks:

    This routine returns FALSE if multiple matches are found.

--*/
{
    SIZE_T cchCompare = 0;
    PCHAR pCursor = NULL;
    PCHAR pEndAddress = NULL;
    SIZE_T i = 0;
    PVOID pCandidate = NULL;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pAddress = 0;

    DBG_PRINT("Searching for unique pattern. (Address = %p, Size = 0x%IX)",
        pBaseAddress,
        cbSearchSize);

    //
    // We do not match the null-terminator when searching.
    //
    cchCompare = cchPattern - 1;
    if (!cchCompare || MAXSIZE_T == cchCompare)
    {
        ERR_PRINT("Invalid pattern length.");
        status = FALSE;
        goto exit;
    }

    pEndAddress = (PCHAR)((ULONG_PTR)pBaseAddress + cbSearchSize);

    for (pCursor = (PCHAR)pBaseAddress;
        (ULONG_PTR)pCursor + cchCompare <= (ULONG_PTR)pEndAddress;
        pCursor++)
    {
        for (i = 0; i < cchCompare; i++)
        {
            if (pszMask[i] == Wildcard)
            {
                continue;
            }

            if (pCursor[i] != pszPattern[i])
            {
                break;
            }
        }
        //
        if (i != cchCompare)
        {
            continue;
        }

        //
        // Verify that we only find one match in the search region.
        //
        if (pCandidate)
        {
            ERR_PRINT("Pattern collision. (%p, %p)", pCandidate, pCursor);
            status = FALSE;
            goto exit;
        }

        //
        // Set the current address as our candidate.
        //
        pCandidate = pCursor;
    }
    //
    if (!pCandidate)
    {
        ERR_PRINT("Failed to find pattern.");
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pAddress = (ULONG_PTR)pCandidate;

exit:
    return status;
}
