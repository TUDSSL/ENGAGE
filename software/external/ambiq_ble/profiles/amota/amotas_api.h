//*****************************************************************************
//
//  amotas_api.h
//! @file
//!
//! @brief Brief description of the header. No need to get fancy here.
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
#ifndef AMOTAS_API_H
#define AMOTAS_API_H

#include "wsf_timer.h"
#include "att_api.h"

// enable debug print for AMOTA profile
// #define AMOTA_DEBUG_ON

#ifdef __cplusplus
extern "C"
{
#endif
//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
#define AMOTA_PACKET_SIZE           (512 + 16)    // Bytes
#define AMOTA_LENGTH_SIZE_IN_PKT    2
#define AMOTA_CMD_SIZE_IN_PKT       1
#define AMOTA_CRC_SIZE_IN_PKT       4
#define AMOTA_HEADER_SIZE_IN_PKT    AMOTA_LENGTH_SIZE_IN_PKT + AMOTA_CMD_SIZE_IN_PKT

#define AMOTA_FW_HEADER_SIZE        44

#define AMOTA_FW_STORAGE_INTERNAL   0
#define AMOTA_FW_STORAGE_EXTERNAL   1

/*! Configurable parameters */
typedef struct
{
    //! Short description of each member should go here.
    uint32_t reserved;
}
AmotasCfg_t;

//*****************************************************************************
//
// External variable definitions
//
//*****************************************************************************


//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
// function definitions
//
//*****************************************************************************

void amotas_init(wsfHandlerId_t handlerId, AmotasCfg_t *pCfg);

void amotas_proc_msg(wsfMsgHdr_t *pMsg);

uint8_t amotas_write_cback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                       uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr);

void amotas_start(dmConnId_t connId, uint8_t resetTimerEvt, uint8_t disconnectTimerEvt, uint8_t amotaCccIdx);

void amotas_stop(dmConnId_t connId);

extern void amotas_conn_close(dmConnId_t connId);

#ifdef __cplusplus
}
#endif

#endif // AMOTAS_API_H
