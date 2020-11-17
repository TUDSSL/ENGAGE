//*****************************************************************************
//
//  amdtps_main.c
//! @file
//!
//! @brief This file provides the main application for the AMDTP service.
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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_buf.h"    //for WsfBufAlloc and WsfBufFree
#include "bstream.h"
#include "att_api.h"
#include "svc_ch.h"
#include "svc_amdtp.h"
#include "app_api.h"
#include "app_hw.h"
#include "amdtps_api.h"
#include "am_util_debug.h"
#include "crc32.h"

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

//*****************************************************************************
//
// Global variables
//
//*****************************************************************************

uint8_t rxPktBuf[AMDTP_PACKET_SIZE];
uint8_t txPktBuf[AMDTP_PACKET_SIZE];
uint8_t ackPktBuf[20];

#if defined(AMDTPS_RXONLY) || defined(AMDTPS_RX2TX)
static int totalLen = 0;
#endif

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

/* Control block */
static struct
{
    amdtpsConn_t            conn[DM_CONN_MAX];      // connection control block
    bool_t                  txReady;                // TRUE if ready to send notifications
    wsfHandlerId_t          appHandlerId;
    AmdtpsCfg_t             cfg;                    // configurable parameters
    amdtpCb_t               core;
}
amdtpsCb;

//*****************************************************************************
//
// Connection Open event
//
//*****************************************************************************
static void
amdtps_conn_open(dmEvt_t *pMsg)
{
    hciLeConnCmplEvt_t *evt = (hciLeConnCmplEvt_t*) pMsg;

    APP_TRACE_INFO0("connection opened\n");
    APP_TRACE_INFO1("handle = 0x%x\n", evt->handle);
    APP_TRACE_INFO1("role = 0x%x\n", evt->role);
    APP_TRACE_INFO3("addrMSB = %02x%02x%02x%02x%02x%02x\n", evt->peerAddr[0], evt->peerAddr[1], evt->peerAddr[2]);
    APP_TRACE_INFO3("addrLSB = %02x%02x%02x%02x%02x%02x\n", evt->peerAddr[3], evt->peerAddr[4], evt->peerAddr[5]);
    APP_TRACE_INFO1("connInterval = 0x%x\n", evt->connInterval);
    APP_TRACE_INFO1("connLatency = 0x%x\n", evt->connLatency);
    APP_TRACE_INFO1("supTimeout = 0x%x\n", evt->supTimeout);
}

//*****************************************************************************
//
// Connection Update event
//
//*****************************************************************************
static void
amdtps_conn_update(dmEvt_t *pMsg)
{
    hciLeConnUpdateCmplEvt_t *evt = (hciLeConnUpdateCmplEvt_t*) pMsg;

    APP_TRACE_INFO1("connection update status = 0x%x", evt->status);
    APP_TRACE_INFO1("handle = 0x%x", evt->handle);
    APP_TRACE_INFO1("connInterval = 0x%x", evt->connInterval);
    APP_TRACE_INFO1("connLatency = 0x%x", evt->connLatency);
    APP_TRACE_INFO1("supTimeout = 0x%x", evt->supTimeout);
}

static void amdtpsSetupToSend(void)
{
    amdtpsConn_t    *pConn = amdtpsCb.conn;
    uint8_t       i;

    for (i = 0; i < DM_CONN_MAX; i++, pConn++)
    {
        if (pConn->connId != DM_CONN_ID_NONE)
        {
            pConn->amdtpToSend = TRUE;
        }
    }
}

//*****************************************************************************
//
// Find Next Connection to Send on
//
//*****************************************************************************
static amdtpsConn_t*
amdtps_find_next2send(void)
{
    amdtpsConn_t *pConn = amdtpsCb.conn;
    return pConn;
}

//*****************************************************************************
//
// Send Notification to Client
//
//*****************************************************************************
static void
amdtpsSendData(uint8_t *buf, uint16_t len)
{
    amdtpsSetupToSend();
    amdtpsConn_t *pConn = amdtps_find_next2send();
    /* send notification */
    if (pConn)
    {
#ifdef AMDTP_DEBUG_ON
        APP_TRACE_INFO1("amdtpsSendData(), Send to connId = %d\n", pConn->connId);
#endif
        AttsHandleValueNtf(pConn->connId, AMDTPS_TX_HDL, len, buf);

        pConn->amdtpToSend = false;
        amdtpsCb.txReady = false;
    }
    else
    {
        APP_TRACE_WARN1("Invalid Conn = %d\n", pConn->connId);
    }
}

static eAmdtpStatus_t
amdtpsSendAck(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len)
{
    AmdtpBuildPkt(&amdtpsCb.core, type, encrypted, enableACK, buf, len);
    // send packet
    amdtpsSetupToSend();
    amdtpsConn_t *pConn = amdtps_find_next2send();
    /* send notification */
    if (pConn)
    {
#ifdef AMDTP_DEBUG_ON
        APP_TRACE_INFO1("amdtpsSendAck(), Send to connId = %d\n", pConn->connId);
#endif
        AttsHandleValueNtf(pConn->connId, AMDTPS_ACK_HDL, amdtpsCb.core.ackPkt.len, amdtpsCb.core.ackPkt.data);

        pConn->amdtpToSend = false;
    }
    else
    {
        APP_TRACE_WARN1("Invalid Conn = %d\n", pConn->connId);
        return AMDTP_STATUS_TX_NOT_READY;
    }

    return AMDTP_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Timer Expiration handler
//
//*****************************************************************************
static void
amdtps_timeout_timer_expired(wsfMsgHdr_t *pMsg)
{
    uint8_t data[1];
    data[0] = amdtpsCb.core.txPktSn;
    APP_TRACE_INFO1("amdtps tx timeout, txPktSn = %d", amdtpsCb.core.txPktSn);
    AmdtpSendControl(&amdtpsCb.core, AMDTP_CONTROL_RESEND_REQ, data, 1);
    // fire a timer for receiving an AMDTP_STATUS_RESEND_REPLY ACK
    WsfTimerStartMs(&amdtpsCb.core.timeoutTimer, amdtpsCb.core.txTimeoutMs);
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpsHandleValueCnf
 *
 *  \brief  Handle a received ATT handle value confirm.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void
amdtpsHandleValueCnf(attEvt_t *pMsg)
{
    //APP_TRACE_INFO2("Cnf status = %d, handle = 0x%x\n", pMsg->hdr.status, pMsg->handle);
    if (pMsg->hdr.status == ATT_SUCCESS)
    {
#if !defined(AMDTPS_RXONLY) && !defined(AMDTPS_RX2TX)
        if (pMsg->handle == AMDTPS_TX_HDL)
        {
            amdtpsCb.txReady = true;
            // process next data
            AmdtpSendPacketHandler(&amdtpsCb.core);
#ifdef AMDTPS_TXTEST
            // fixme when last packet, continue to send next one.
            if (amdtpsCb.core.txState == AMDTP_STATE_WAITING_ACK)
            {
                uint8_t temp[3];
                temp[0] = AMDTP_STATUS_SUCCESS;
                AmdtpPacketHandler(&amdtpsCb.core, AMDTP_PKT_TYPE_ACK, 3, temp);
            }
#endif
        }
#endif
    }
    else
    {
#if 0 //def AMDTPS_TXTEST
        // workround for ATT timeout issue
        /* Connection control block */
        typedef struct
        {

          wsfQueue_t        prepWriteQueue;     /* prepare write queue */
          wsfTimer_t        idleTimer;          /* service discovery idle timer */
          uint16_t          handle;             /* connection handle */
          uint16_t          mtu;                /* connection mtu */
          dmConnId_t        connId;             /* DM connection ID */
          bool_t            mtuSent;            /* MTU req or rsp sent */
          bool_t            flowDisabled;       /* Data flow disabled */
          bool_t            transTimedOut;      /* ATT transaction timed out */
        } attCcb_t;
        extern attCcb_t *attCcbByConnId(dmConnId_t connId);
        if (pMsg->hdr.status == 0x71)
        {
            attCcb_t *ccb = attCcbByConnId(1);
            ccb->transTimedOut = FALSE;
        }
#endif
        APP_TRACE_WARN2("cnf status = %d, hdl = 0x%x\n", pMsg->hdr.status, pMsg->handle);
    }
}

//*****************************************************************************
//
//! @brief initialize amdtp service
//!
//! @param handlerId - connection handle
//! @param pCfg - configuration parameters
//!
//! @return None
//
//*****************************************************************************
void
amdtps_init(wsfHandlerId_t handlerId, AmdtpsCfg_t *pCfg, amdtpRecvCback_t recvCback, amdtpTransCback_t transCback)
{
    memset(&amdtpsCb, 0, sizeof(amdtpsCb));
    amdtpsCb.appHandlerId = handlerId;
    amdtpsCb.txReady = false;
    amdtpsCb.core.txState = AMDTP_STATE_INIT;
    amdtpsCb.core.rxState = AMDTP_STATE_RX_IDLE;
    amdtpsCb.core.timeoutTimer.handlerId = handlerId;
    for (int i = 0; i < DM_CONN_MAX; i++)
    {
        amdtpsCb.conn[i].connId = DM_CONN_ID_NONE;
    }

    amdtpsCb.core.lastRxPktSn = 0;
    amdtpsCb.core.txPktSn = 0;

    resetPkt(&amdtpsCb.core.rxPkt);
    amdtpsCb.core.rxPkt.data = rxPktBuf;

    resetPkt(&amdtpsCb.core.txPkt);
    amdtpsCb.core.txPkt.data = txPktBuf;

    resetPkt(&amdtpsCb.core.ackPkt);
    amdtpsCb.core.ackPkt.data = ackPktBuf;

    amdtpsCb.core.recvCback = recvCback;
    amdtpsCb.core.transCback = transCback;

    amdtpsCb.core.txTimeoutMs = TX_TIMEOUT_DEFAULT;

    amdtpsCb.core.data_sender_func = amdtpsSendData;
    amdtpsCb.core.ack_sender_func = amdtpsSendAck;
}

static void
amdtps_conn_close(dmEvt_t *pMsg)
{
    hciDisconnectCmplEvt_t *evt = (hciDisconnectCmplEvt_t*) pMsg;
    dmConnId_t connId = evt->hdr.param;
    /* clear connection */
    amdtpsCb.conn[connId - 1].connId = DM_CONN_ID_NONE;
    amdtpsCb.conn[connId - 1].amdtpToSend = FALSE;

    WsfTimerStop(&amdtpsCb.core.timeoutTimer);
    amdtpsCb.core.txState = AMDTP_STATE_INIT;
    amdtpsCb.core.rxState = AMDTP_STATE_RX_IDLE;
    amdtpsCb.core.lastRxPktSn = 0;
    amdtpsCb.core.txPktSn = 0;
    resetPkt(&amdtpsCb.core.rxPkt);
    resetPkt(&amdtpsCb.core.txPkt);
    resetPkt(&amdtpsCb.core.ackPkt);

#if defined(AMDTPS_RXONLY) || defined(AMDTPS_RX2TX)
    APP_TRACE_INFO1("*** RECEIVED TOTAL %d ***", totalLen);
    totalLen = 0;
#endif

}

uint8_t
amdtps_write_cback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                   uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    eAmdtpStatus_t status = AMDTP_STATUS_UNKNOWN_ERROR;
    amdtpPacket_t *pkt = NULL;
#if 0
    uint16_t i = 0;
    APP_TRACE_INFO0("============= data arrived start ===============");
    for (i = 0; i < len; i++)
    {
        APP_TRACE_INFO1("%x\t", pValue[i]);
    }
    APP_TRACE_INFO0("");
    APP_TRACE_INFO0("============= data arrived end ===============");
#endif
    if (handle == AMDTPS_RX_HDL)
    {
#if defined(AMDTPS_RX2TX)
        amdtpsSendData(pValue, len);
#endif
#if defined(AMDTPS_RXONLY) || defined(AMDTPS_RX2TX)
        totalLen += len;
        APP_TRACE_INFO2("received data len %d, total %d", len, totalLen);
        return ATT_SUCCESS;
#else /* RXONLY && RX2TX */
        status = AmdtpReceivePkt(&amdtpsCb.core, &amdtpsCb.core.rxPkt, len, pValue);
#endif /* RXONLY && RX2TX */
    }
    else if (handle == AMDTPS_ACK_HDL)
    {
        status = AmdtpReceivePkt(&amdtpsCb.core, &amdtpsCb.core.ackPkt, len, pValue);
    }

    if (status == AMDTP_STATUS_RECEIVE_DONE)
    {
        if (handle == AMDTPS_RX_HDL)
        {
            pkt = &amdtpsCb.core.rxPkt;
        }
        else if (handle == AMDTPS_ACK_HDL)
        {
            pkt = &amdtpsCb.core.ackPkt;
        }

        AmdtpPacketHandler(&amdtpsCb.core, (eAmdtpPktType_t)pkt->header.pktType, pkt->len - AMDTP_CRC_SIZE_IN_PKT, pkt->data);
    }

    return ATT_SUCCESS;
}

void
amdtps_start(dmConnId_t connId, uint8_t timerEvt, uint8_t amdtpCccIdx)
{
    //
    // set conn id
    //
    amdtpsCb.conn[connId - 1].connId = connId;
    amdtpsCb.conn[connId - 1].amdtpToSend = TRUE;

    amdtpsCb.core.timeoutTimer.msg.event = timerEvt;
    amdtpsCb.core.txState = AMDTP_STATE_TX_IDLE;
    amdtpsCb.txReady = true;

    amdtpsCb.core.attMtuSize = AttGetMtu(connId);
    APP_TRACE_INFO1("MTU size = %d bytes", amdtpsCb.core.attMtuSize);
}

void
amdtps_stop(dmConnId_t connId)
{
    //
    // clear connection
    //
    amdtpsCb.conn[connId - 1].connId = DM_CONN_ID_NONE;
    amdtpsCb.conn[connId - 1].amdtpToSend = FALSE;

    amdtpsCb.core.txState = AMDTP_STATE_INIT;
    amdtpsCb.txReady = false;
}


void
amdtps_proc_msg(wsfMsgHdr_t *pMsg)
{
    if (pMsg->event == DM_CONN_OPEN_IND)
    {
        amdtps_conn_open((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_CLOSE_IND)
    {
        amdtps_conn_close((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_UPDATE_IND)
    {
        amdtps_conn_update((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == amdtpsCb.core.timeoutTimer.msg.event)
    {
        amdtps_timeout_timer_expired(pMsg);
    }
    else if (pMsg->event == ATTS_HANDLE_VALUE_CNF)
    {
        amdtpsHandleValueCnf((attEvt_t *) pMsg);
    }
}

//*****************************************************************************
//
//! @brief Send data to Client via notification
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
AmdtpsSendPacket(eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len)
{
    //
    // Check if ready to send notification
    //
    if ( !amdtpsCb.txReady )
    {
        //set in callback amdtpsHandleValueCnf
        APP_TRACE_INFO1("data sending failed, not ready for notification.", NULL);
        return AMDTP_STATUS_TX_NOT_READY;
    }

    //
    // Check if the service is idle to send
    //
    if ( amdtpsCb.core.txState != AMDTP_STATE_TX_IDLE )
    {
        APP_TRACE_INFO1("data sending failed, tx state = %d", amdtpsCb.core.txState);
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

    AmdtpBuildPkt(&amdtpsCb.core, type, encrypted, enableACK, buf, len);

    // send packet
    AmdtpSendPacketHandler(&amdtpsCb.core);

    return AMDTP_STATUS_SUCCESS;
}
