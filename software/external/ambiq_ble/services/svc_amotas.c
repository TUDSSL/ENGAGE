//*****************************************************************************
//
//! @file svc_amotas.c
//!
//! @brief AM OTA service implementation
//!
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
#include "wsf_types.h"
#include "att_api.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "svc_amotas.h"
#include "svc_cfg.h"

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
//#define SOME_MACRO              42          //!< This is the answer

//*****************************************************************************
//
// Global Variables
//
//*****************************************************************************
//uint32_t g_ui32Stuff;

/**************************************************************************************************
 Static Variables
**************************************************************************************************/
/* UUIDs */
static const uint8_t svcRxUuid[] = {ATT_UUID_AMOTA_RX};
static const uint8_t svcTxUuid[] = {ATT_UUID_AMOTA_TX};

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* AMOTA service declaration */
static const uint8_t amotaSvc[] = {ATT_UUID_AMOTA_SERVICE};
static const uint16_t amotaLenSvc = sizeof(amotaSvc);

/* AMOTA RX characteristic */ 
static const uint8_t amotaRxCh[] = {ATT_PROP_WRITE_NO_RSP, UINT16_TO_BYTES(AMOTAS_RX_HDL), ATT_UUID_AMOTA_RX};
static const uint16_t amotaLenRxCh = sizeof(amotaRxCh);

/* AMOTA TX characteristic */ 
static const uint8_t amotaTxCh[] = {ATT_PROP_NOTIFY, UINT16_TO_BYTES(AMOTAS_TX_HDL), ATT_UUID_AMOTA_TX};
static const uint16_t amotaLenTxCh = sizeof(amotaTxCh);

/* AMOTA RX data */
/* Note these are dummy values */
static const uint8_t amotaRx[] = {0};
static const uint16_t amotaLenRx = sizeof(amotaRx);

/* AMOTA TX data */
/* Note these are dummy values */
static const uint8_t amotaTx[] = {0};
static const uint16_t amotaLenTx = sizeof(amotaTx);

/* Proprietary data client characteristic configuration */
static uint8_t amotaTxChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t amotaLenTxChCcc = sizeof(amotaTxChCcc);


/* Attribute list for AMOTA group */
static const attsAttr_t amotaList[] =
{
  {
    attPrimSvcUuid, 
    (uint8_t *) amotaSvc,
    (uint16_t *) &amotaLenSvc, 
    sizeof(amotaSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) amotaRxCh,
    (uint16_t *) &amotaLenRxCh,
    sizeof(amotaRxCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcRxUuid,
    (uint8_t *) amotaRx,
    (uint16_t *) &amotaLenRx,
    ATT_VALUE_MAX_LEN,
    (ATTS_SET_UUID_128 | ATTS_SET_VARIABLE_LEN | ATTS_SET_WRITE_CBACK),
    ATTS_PERMIT_WRITE
  },
  {
    attChUuid,
    (uint8_t *) amotaTxCh,
    (uint16_t *) &amotaLenTxCh,
    sizeof(amotaTxCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcTxUuid,
    (uint8_t *) amotaTx,
    (uint16_t *) &amotaLenTx,
    ATT_VALUE_MAX_LEN,
    (ATTS_SET_UUID_128 | ATTS_SET_VARIABLE_LEN),
    ATTS_PERMIT_READ
  },
  {
    attCliChCfgUuid,
    (uint8_t *) amotaTxChCcc,
    (uint16_t *) &amotaLenTxChCcc,
    sizeof(amotaTxChCcc),
    ATTS_SET_CCC,
    (ATTS_PERMIT_READ | ATTS_PERMIT_WRITE)
  }
};

/* AMOTA group structure */
static attsGroup_t svcAmotaGroup =
{
  NULL,
  (attsAttr_t *) amotaList,
  NULL,
  NULL,
  AMOTAS_START_HDL,
  AMOTAS_END_HDL
};

/*************************************************************************************************/
/*!
 *  \fn     SvcAmotasAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcAmotasAddGroup(void)
{
  AttsAddGroup(&svcAmotaGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcAmotaRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcAmotasRemoveGroup(void)
{
  AttsRemoveGroup(AMOTAS_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcAmotasCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcAmotasCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcAmotaGroup.readCback = readCback;
  svcAmotaGroup.writeCback = writeCback;
}
