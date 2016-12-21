/** @file
  Network test file services implementations.

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

#include "ENTS/EasDispatcher/Include/Eas.h"
#include <Protocol/EntsMonitorProtocol.h>

//
// Local functions declarations
//
EFI_STATUS
LoadSingleTestFile (
  IN EFI_DEVICE_PATH_PROTOCOL             *DevicePath,
  IN CHAR16                               *FileName,
  OUT EFI_NETWORK_TEST_FILE               **TestFile
  );

EFI_STATUS
EntsCreateSingleTestFile (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
  IN CHAR16                     *FileName,
  IN CHAR16                     *CmdName,
  IN EFI_HANDLE                 ImageHandle,
  IN UINTN                      Type,
  IN VOID                       *Context,
  OUT EFI_NETWORK_TEST_FILE     **TestFile
  );

EFI_STATUS
EntsUnloadSingleTestFile (
  IN EFI_NETWORK_TEST_FILE          *TestFile
  );

//
// External Functions implementations
//
EFI_STATUS
EntsLoadTestFiles (
  IN EFI_DEVICE_PATH_PROTOCOL     *DevicePath,
  IN CHAR16                       *FilePath,
  IN BOOLEAN                      Recursive
  )
/*++

Routine Description:

  Load all application files.

Arguments:

  DevicePath  - Device path of the test files.
  FilePath    - Path of the test files.
  Recursive   - Resursively.

Returns:

  EFI_SUCCESS           - Operation succeeded.
  EFI_INVALID_PARAMETER - Invalid parameter.
  Others                - Operation failed.

--*/
{
  EFI_STATUS                      Status;
  EFI_HANDLE                      DeviceHandle;
  EFI_FILE_HANDLE                 RootDir;
  EFI_FILE_HANDLE                 TestDir;
  CHAR16                          *SubDir;
  CHAR16                          *FileName;
  UINTN                           FileInfoSize;
  EFI_FILE_INFO                   *FileInfo;
  EFI_DEVICE_PATH_PROTOCOL        *RemainderPath;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Vol;
  EFI_NETWORK_TEST_FILE           *TestFile;

  //
  // Trace information
  //
  EFI_ENTS_STATUS ((L"Load test files ..."));
  //
  // Check parameters
  //
  if ((DevicePath == NULL) || (FilePath == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Locate the device handle from the device path
  //
  RemainderPath = DevicePath;

  Status = gBS->LocateDevicePath (
                &gEfiSimpleFileSystemProtocolGuid,
                &RemainderPath,
                &DeviceHandle
                );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"in LoadTestFile: Locate device handle - %r", Status));
    return Status;
  }
  //
  // Locate the volume of the file system
  //
  Status = gBS->HandleProtocol (
                DeviceHandle,
                &gEfiSimpleFileSystemProtocolGuid,
                &Vol
                );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"in LoadTestFile: Locate file volume - %r", Status));
    return Status;
  }
  //
  // Open the root volume
  //
  Status = Vol->OpenVolume (Vol, &RootDir);
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Open file volume - %r", Status));
    return Status;
  }
  //
  // Open the test dir
  //
  Status = RootDir->Open (
                      RootDir,
                      &TestDir,
                      FilePath,
                      EFI_FILE_MODE_READ,
                      EFI_FILE_DIRECTORY
                      );
  if (EFI_ERROR (Status)) {
    RootDir->Close (RootDir);
    return Status;
  }

  RootDir->Close (RootDir);

  //
  // Allocate memory for the entries in the directory
  //
  FileInfoSize = sizeof (EFI_FILE_INFO) + ENTS_MAX_BUFFER_SIZE * 4;

  Status = gBS->AllocatePool (
                EfiBootServicesData,
                FileInfoSize,
                &FileInfo
                );
  if (EFI_ERROR (Status)) {
    TestDir->Close (TestDir);
    return Status;
  }
  //
  // Walk through each file in the test directory
  //
  EFI_ENTS_STATUS ((L"reading a file entry.."));
  while (TRUE) {
    TestFile = NULL;

    //
    // Read a file entry
    //
    FileInfoSize = sizeof (EFI_FILE_INFO) + ENTS_MAX_BUFFER_SIZE * 4;

    Status = TestDir->Read (
                        TestDir,
                        &FileInfoSize,
                        FileInfo
                        );
    if (EFI_ERROR (Status) || (FileInfoSize == 0)) {
      break;
    }

    if ((FileInfo->Attribute & EFI_FILE_DIRECTORY) == 0) {
      //
      // This is a file
      //
      if (EntsStrEndWith (FileInfo->FileName, L".efi")) {
        //
        // Load the test file
        //
        FileName = PoolPrint (L"%s\\%s", FilePath, FileInfo->FileName);
        if (FileName == NULL) {
          EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Pool print - %r", EFI_OUT_OF_RESOURCES));
          break;
        }

        Status = LoadSingleTestFile (
                  DevicePath,
                  FileName,
                  &TestFile
                  );

        if (EFI_ERROR (Status)) {
          EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Load a test file(%s) - %r", FileName, Status));
          FreePool (FileName);
          continue;
        }

        FreePool (FileName);
        //
        // Add the test file to the test file list
        //
        InsertTailList (&gEasFT->TestAppList, &TestFile->Link);
      }
    } else {
      if (Recursive) {
        //
        // Skip the '.' and '..' dir
        //
        if ((StrCmp (FileInfo->FileName, L".") == 0) || (StrCmp (FileInfo->FileName, L"..") == 0)) {
          continue;
        }
        //
        // Load the test files under the sub directory
        //
        SubDir = PoolPrint (L"%s\\%s", FilePath, FileInfo->FileName);
        if (SubDir == NULL) {
          EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Pool print - %r", EFI_OUT_OF_RESOURCES));
          break;
        }

        Status = EntsLoadTestFiles (
                  DevicePath,
                  SubDir,
                  Recursive
                  );
        if (EFI_ERROR (Status)) {
          FreePool (SubDir);
          continue;
        }

        FreePool (SubDir);
      }
    }
  }
  //
  // Free resources
  //
  FreePool (FileInfo);
  TestDir->Close (TestDir);

  //
  // Done
  //
  return EFI_SUCCESS;
}

EFI_STATUS
EntsUnloadTestFiles (
  IN LIST_ENTRY               *TestAppList
  )
/*++

Routine Description:

  Unload all test files.

Arguments:

  TestAppList - Pointer to the test file list.

Returns:

  EFI_SUCCESS - Operation succeeded.
  Others      - Operation failed.

--*/
{
  EFI_NETWORK_TEST_FILE *TestFile;
  EFI_STATUS            Status;

  Status = EFI_SUCCESS;
  //
  // Trace information
  //
  EFI_ENTS_STATUS ((L"\nUnload test files ..."));
  while (!IsListEmpty (TestAppList)) {
    TestFile = CR (TestAppList->ForwardLink, EFI_NETWORK_TEST_FILE, Link, EFI_NETWORK_TEST_FILE_SIGNATURE);

    RemoveEntryList (&TestFile->Link);
    Status = EntsUnloadSingleTestFile (TestFile);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }
  //
  // Done
  //
  return EFI_SUCCESS;
}

EFI_STATUS
EntsFindTestFileByName (
  IN CHAR16                       *CmdName,
  IN OUT EFI_NETWORK_TEST_FILE    **TestFile
  )
/*++

Routine Description:

  Find a test file by a name.

Arguments:

  CmdName   - Command name.
  TestFile  - Pointer to the testFile structure.

Returns:

  EFI_SUCCESS - Operation succeeded.
  Others - Operation failed.

--*/
{
  LIST_ENTRY        *Link;
  EFI_NETWORK_TEST_FILE *TestFileTmp;
  EFI_STATUS            Status;

  TestFileTmp = NULL;
  Status      = EFI_NOT_FOUND;

  //
  // Walk through TestTestFile linked list
  // to get corresponding file to dispatch
  //
  for (Link = (gEasFT->TestAppList).ForwardLink; Link != &(gEasFT->TestAppList); Link = Link->ForwardLink) {
    TestFileTmp = CR (Link, EFI_NETWORK_TEST_FILE, Link, EFI_NETWORK_TEST_FILE_SIGNATURE);

    if (StriCmp (TestFileTmp->CmdName, CmdName) == 0) {
      *TestFile = TestFileTmp;
      Status    = EFI_SUCCESS;
      break;
    }
  }

  if (Status != EFI_SUCCESS) {
    EFI_ENTS_STATUS ((L"can't find cmd(%s)", CmdName));
  }

  return Status;
}
//
// Local Functions implementations
//
EFI_STATUS
LoadSingleTestFile (
  IN EFI_DEVICE_PATH_PROTOCOL        *DevicePath,
  IN CHAR16                          *FileName,
  OUT EFI_NETWORK_TEST_FILE          **TestFile
  )
/*++

Routine Description:

  Load a single test file.

Arguments:

  DevicePath  - Device path of the test file.
  FileName    - Name of the test file.
  TestFile    - Pointer to the test file structure's pointer.

Returns:

  EFI_SUCCESS           - Operation succeeded.
  EFI_INVALID_PARAMETER - Parameter invalid.
  EFI_OUT_OF_RESOURCES  - FileNode is NULL or FilePath is NULL.
  EFI_UNSUPPORTED       - File was not supported.
  Others                - Some failure happened.

--*/
{
  EFI_STATUS                Status;
  EFI_HANDLE                ImageHandle;
  EFI_DEVICE_PATH_PROTOCOL  *FileNode;
  EFI_DEVICE_PATH_PROTOCOL  *FilePath;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  UINTN                     ExitDataSize;
  CHAR16                    *ExitData;
  EFI_ENTS_PROTOCOL         *EntsProtocol;

  //
  // Check parameters
  //
  if ((DevicePath == NULL) || (FileName == NULL) || (TestFile == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Add the file path to the device path
  //
  FileNode = FileDevicePath (NULL, FileName);
  if (FileNode == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FilePath = AppendDevicePath (DevicePath, FileNode);
  if (FilePath == NULL) {
    FreePool (FileNode);
    return EFI_OUT_OF_RESOURCES;
  }

  FreePool (FileNode);

  //
  // Load the test file
  //
  Status = gBS->LoadImage (
                FALSE,
                gEasFT->ImageHandle,
                FilePath,
                NULL,
                0,
                &ImageHandle
                );
  if (EFI_ERROR (Status)) {
    FreePool (FilePath);
    return Status;
  }

  FreePool (FilePath);

  //
  // Verify the image is a driver or not
  //
  Status = gBS->HandleProtocol (
                ImageHandle,
                &gEfiLoadedImageProtocolGuid,
                &LoadedImage
                );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((LoadedImage->ImageCodeType == EfiBootServicesCode) || (LoadedImage->ImageCodeType == EfiRuntimeServicesCode)) {
    //
    // It is a driver
    //
    Status = gBS->StartImage (
                  ImageHandle,
                  &ExitDataSize,
                  &ExitData
                  );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiEntsProtocolGuid,
                  &EntsProtocol
                  );
    if (!EFI_ERROR (Status)) {
      Status = EntsCreateSingleTestFile (
                DevicePath,
                FileName,
                EntsProtocol->ClientName,
                ImageHandle,
                EFI_NETWORK_TEST_FILE_DRIVER,
                NULL,
                TestFile
                );
      if (EFI_ERROR (Status)) {
        return Status;
      }
    } else {
      return EFI_UNSUPPORTED;
    }
  } else {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EntsUnloadSingleTestFile (
  IN EFI_NETWORK_TEST_FILE          *TestFile
  )
/*++

Routine Description:

  Unload a single test file.

Arguments:

  TestFile    - Pointer to the test file structure.

Returns:

  EFI_SUCCESS - Operation succeeded.
  Others      - Some failure happened.

--*/
{
  //
  // Free the items of the test file
  //
  if (TestFile->DevicePath != NULL) {
    FreePool (TestFile->DevicePath);
    TestFile->DevicePath = NULL;
  }

  if (TestFile->FileName != NULL) {
    FreePool (TestFile->FileName);
    TestFile->FileName = NULL;
  }

  if (TestFile->CmdName != NULL) {
    FreePool (TestFile->CmdName);
    TestFile->CmdName = NULL;
  }

  if (TestFile->Type == EFI_NETWORK_TEST_FILE_DRIVER) {
    gBS->UnloadImage (TestFile->ImageHandle);
  }
  //
  // Free the test file itself
  //
  FreePool (TestFile);

  //
  // Done
  //
  return EFI_SUCCESS;
}

/**
 *
 *  @param DevicePath
 *  @param FileName
 *  @param Type
 *  @param Context        Context of the test file.
 *  @param TestFile       Pointer to the test file structure's pointer.
 *  @return EFI_SUCCESS   Successfully.
 *  @return Other value   Something failed.
 */
EFI_STATUS
EntsCreateSingleTestFile (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
  IN CHAR16                     *FileName,
  IN CHAR16                     *CmdName,
  IN EFI_HANDLE                 ImageHandle,
  IN UINTN                      Type,
  IN VOID                       *Context,
  OUT EFI_NETWORK_TEST_FILE     **TestFile
  )
/*++

Routine Description:

  Create a single test file.

Arguments:

  DevicePath  - Device path of the test file.
  FileName    - Name of the test file.
  CmdName     - Command name string.
  ImageHandle - The image handle.
  Type        - Type of the test file.
  Context     - Context of the test file.
  TestFile    - Pointer to the test file structure's pointer.

Returns:

  EFI_SUCCESS           - Operation succeeded.
  EFI_INVALID_PARAMETER - Parameter invalid.
  Others                - Some failure happened.

--*/
{
  EFI_STATUS  Status;

  //
  // Check parameter
  //
  if ((DevicePath == NULL) || (FileName == NULL) || (TestFile == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Allocate memory for the test file
  //
  Status = gBS->AllocatePool (
                EfiBootServicesData,
                sizeof (EFI_NETWORK_TEST_FILE),
                TestFile
                );
  if (EFI_ERROR (Status)) {
    EFI_ENTS_DEBUG ((EFI_ENTS_D_ERROR, L"Allocate pool - %r", Status));
    return Status;
  }

  ZeroMem (*TestFile, sizeof (EFI_NETWORK_TEST_FILE));

  //
  // Initialize the test file
  //
  (*TestFile)->Signature    = EFI_NETWORK_TEST_FILE_SIGNATURE;
  (*TestFile)->Version      = EFI_NETWORK_TEST_FILE_VERSION;

  (*TestFile)->DevicePath   = DuplicateDevicePath (DevicePath);
  (*TestFile)->FileName     = EntsStrDuplicate (FileName);
  (*TestFile)->CmdName      = EntsStrDuplicate (CmdName);
  (*TestFile)->ImageHandle  = ImageHandle;
  (*TestFile)->Type         = Type;
  (*TestFile)->Context      = Context;
  InitializeListHead (&(*TestFile)->Link);

  //
  // Done
  //
  return EFI_SUCCESS;
}
