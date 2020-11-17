//*****************************************************************************
//
//! @file svc_amotas.h
//!
//! @brief AmbiqMicro OTA service definition
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
#ifndef SVC_AMOTAS_H
#define SVC_AMOTAS_H

//
// Put additional includes here if necessary.
//

#ifdef __cplusplus
extern "C"
{
#endif
//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

/*! Base UUID:  00002760-08C2-11E1-9073-0E8AC72EXXXX */
#define ATT_UUID_AMBIQ_BASE             0x2E, 0xC7, 0x8A, 0x0E, 0x73, 0x90, \
                                            0xE1, 0x11, 0xC2, 0x08, 0x60, 0x27, 0x00, 0x00

/*! Macro for building Ambiq UUIDs */
#define ATT_UUID_AMBIQ_BUILD(part)      UINT16_TO_BYTES(part), ATT_UUID_AMBIQ_BASE

/*! Partial amota service UUIDs */
#define ATT_UUID_AMOTA_SERVICE_PART     0x1001

/*! Partial amota rx characteristic UUIDs */
#define ATT_UUID_AMOTA_RX_PART          0x0001

/*! Partial amota tx characteristic UUIDs */
#define ATT_UUID_AMOTA_TX_PART          0x0002

/* Amota services */
#define ATT_UUID_AMOTA_SERVICE          ATT_UUID_AMBIQ_BUILD(ATT_UUID_AMOTA_SERVICE_PART)

/* Amota characteristics */
#define ATT_UUID_AMOTA_RX               ATT_UUID_AMBIQ_BUILD(ATT_UUID_AMOTA_RX_PART)
#define ATT_UUID_AMOTA_TX               ATT_UUID_AMBIQ_BUILD(ATT_UUID_AMOTA_TX_PART)

// AM OTA Service
#define AMOTAS_START_HDL               0x300
#define AMOTAS_END_HDL                 (AMOTAS_MAX_HDL - 1)


/* AMOTA Service Handles */
enum
{
  AMOTA_SVC_HDL = AMOTAS_START_HDL,     /* AMOTA service declaration */
  AMOTAS_RX_CH_HDL,                     /* AMOTA write command characteristic */ 
  AMOTAS_RX_HDL,                        /* AMOTA write command data */
  AMOTAS_TX_CH_HDL,                     /* AMOTA notify characteristic */ 
  AMOTAS_TX_HDL,                        /* AMOTA notify data */
  AMOTAS_TX_CH_CCC_HDL,                 /* AMOTA notify client characteristic configuration */
  AMOTAS_MAX_HDL
};


//*****************************************************************************
//
// External variable definitions
//
//*****************************************************************************
//extern uint32_t g_ui32Stuff;

//*****************************************************************************
//
// Function definitions.
//
//*****************************************************************************
void SvcAmotasAddGroup(void);
void SvcAmotasRemoveGroup(void);
void SvcAmotasCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
}
#endif

#endif // SVC_AMOTAS_H
