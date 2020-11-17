//*****************************************************************************
//
//! @file am_app_KWD_AMA.h
//!
//! @brief header file of AMA protocol handler
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2017, Ambiq Micro
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
// This is part of revision v1.2.11 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_APP_KWD_AMA_H
#define AM_APP_KWD_AMA_H

#include "accessories.pb.h"

#define AMA_TRANSPORT_HEADER_VERSION_MASK       0xF000
#define AMA_TRANSPORT_HEADER_STEAM_ID_MASK      0x0F80
#define AMA_TRANSPORT_HEADER_LENGTH_TYPE_MASK   0x01

typedef struct
{
  #if USE_OUTPUT_AMVOS_AMA
    uint8_t ama_buf[AMA_BUFFER_SIZE + 3];
  #else
    uint8_t* buf;
  #endif
    uint32_t len;
    uint32_t reserved;
} sRadioQueue_t;

typedef enum
{
    VOS_AMA_INIT = 0,           // New state for disconnected time
    VOS_AMA_IDLE,
    VOS_AMA_LISTENING,
    VOS_AMA_PROCESSING,
    VOS_AMA_SPEAKING
} eVosAmaStatus_t;

//*****************************************************************************
// External function declaration
//*****************************************************************************

extern bool am_app_KWD_AMA_start_speech_send(void);
extern int am_app_KWD_AMA_rx_handler(uint8_t *data, uint16_t len);
extern bool am_app_KWD_AMA_keep_alive_send(void);

extern uint32_t g_ui32AmaDialogID;
extern eVosAmaStatus_t g_eAmaStatus;

#endif  // #ifndef AM_APP_KWD_AMA_H
