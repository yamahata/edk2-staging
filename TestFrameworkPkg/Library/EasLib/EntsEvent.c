/** @file
  Event operation functions

  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>
#include <Library/EntsLib.h>
#include <Library/DebugLib.h>

EFI_STATUS
EntsWaitForSingleEvent (
  IN EFI_EVENT                Event,
  IN UINT64                   Timeout OPTIONAL
  )
/*++

Routine Description:
  Function waits for a given event to fire, or for an optional timeout to expire.

Arguments:

  Event            - The event to wait for

  Timeout          - An optional timeout value in 100 ns units.

Returns:

  EFI_SUCCESS       - Event fired before Timeout expired.
  EFI_TIME_OUT     - Timout expired before Event fired..

--*/
{
  EFI_STATUS  Status;
  UINTN       Index;
  EFI_EVENT   TimerEvent;
  EFI_EVENT   WaitList[2];

  if (Timeout) {
    //
    // Create a timer event
    //
    Status = gntBS->CreateEvent (EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (!EFI_ERROR (Status)) {
      //
      // Set the timer event
      //
      gntBS->SetTimer (
              TimerEvent,
              TimerRelative,
              Timeout
              );

      //
      // Wait for the original event or the timer
      //
      WaitList[0] = Event;
      WaitList[1] = TimerEvent;
      Status      = gntBS->WaitForEvent (2, WaitList, &Index);
      gntBS->CloseEvent (TimerEvent);

      //
      // If the timer expired, change the return to timed out
      //
      if (!EFI_ERROR (Status) && Index == 1) {
        Status = EFI_TIMEOUT;
      }
    }
  } else {
    //
    // No timeout... just wait on the event
    //
    Status = gntBS->WaitForEvent (1, &Event, &Index);
    ASSERT (!EFI_ERROR (Status));
    ASSERT (Index == 0);
  }

  return Status;
}
