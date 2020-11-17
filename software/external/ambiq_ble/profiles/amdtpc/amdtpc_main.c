// ****************************************************************************
//
//  amdtpc_main.c
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

#include <string.h>
#include <stdbool.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "bstream.h"
#include "app_api.h"
#include "amdtpc_api.h"
#include "svc_amdtp.h"
#include "wsf_trace.h"

static void amdtpcHandleWriteResponse(attEvt_t *pMsg);

//*****************************************************************************
//
// Global variables
//
//*****************************************************************************

uint8_t rxPktBuf[AMDTP_PACKET_SIZE];
uint8_t txPktBuf[AMDTP_PACKET_SIZE];
uint8_t ackPktBuf[20];


/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* UUIDs */
static const uint8_t amdtpSvcUuid[] = {ATT_UUID_AMDTP_SERVICE};    /*! AMDTP service */
static const uint8_t amdtpRxChUuid[] = {ATT_UUID_AMDTP_RX};        /*! AMDTP Rx */
static const uint8_t amdtpTxChUuid[] = {ATT_UUID_AMDTP_TX};        /*! AMDTP Tx */
static const uint8_t amdtpAckChUuid[] = {ATT_UUID_AMDTP_ACK};      /*! AMDTP Ack */

/* Characteristics for discovery */

/*! Proprietary data */
static const attcDiscChar_t amdtpRx =
{
  amdtpRxChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

static const attcDiscChar_t amdtpTx =
{
  amdtpTxChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

/*! AMDTP Tx CCC descriptor */
static const attcDiscChar_t amdtpTxCcc =
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

static const attcDiscChar_t amdtpAck =
{
  amdtpAckChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

/*! AMDTP Tx CCC descriptor */
static const attcDiscChar_t amdtpAckCcc =
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *amdtpDiscCharList[] =
{
  &amdtpRx,                  /*! Rx */
  &amdtpTx,                  /*! Tx */
  &amdtpTxCcc,               /*! Tx CCC descriptor */
  &amdtpAck,                 /*! Ack */
  &amdtpAckCcc               /*! Ack CCC descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
//WSF_ASSERT(AMDTP_HDL_LIST_LEN == ((sizeof(amdtpDiscCharList) / sizeof(attcDiscChar_t *))));


/* Control block */
static struct
{
    bool_t                  txReady;                // TRUE if ready to send notifications
    uint16_t                attRxHdl;
    uint16_t                attAckHdl;
    amdtpCb_t               core;
}
amdtpcCb;

/*************************************************************************************************/
/*!
 *  \fn     AmdtpDiscover
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
void
AmdtpcDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_128_UUID_LEN, (uint8_t *) amdtpSvcUuid,
                     AMDTP_HDL_LIST_LEN, (attcDiscChar_t **) amdtpDiscCharList, pHdlList);
}

//*****************************************************************************
//
// Send data to Server
//
//*****************************************************************************
static void
amdtpcSendData(uint8_t *buf, uint16_t len)
{
    dmConnId_t connId;

    if ((connId = AppConnIsOpen()) == DM_CONN_ID_NONE)
    {
        APP_TRACE_INFO0("AmdtpcSendData() no connection\n");
        return;
    }
    if (amdtpcCb.attRxHdl != ATT_HANDLE_NONE)
    {
        AttcWriteCmd(connId, amdtpcCb.attRxHdl, len, buf);
        amdtpcCb.txReady = false;
    }
    else
    {
        APP_TRACE_WARN1("Invalid attRxHdl = 0x%x\n", amdtpcCb.attRxHdl);
    }
}

static eAmdtpStatus_t
amdtpcSendAck(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len)
{
    dmConnId_t connId;

    AmdtpBuildPkt(&amdtpcCb.core, type, encrypted, enableACK, buf, len);

    if ((connId = AppConnIsOpen()) == DM_CONN_ID_NONE)
    {
        APP_TRACE_INFO0("AmdtpcSendAck() no connection\n");
        return AMDTP_STATUS_TX_NOT_READY;
    }

    if (amdtpcCb.attAckHdl != ATT_HANDLE_NONE)
    {
        //APP_TRACE_INFO2("rxHdl = 0x%x, ackHdl = 0x%x\n", amdtpcCb.attRxHdl, amdtpcCb.attAckHdl);
        AttcWriteCmd(connId, amdtpcCb.attAckHdl, amdtpcCb.core.ackPkt.len, amdtpcCb.core.ackPkt.data);
    }
    else
    {
        APP_TRACE_INFO1("Invalid attAckHdl = 0x%x\n", amdtpcCb.attAckHdl);
        return AMDTP_STATUS_TX_NOT_READY;
    }
    return AMDTP_STATUS_SUCCESS;
}

void
amdtpc_init(wsfHandlerId_t handlerId, amdtpRecvCback_t recvCback, amdtpTransCback_t transCback)
{
    memset(&amdtpcCb, 0, sizeof(amdtpcCb));
    amdtpcCb.txReady = false;
    amdtpcCb.core.txState = AMDTP_STATE_TX_IDLE;
    amdtpcCb.core.rxState = AMDTP_STATE_INIT;
    amdtpcCb.core.timeoutTimer.handlerId = handlerId;

    amdtpcCb.core.lastRxPktSn = 0;
    amdtpcCb.core.txPktSn = 0;

    resetPkt(&amdtpcCb.core.rxPkt);
    amdtpcCb.core.rxPkt.data = rxPktBuf;

    resetPkt(&amdtpcCb.core.txPkt);
    amdtpcCb.core.txPkt.data = txPktBuf;

    resetPkt(&amdtpcCb.core.ackPkt);
    amdtpcCb.core.ackPkt.data = ackPktBuf;

    amdtpcCb.core.recvCback = recvCback;
    amdtpcCb.core.transCback = transCback;

    amdtpcCb.core.txTimeoutMs = TX_TIMEOUT_DEFAULT;

    amdtpcCb.core.data_sender_func = amdtpcSendData;
    amdtpcCb.core.ack_sender_func = amdtpcSendAck;
}

static void
amdtpc_conn_close(dmEvt_t *pMsg)
{
    /* clear connection */
    WsfTimerStop(&amdtpcCb.core.timeoutTimer);
    amdtpcCb.txReady = false;
    amdtpcCb.core.txState = AMDTP_STATE_TX_IDLE;
    amdtpcCb.core.rxState = AMDTP_STATE_INIT;
    amdtpcCb.core.lastRxPktSn = 0;
    amdtpcCb.core.txPktSn = 0;
    resetPkt(&amdtpcCb.core.rxPkt);
    resetPkt(&amdtpcCb.core.txPkt);
    resetPkt(&amdtpcCb.core.ackPkt);
}

void
amdtpc_start(uint16_t rxHdl, uint16_t ackHdl, uint8_t timerEvt)
{
    amdtpcCb.txReady = true;
    amdtpcCb.attRxHdl = rxHdl;
    amdtpcCb.attAckHdl = ackHdl;
    amdtpcCb.core.timeoutTimer.msg.event = timerEvt;

    dmConnId_t connId;

    if ((connId = AppConnIsOpen()) == DM_CONN_ID_NONE)
    {
        APP_TRACE_INFO0("amdtpc_start() no connection\n");
        return;
    }

    amdtpcCb.core.attMtuSize = AttGetMtu(connId);
    APP_TRACE_INFO1("MTU size = %d bytes", amdtpcCb.core.attMtuSize);
}

//*****************************************************************************
//
// Timer Expiration handler
//
//*****************************************************************************
void
amdtpc_timeout_timer_expired(wsfMsgHdr_t *pMsg)
{
    uint8_t data[1];
    data[0] = amdtpcCb.core.txPktSn;
    APP_TRACE_INFO1("amdtpc tx timeout, txPktSn = %d", amdtpcCb.core.txPktSn);
    AmdtpSendControl(&amdtpcCb.core, AMDTP_CONTROL_RESEND_REQ, data, 1);
    // fire a timer for receiving an AMDTP_STATUS_RESEND_REPLY ACK
    WsfTimerStartMs(&amdtpcCb.core.timeoutTimer, amdtpcCb.core.txTimeoutMs);
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcValueNtf
 *
 *  \brief  Process a received ATT notification.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static uint8_t
amdtpcValueNtf(attEvt_t *pMsg)
{
    eAmdtpStatus_t status = AMDTP_STATUS_UNKNOWN_ERROR;
    amdtpPacket_t *pkt = NULL;
#if 0
    APP_TRACE_INFO0("receive ntf data\n");
    APP_TRACE_INFO1("handle = 0x%x\n", pMsg->handle);
    for (int i = 0; i < pMsg->valueLen; i++)
    {
        APP_TRACE_INFO1("%02x ", pMsg->pValue[i]);
    }
    APP_TRACE_INFO0("\n");
#endif

    if (pMsg->handle == amdtpcCb.attRxHdl)
    {
        status = AmdtpReceivePkt(&amdtpcCb.core, &amdtpcCb.core.rxPkt, pMsg->valueLen, pMsg->pValue);
    }
    else if ( pMsg->handle == amdtpcCb.attAckHdl )
    {
        status = AmdtpReceivePkt(&amdtpcCb.core, &amdtpcCb.core.ackPkt, pMsg->valueLen, pMsg->pValue);
    }

    if (status == AMDTP_STATUS_RECEIVE_DONE)
    {
        if (pMsg->handle == amdtpcCb.attRxHdl)
        {
            pkt = &amdtpcCb.core.rxPkt;
        }
        else if (pMsg->handle == amdtpcCb.attAckHdl)
        {
            pkt = &amdtpcCb.core.ackPkt;
        }

        AmdtpPacketHandler(&amdtpcCb.core, (eAmdtpPktType_t)pkt->header.pktType, pkt->len - AMDTP_CRC_SIZE_IN_PKT, pkt->data);
    }

    return ATT_SUCCESS;
}

static void
amdtpcHandleWriteResponse(attEvt_t *pMsg)
{
    //APP_TRACE_INFO2("amdtpcHandleWriteResponse, status = %d, hdl = 0x%x\n", pMsg->hdr.status, pMsg->handle);
    if (pMsg->hdr.status == ATT_SUCCESS && pMsg->handle == amdtpcCb.attRxHdl)
    {
        amdtpcCb.txReady = true;
        // process next data
        AmdtpSendPacketHandler(&amdtpcCb.core);
    }
}

void
amdtpc_proc_msg(wsfMsgHdr_t *pMsg)
{
    if (pMsg->event == DM_CONN_OPEN_IND)
    {
    }
    else if (pMsg->event == DM_CONN_CLOSE_IND)
    {
        amdtpc_conn_close((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_UPDATE_IND)
    {
    }
    else if (pMsg->event == amdtpcCb.core.timeoutTimer.msg.event)
    {
       amdtpc_timeout_timer_expired(pMsg);
    }
    else if (pMsg->event == ATTC_WRITE_CMD_RSP)
    {
        amdtpcHandleWriteResponse((attEvt_t *) pMsg);
    }
    else if (pMsg->event == ATTC_HANDLE_VALUE_NTF)
    {
        amdtpcValueNtf((attEvt_t *) pMsg);
    }
}

//*****************************************************************************
//
//! @brief Send data to Server via write command
//!
//! @param type - packet type
//! @param encrypted - is packet encrypted
//! @param enableACK - does client need to response
//! @param buf - data
//! @param len - data length
//!
//! @return status
//
//*****************************************************************************
eAmdtpStatus_t
AmdtpcSendPacket(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len)
{
    //
    // Check if the service is idle to send
    //
    if ( amdtpcCb.core.txState != AMDTP_STATE_TX_IDLE )
    {
        APP_TRACE_INFO1("data sending failed, tx state = %d", amdtpcCb.core.txState);
        return AMDTP_STATUS_BUSY;
    }

    //
    // Check if data length is valid
    //
    if ( len > AMDTP_MAX_PAYLOAD_SIZE )
    {
        APP_TRACE_INFO1("data sending failed, exceed maximum payload, len = %d.", len);
        return AMDTP_STATUS_INVALID_PKT_LENGTH;
    }

    //
    // Check if ready to send notification
    //
    if ( !amdtpcCb.txReady )
    {
        //set in callback amdtpsHandleValueCnf
        APP_TRACE_INFO1("data sending failed, not ready for notification.", NULL);
        return AMDTP_STATUS_TX_NOT_READY;
    }

    AmdtpBuildPkt(&amdtpcCb.core, type, encrypted, enableACK, buf, len);

    // send packet
    AmdtpSendPacketHandler(&amdtpcCb.core);

    return AMDTP_STATUS_SUCCESS;
}
