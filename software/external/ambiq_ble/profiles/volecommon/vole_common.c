// ****************************************************************************
//
//  vole_common.c
//! @file
//!
//! @brief This file provides the shared functions for the Vole service.
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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "vole_common.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "att_api.h"
#include "am_util_debug.h"
#include "crc32.h"
#include "am_util.h"

void
VoleResetPkt(volePacket_t *pkt)
{
  //  pkt->offset = 0;
  //  pkt->header.pktType = VOLE_PKT_TYPE_UNKNOWN;
  //  pkt->len = 0;
}

eVoleStatus_t
VoleReceivePkt(voleCb_t *voleCb, volePacket_t *pkt, uint16_t len, uint8_t *pValue)
{
    uint8_t dataIdx = 0;
    uint32_t calDataCrc = 0;
    uint16_t header = 0;

    if (pkt->offset == 0 && len < VOLE_PREFIX_SIZE_IN_PKT)
    {
        APP_TRACE_INFO0("Invalid packet!!!");
        VoleSendReply(voleCb, VOLE_STATUS_INVALID_PKT_LENGTH, NULL, 0);
        return VOLE_STATUS_INVALID_PKT_LENGTH;
    }

    // new packet
    if (pkt->offset == 0)
    {
        BYTES_TO_UINT16(pkt->len, pValue);
        BYTES_TO_UINT16(header, &pValue[2]);
        pkt->header.pktType = (header & PACKET_TYPE_BIT_MASK) >> PACKET_TYPE_BIT_OFFSET;
        pkt->header.pktSn = (header & PACKET_SN_BIT_MASK) >> PACKET_SN_BIT_OFFSET;
        pkt->header.encrypted = (header & PACKET_ENCRYPTION_BIT_MASK) >> PACKET_ENCRYPTION_BIT_OFFSET;
        pkt->header.ackEnabled = (header & PACKET_ACK_BIT_MASK) >> PACKET_ACK_BIT_OFFSET;
        dataIdx = VOLE_PREFIX_SIZE_IN_PKT;
        if (pkt->header.pktType == VOLE_PKT_TYPE_DATA)
        {
            voleCb->rxState = VOLE_STATE_GETTING_DATA;
        }
#ifdef VOLE_DEBUG_ON
        APP_TRACE_INFO1("pkt len = 0x%x", pkt->len);
        APP_TRACE_INFO1("pkt header = 0x%x", header);
#endif
        APP_TRACE_INFO2("type = %d, sn = %d", pkt->header.pktType, pkt->header.pktSn);
        APP_TRACE_INFO2("enc = %d, ackEnabled = %d", pkt->header.encrypted,  pkt->header.ackEnabled);
    }

    // make sure we have enough space for new data
    if (pkt->offset + len - dataIdx > VOLE_PACKET_SIZE)
    {
        APP_TRACE_INFO0("not enough buffer size!!!");
        if (pkt->header.pktType == VOLE_PKT_TYPE_DATA)
        {
            voleCb->rxState = VOLE_STATE_RX_IDLE;
        }
        // reset pkt
        VoleResetPkt(pkt);
        VoleSendReply(voleCb, VOLE_STATUS_INSUFFICIENT_BUFFER, NULL, 0);
        return VOLE_STATUS_INSUFFICIENT_BUFFER;
    }

    // copy new data into buffer and also save crc into it if it's the last frame in a packet
    // 4 bytes crc is included in pkt length
    memcpy(pkt->data + pkt->offset, pValue + dataIdx, len - dataIdx);
    pkt->offset += (len - dataIdx);

    // whole packet received
    if (pkt->offset >= pkt->len)
    {
        uint32_t peerCrc = 0;
        //
        // check CRC
        //
        BYTES_TO_UINT32(peerCrc, pkt->data + pkt->len - VOLE_CRC_SIZE_IN_PKT);
        calDataCrc = CalcCrc32(0xFFFFFFFFU, pkt->len - VOLE_CRC_SIZE_IN_PKT, pkt->data);
#ifdef VOLE_DEBUG_ON
        APP_TRACE_INFO1("calDataCrc = 0x%x ", calDataCrc);
        APP_TRACE_INFO1("peerCrc = 0x%x", peerCrc);
        APP_TRACE_INFO1("len: %d", pkt->len);
#endif

        if (peerCrc != calDataCrc)
        {
            APP_TRACE_INFO0("crc error\n");

            if (pkt->header.pktType == VOLE_PKT_TYPE_DATA)
            {
                voleCb->rxState = VOLE_STATE_RX_IDLE;
            }
            // reset pkt
            VoleResetPkt(pkt);

            VoleSendReply(voleCb, VOLE_STATUS_CRC_ERROR, NULL, 0);

            return VOLE_STATUS_CRC_ERROR;
        }

        return VOLE_STATUS_RECEIVE_DONE;
    }

    return VOLE_STATUS_RECEIVE_CONTINUE;
}

//*****************************************************************************
//
// Vole packet handler
//
//*****************************************************************************
void
VolePacketHandler(voleCb_t *voleCb, eVolePktType_t type, uint16_t len, uint8_t *buf)
{
    // APP_TRACE_INFO2("received packet type = %d, len = %d\n", type, len);

    switch(type)
    {
        case VOLE_PKT_TYPE_DATA:
            //
            // data package recevied
            //
            // record packet serial number
            voleCb->lastRxPktSn = voleCb->rxPkt.header.pktSn;
            VoleSendReply(voleCb, VOLE_STATUS_SUCCESS, NULL, 0);
            if (voleCb->recvCback)
            {
                voleCb->recvCback(buf, len);
            }

            voleCb->rxState = VOLE_STATE_RX_IDLE;
            VoleResetPkt(&voleCb->rxPkt);
            break;

        case VOLE_PKT_TYPE_ACK:
        {
            eVoleStatus_t status = (eVoleStatus_t)buf[0];
            // stop tx timeout timer
            WsfTimerStop(&voleCb->timeoutTimer);

            if (voleCb->txState != VOLE_STATE_TX_IDLE)
            {
                // APP_TRACE_INFO1("set txState back to idle, state = %d\n", voleCb->txState);
                voleCb->txState = VOLE_STATE_TX_IDLE;
            }

            if (status == VOLE_STATUS_CRC_ERROR || status == VOLE_STATUS_RESEND_REPLY)
            {
                // resend packet
                VoleSendPacketHandler(voleCb);
            }
            else
            {
                // increase packet serial number if send successfully
                if (status == VOLE_STATUS_SUCCESS)
                {
                    voleCb->txPktSn++;
                    if (voleCb->txPktSn == 16)
                    {
                        voleCb->txPktSn = 0;
                    }
                }

                // packet transfer successful or other error
                // reset packet
                VoleResetPkt(&voleCb->txPkt);

                // notify application layer
                if (voleCb->transCback)
                {
                    voleCb->transCback(status);
                }
            }
            VoleResetPkt(&voleCb->ackPkt);
        }
            break;

        case VOLE_PKT_TYPE_CONTROL:
        {
            eVoleControl_t control = (eVoleControl_t)buf[0];
            uint8_t resendPktSn = buf[1];
            if (control == VOLE_CONTROL_RESEND_REQ)
            {
                APP_TRACE_INFO2("resendPktSn = %d, lastRxPktSn = %d", resendPktSn, voleCb->lastRxPktSn);
                voleCb->rxState = VOLE_STATE_RX_IDLE;
                VoleResetPkt(&voleCb->rxPkt);
                if (resendPktSn > voleCb->lastRxPktSn)
                {
                    VoleSendReply(voleCb, VOLE_STATUS_RESEND_REPLY, NULL, 0);
                }
                else if (resendPktSn == voleCb->lastRxPktSn)
                {
                    VoleSendReply(voleCb, VOLE_STATUS_SUCCESS, NULL, 0);
                }
                else
                {
                    APP_TRACE_WARN2("resendPktSn = %d, lastRxPktSn = %d", resendPktSn, voleCb->lastRxPktSn);
                }
            }
            else
            {
                APP_TRACE_WARN1("unexpected contrl = %d\n", control);
            }
            VoleResetPkt(&voleCb->ackPkt);
        }
            break;

        default:
        break;
    }
}

void
VoleBuildPkt(voleCb_t *voleCb, eVolePktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len)
{
    uint16_t header = 0;
    uint32_t calDataCrc;
    volePacket_t *pkt;

    if (type == VOLE_PKT_TYPE_DATA)
    {
        pkt = &voleCb->txPkt;
        header = voleCb->txPktSn << PACKET_SN_BIT_OFFSET;
    }
    else
    {
        pkt = &voleCb->ackPkt;
    }

    //
    // Prepare header frame to be sent first
    //
    // length
    pkt->len = len + VOLE_PREFIX_SIZE_IN_PKT + VOLE_CRC_SIZE_IN_PKT;
    pkt->data[0]  = (len + VOLE_CRC_SIZE_IN_PKT) & 0xff;
    pkt->data[1]  = ((len + VOLE_CRC_SIZE_IN_PKT) >> 8) & 0xff;

    // header
    header = header | (type << PACKET_TYPE_BIT_OFFSET);
    if (encrypted)
    {
        header = header | (1 << PACKET_ENCRYPTION_BIT_OFFSET);
    }
    if (enableACK)
    {
        header = header | (1 << PACKET_ACK_BIT_OFFSET);
    }
    pkt->data[2] = (header & 0xff);
    pkt->data[3] = (header >> 8);

    // copy data
    memcpy(&(pkt->data[VOLE_PREFIX_SIZE_IN_PKT]), buf, len);
    calDataCrc = CalcCrc32(0xFFFFFFFFU, len, buf);

    // add checksum
    pkt->data[VOLE_PREFIX_SIZE_IN_PKT + len] = (calDataCrc & 0xff);
    pkt->data[VOLE_PREFIX_SIZE_IN_PKT + len + 1] = ((calDataCrc >> 8) & 0xff);
    pkt->data[VOLE_PREFIX_SIZE_IN_PKT + len + 2] = ((calDataCrc >> 16) & 0xff);
    pkt->data[VOLE_PREFIX_SIZE_IN_PKT + len + 3] = ((calDataCrc >> 24) & 0xff);
}

//*****************************************************************************
//
// Send Reply to Sender
//
//*****************************************************************************
void
VoleSendReply(voleCb_t *voleCb, eVoleStatus_t status, uint8_t *data, uint16_t len)
{
    uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN] = {0};
    eVoleStatus_t st;

    WSF_ASSERT(len < ATT_DEFAULT_PAYLOAD_LEN);

    buf[0] = status;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }
    st = voleCb->ack_sender_func(VOLE_PKT_TYPE_ACK, false, false, buf, len + 1);
    if (st != VOLE_STATUS_SUCCESS)
    {
        APP_TRACE_WARN1("VoleSendReply status = %d\n", status);
    }
}


//*****************************************************************************
//
// Send control message to Receiver
//
//*****************************************************************************
void
VoleSendControl(voleCb_t *voleCb, eVoleControl_t control, uint8_t *data, uint16_t len)
{
    uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN] = {0};
    eVoleStatus_t st;

    WSF_ASSERT(len < ATT_DEFAULT_PAYLOAD_LEN);

    buf[0] = control;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }
    st = voleCb->ack_sender_func(VOLE_PKT_TYPE_CONTROL, false, false, buf, len + 1);
    if (st != VOLE_STATUS_SUCCESS)
    {
        APP_TRACE_WARN1("VoleSendControl status = %d\n", st);
    }
}

void
VoleSendPacketHandler(voleCb_t *voleCb)
{
    uint16_t transferSize = 0;
    uint16_t remainingBytes = 0;
    volePacket_t *txPkt = &voleCb->txPkt;

    if ( voleCb->txState == VOLE_STATE_TX_IDLE )
    {
        txPkt->offset = 0;
        voleCb->txState = VOLE_STATE_SENDING;
    }

    if ( txPkt->offset >= txPkt->len )
    {
        // done sent packet
        voleCb->txState = VOLE_STATE_WAITING_ACK;
        // start tx timeout timer
        WsfTimerStartMs(&voleCb->timeoutTimer, voleCb->txTimeoutMs);
    }
    else
    {
        remainingBytes = txPkt->len - txPkt->offset;
        transferSize = ((voleCb->attMtuSize - 3) > remainingBytes)
                                            ? remainingBytes
                                            : (voleCb->attMtuSize - 3);
        // send packet
        APP_TRACE_INFO1("VoleSendPacketHandler, send len:%d", transferSize);
        voleCb->data_sender_func(&txPkt->data[txPkt->offset], transferSize);
        txPkt->offset += transferSize;
    }
}
