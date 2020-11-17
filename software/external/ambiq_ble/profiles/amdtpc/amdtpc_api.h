// ****************************************************************************
//
//  amdtpc_api.h
//! @file
//!
//! @brief Ambiq Micro AMDTP client.
//!
//! @{
//
// ****************************************************************************

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

#ifndef AMDTPC_API_H
#define AMDTPC_API_H

#include "att_api.h"
#include "amdtp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

enum
{
    AMDTP_RX_HDL_IDX,             /*! Rx data */
    AMDTP_TX_DATA_HDL_IDX,        /*! Tx data */
    AMDTP_TX_DATA_CCC_HDL_IDX,    /*! Tx data client characteristic configuration descriptor */
    AMDTP_ACK_HDL_IDX,            /*! Ack */
    AMDTP_ACK_CCC_HDL_IDX,        /*! Ack client characteristic configuration descriptor */
    AMDTP_HDL_LIST_LEN            /*! Handle list length */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     AmdtpcDiscover
 *
 *  \brief  Perform service and characteristic discovery for AMDTP service.
 *          Parameter pHdlList must point to an array of length AMDTP_HDL_LIST_LEN.
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpcDiscover(dmConnId_t connId, uint16_t *pHdlList);

void
amdtpc_init(wsfHandlerId_t handlerId, amdtpRecvCback_t recvCback, amdtpTransCback_t transCback);

void
amdtpc_start(uint16_t rxHdl, uint16_t ackHdl, uint8_t timerEvt);

void
amdtpc_proc_msg(wsfMsgHdr_t *pMsg);

eAmdtpStatus_t
AmdtpcSendPacket(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len);

#ifdef __cplusplus
};
#endif

#endif /* AMDTPC_API_H */
