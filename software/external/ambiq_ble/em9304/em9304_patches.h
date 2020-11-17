//*****************************************************************************
//
//! @file em9304_patches.h
//!
//! @brief This is a generated file.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2020, Ambiq Micro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision 2.4.2 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>

#ifndef EM9304_PATCHES_H
#define EM9304_PATCHES_H

//*****************************************************************************
//
// Length of the binary array in bytes.
//
//*****************************************************************************
#define EM9304_PATCHES_NUM_PATCHES          8

// EM patch destination memory:
// 1 means EM patch will be programmed into OTP if emp file
// was not programmed before.
// 0 means EM patch will be programmed into IRAM each time when
// EM9304 is cold boot.

#define DEST_MEMORY_IRAM                    0
#define DEST_MEMORY_OTP                     1
#define EM9304_PATCHES_DEST_MEMORY          0

//*****************************************************************************
//
// EM9304 Container Info type
//
//*****************************************************************************
typedef struct
{
  uint16_t  buildNumber;        // Firmware Build Number
  uint16_t  userBuildNumber;    // User defined Build Number (determines patch precedence)
  uint8_t   containerVersion;   // Container Version
  uint8_t   containerType;      // Container Type
  uint8_t   containerID;        // Container ID
  bool      applyPatch;         // Flag to apply this patch.
  uint8_t   startingPatch;      // Starting patch index.
  uint8_t   endingPatch;        // Ending patch index + 1.
} em9304_container_info_t;

//*****************************************************************************
//
// Extracted binary array.
//
//*****************************************************************************
extern em9304_container_info_t g_pEm9304Patches[8];
extern const uint8_t g_pEm9304PatchesHCICmd[131][68];

#endif // EM9304_PATCHES_H
