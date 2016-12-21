/** @file
  Remote Interface Validation Language Type implementation

  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>

#include "RivlType.h"
#include "RivlVariable.h"

RIVL_TYPE           *gRivlTypeList = NULL;
RIVL_INTERNAL_TYPE  gRivlInternalTypeArray[] = {
  {
    L"BOOLEAN",
    sizeof (BOOLEAN)
  },
  {
    L"INTN",
    sizeof (INTN)
  },
  {
    L"UINTN",
    sizeof (UINTN)
  },
  {
    L"INT8",
    sizeof (INT8)
  },
  {
    L"UINT8",
    sizeof (UINT8)
  },
  {
    L"INT16",
    sizeof (INT16)
  },
  {
    L"UINT16",
    sizeof (UINT16)
  },
  {
    L"INT32",
    sizeof (INT32)
  },
  {
    L"UINT32",
    sizeof (UINT32)
  },
  {
    L"INT64",
    sizeof (INT64)
  },
  {
    L"UINT64",
    sizeof (UINT64)
  },
  {
    L"CHAR8",
    sizeof (CHAR8)
  },
  {
    L"CHAR16",
    sizeof (CHAR16)
  },
  {
    L"POINTER",
    sizeof (VOID *)
  },
  0
};

//
// Internal Function
//
EFI_STATUS
AddRivlMember (
  IN RIVL_MEMBER               *RivlMember,
  IN CHAR16                    *Name,
  IN CHAR16                    *Type,
  IN UINT32                    TotalSize,
  IN UINT32                    Offset
  );

EFI_STATUS
RivlAddRivlType (
  IN CHAR16                    *TypeBuf
  )
/*++

Routine Description:

  This func is attend to add RivlType to gRivlTypeList
  The format of the type buffer is :
  <TypeName> <TypeSize> <TypeMemberNumber>
    (<MemberName> <MemberType> <MemberSize> <MemberOffset>) ...

Arguments:

  TypeBuf -  The buffer string of Type

Returns:

  EFI_INVALID_PARAMETER - Invalide parameter.
  EFI_OUT_OF_RESOURCES  - Memory allocation failed.
  EFI_SUCCESS           - Operation succeeded.

--*/
{
  EFI_STATUS  Status;
  CHAR16      *Type;
  CHAR16      *TotalSizeStr;
  UINTN       TotalSize;
  CHAR16      *MemberNumberStr;
  UINTN       MemberNumber;
  RIVL_MEMBER *RivlMember;
  CHAR16      *MemberName;
  CHAR16      *MemberType;
  CHAR16      *MemberTotalSizeStr;
  UINTN       MemberTotalSize;
  CHAR16      *MemberOffsetStr;
  UINTN       MemberOffset;
  UINTN       MemberIndex;
  INTN        Index;

  //
  // Conformance check
  //
  if (TypeBuf == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Seperate the TypeBuf
  //
  Type            = strtok_line (TypeBuf, RIVL_SEP_SIG);
  TotalSizeStr    = strtok_line (NULL, RIVL_SEP_SIG);
  MemberNumberStr = strtok_line (NULL, RIVL_SEP_SIG);

  //
  // Conformance check
  //
  if ((Type == NULL) || (TotalSizeStr == NULL) || (MemberNumberStr == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Convert string to number
  //
  Index = EntsStrToUINTN (TotalSizeStr, &TotalSize);
  if (Index == -1) {
    return EFI_INVALID_PARAMETER;
  }

  Index = EntsStrToUINTN (MemberNumberStr, &MemberNumber);
  if (Index == -1) {
    return EFI_INVALID_PARAMETER;
  }

  RivlMember = AllocatePool (sizeof (RIVL_MEMBER) * (MemberNumber + 1));
  if (RivlMember == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (RivlMember, sizeof (RIVL_MEMBER) * (MemberNumber + 1));

  for (MemberIndex = 0; MemberIndex < MemberNumber; MemberIndex++) {
    MemberName          = strtok_line (NULL, RIVL_SEP_SIG);
    MemberType          = strtok_line (NULL, RIVL_SEP_SIG);
    MemberTotalSizeStr  = strtok_line (NULL, RIVL_SEP_SIG);
    MemberOffsetStr     = strtok_line (NULL, RIVL_SEP_SIG);

    //
    // Conformance check
    //
    if ((MemberName == NULL) || (MemberType == NULL) || (MemberTotalSizeStr == NULL) || (MemberOffsetStr == NULL)) {
      FreePool (RivlMember);
      return EFI_INVALID_PARAMETER;
    }
    //
    // Convert string to number
    //
    Index = EntsStrToUINTN (MemberTotalSizeStr, &MemberTotalSize);
    if (Index == -1) {
      FreePool (RivlMember);
      return EFI_INVALID_PARAMETER;
    }

    Index = EntsStrToUINTN (MemberOffsetStr, &MemberOffset);
    if (Index == -1) {
      FreePool (RivlMember);
      return EFI_INVALID_PARAMETER;
    }
    //
    // Add Rivl Member
    //
    AddRivlMember (
      RivlMember + MemberIndex,
      MemberName,
      MemberType,
      (UINT32) MemberTotalSize,
      (UINT32) MemberOffset
      );
  }
  //
  // Add last node
  //
  AddRivlMember (
    RivlMember + MemberIndex,
    L"\0",
    L"\0",
    0,
    0
    );

  //
  // Add Rivl Type
  //
  Status = AddRivlType (
            Type,
            (UINT32) TotalSize,
            (UINT32) MemberNumber,
            RivlMember
            );

  return Status;
}

EFI_STATUS
RivlDelRivlType (
  IN CHAR16                    *TypeBuf
  )
/*++

Routine Description:

  This func is attend to delete RivlType from gRivlTypeList.

Arguments:

  TypeBuf     - The buffer string of Type, NULL means delete all.

Returns:

  EFI_SUCCESS - Operation succeeded.

--*/
{
  EFI_STATUS  Status;
  CHAR16      *Name;

  //
  // If buf is NULL, delete all type
  //
  if (TypeBuf == NULL) {
    Status = DelRivlAllType ();
    return EFI_SUCCESS;
  }
  //
  // Del Rivl Type
  //
  Name = strtok_line (TypeBuf, RIVL_SEP_SIG);
  if (Name != NULL) {
    DelRivlType (Name);
  }

  while (Name != NULL) {
    Name = strtok_line (NULL, RIVL_SEP_SIG);
    if (Name != NULL) {
      DelRivlType (Name);
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
AddRivlType (
  IN CHAR16                    *Type,
  IN UINT32                    Size,
  IN UINT32                    MemberNumber,
  IN RIVL_MEMBER               *RivlMember
  )
/*++

Routine Description:

  This func is to add RivlType to gRivlTypeList.

Arguments:

  Type          - Type of RivlType.
  Size          - Size of RivlType.
  MemberNumber  - MemberNumber of RivlType.
  RivlMember    - Pointer to the RivlMember of RivlType.

Returns:

  EFI_ALREADY_STARTED   - Rivil type already existed.
  EFI_OUT_OF_RESOURCES  - Memory allocation failed.
  EFI_SUCCESS           - Operation succeeded.

--*/
{
  RIVL_TYPE *NewRivlType;
  RIVL_TYPE *OldRivlType;

  NewRivlType = AllocatePool (sizeof (RIVL_TYPE));
  if (NewRivlType == NULL) {
    FreePool (RivlMember);
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (NewRivlType, sizeof (RIVL_TYPE));

  //
  // Fill Type
  //
  StrCpy (NewRivlType->Type, Type);

  //
  // Fill MemberNumber
  //
  NewRivlType->MemberNumber = MemberNumber;

  //
  // Fill Size
  //
  NewRivlType->Size = Size;

  //
  // Fill Member
  //
  NewRivlType->Member = RivlMember;

  //
  // Duplicate check
  //
  OldRivlType = SearchRivlType (Type);
  if (OldRivlType != NULL) {
    RIVL_DEBUG ((L"Duplicated Type - %s\n", Type));
    FreePool (RivlMember);
    FreePool (NewRivlType);
    return EFI_ALREADY_STARTED;
  }
  //
  // Add to Variable List
  //
  OldRivlType       = gRivlTypeList;
  gRivlTypeList     = NewRivlType;
  NewRivlType->Next = OldRivlType;
  if (OldRivlType != NULL) {
    OldRivlType->Prev = NewRivlType;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
DelRivlType (
  IN CHAR16                    *Type
  )
/*++

Routine Description:

  This func is to delete RivlType from gRivlTypeList.

Arguments:

  Type  - Type of RivlType.

Returns:

  EFI_INVALID_PARAMETER - Parameter invalid.
  EFI_NOT_FOUND         - The specified type cannot be found.
  EFI_SUCCESS           - Operation succeeded.

--*/
{
  RIVL_TYPE *OldRivlType;

  if (Type == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Search the node
  //
  OldRivlType = SearchRivlType (Type);
  if (OldRivlType == NULL) {
    return EFI_NOT_FOUND;
  }

  if (OldRivlType->Prev != NULL) {
    OldRivlType->Prev->Next = OldRivlType->Next;
  }

  if (OldRivlType->Next != NULL) {
    OldRivlType->Next->Prev = OldRivlType->Prev;
  }

  if (gRivlTypeList == OldRivlType) {
    gRivlTypeList = OldRivlType->Next;
  }
  //
  // Free the node
  //
  if (OldRivlType->Member != NULL) {
    FreePool (OldRivlType->Member);
  }

  FreePool (OldRivlType);

  return EFI_SUCCESS;
}

EFI_STATUS
DelRivlAllType (
  VOID
  )
/*++

Routine Description:

  This func is to delete all RivlType from gRivlTypeList

Arguments:

  None

Returns:

  EFI_SUCCESS - Operation succeeded.

--*/
{
  RIVL_TYPE *OldRivlType;

  OldRivlType = gRivlTypeList;
  while (OldRivlType != NULL) {
    gRivlTypeList = gRivlTypeList->Next;

    //
    // Free node
    //
    if (OldRivlType->Member != NULL) {
      FreePool (OldRivlType->Member);
    }

    FreePool (OldRivlType);

    OldRivlType = gRivlTypeList;
  }

  return EFI_SUCCESS;
}

RIVL_TYPE *
SearchRivlType (
  IN CHAR16                    *Type
  )
/*++

Routine Description:

  This func is to search RivlType from gRivlTypeList.

Arguments:

  Type  - Type of RivlType.

Returns:

  The pointer of RivlType found successfully, NULL means not found

--*/
{
  RIVL_TYPE *RivlType;

  if (Type == NULL) {
    return NULL;
  }

  RivlType = gRivlTypeList;
  while (RivlType != NULL) {
    if (StrCmp (RivlType->Type, Type) == 0) {
      //
      // Find it
      //
      return RivlType;
    }

    RivlType = RivlType->Next;
  }

  return NULL;
}

RIVL_INTERNAL_TYPE *
SearchRivlInternalType (
  IN CHAR16                    *Type
  )
/*++

Routine Description:

  This func is to search RivlInternalType from gRivlInternalTypeArray.

Arguments:

  Type  - Type of RivlInternalType.

Returns:

  The pointer of RivlInternalType found successfully, NULL means not found

--*/
{
  RIVL_INTERNAL_TYPE  *RivlInternalType;

  RivlInternalType = gRivlInternalTypeArray;
  while (RivlInternalType->Type[0] != L'\0') {
    if (StrCmp (RivlInternalType->Type, Type) == 0) {
      //
      // Find it
      //
      return RivlInternalType;
    }

    RivlInternalType++;
  }

  return NULL;
}

RIVL_MEMBER *
SearchRivlMember (
  IN RIVL_TYPE                 *RivlType,
  IN CHAR16                    *Name
  )
/*++

Routine Description:

  This func is to search RivlMember from RivlType.

Arguments:

  RivlType  - Which RivlType to be found in.
  Name      - Name of RivlMember.

Returns:

  The pointer of RivlMember found successfully, NULL means not found

--*/
{
  RIVL_MEMBER *RivlMember;

  RivlMember = RivlType->Member;
  while (RivlMember->Name[0] != L'\0') {
    if (StrCmp (RivlMember->Name, Name) == 0) {
      //
      // Find it
      //
      return RivlMember;
    }

    RivlMember++;
  }

  return NULL;
}
//
// Internal function implementation
//
EFI_STATUS
AddRivlMember (
  IN RIVL_MEMBER               *RivlMember,
  IN CHAR16                    *Name,
  IN CHAR16                    *Type,
  IN UINT32                    TotalSize,
  IN UINT32                    Offset
  )
/*++

Routine Description:

  This func is to add RivlMember to the member of RivlType.

Arguments:

  RivlMember  - Which RivlMember to be added
  Name        - Name of RivlMember.
  Type        - Type of RivlMember.
  TotalSize   - TotalSize (TypeSize * ArrayNumber) of RivlMember.
  Offset      - Offset of RivlMember.

Returns:

  EFI_SUCCESS           - Operation succeeded.
  EFI_INVALID_PARAMETER - Parameter invalid.

--*/
{
  RIVL_INTERNAL_TYPE  *RivlInternalType;
  RIVL_TYPE           *RivlType;

  //
  // Fill Name
  //
  StrCpy (RivlMember->Name, Name);

  //
  // Fill Type
  //
  StrCpy (RivlMember->Type, Type);

  //
  // Fill Offset
  //
  RivlMember->Offset = Offset;

  //
  // Fill TypeSize and ArrayNumber
  //
  if (StrCmp (Type, L"\0") == 0) {
    return EFI_SUCCESS;
  }

  RivlInternalType = SearchRivlInternalType (Type);
  if (RivlInternalType != NULL) {
    RivlMember->TypeSize    = RivlInternalType->Size;
    RivlMember->ArrayNumber = TotalSize / RivlMember->TypeSize;
    if (RivlMember->ArrayNumber * RivlMember->TypeSize != TotalSize) {
      return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
  }

  RivlType = SearchRivlType (Type);
  if (RivlType != NULL) {
    RivlMember->TypeSize    = RivlType->Size;
    RivlMember->ArrayNumber = TotalSize / RivlMember->TypeSize;
    if (RivlMember->ArrayNumber * RivlMember->TypeSize != TotalSize) {
      return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}
