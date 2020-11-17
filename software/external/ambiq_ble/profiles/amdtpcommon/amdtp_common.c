// ****************************************************************************
//
//  amdtp_common.c
//! @file
//!
//! @brief This file provides the shared functions for the AMDTP service.
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
#include "amdtp_common.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "att_api.h"
#include "am_util_debug.h"
#include "crc32.h"
#include "am_util.h"

void
resetPkt(amdtpPacket_t *pkt)
{
    pkt->offset = 0;
    pkt->header.pktType = AMDTP_PKT_TYPE_UNKNOWN;
    pkt->len = 0;
}

eAmdtpStatus_t
AmdtpReceivePkt(amdtpCb_t *amdtpCb, amdtpPacket_t *pkt, uint16_t len, uint8_t *pValue)
{
    uint8_t dataIdx = 0;
    uint32_t calDataCrc = 0;
    uint16_t header = 0;

    if (pkt->offset == 0 && len < AMDTP_PREFIX_SIZE_IN_PKT)
    {
        APP_TRACE_INFO0("Invalid packet!!!");
        AmdtpSendReply(amdtpCb, AMDTP_STATUS_INVALID_PKT_LENGTH, NULL, 0);
        return AMDTP_STATUS_INVALID_PKT_LENGTH;
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
        dataIdx = AMDTP_PREFIX_SIZE_IN_PKT;
        if (pkt->header.pktType == AMDTP_PKT_TYPE_DATA)
        {
            amdtpCb->rxState = AMDTP_STATE_GETTING_DATA;
        }
#ifdef AMDTP_DEBUG_ON
        APP_TRACE_INFO1("pkt len = 0x%x", pkt->len);
        APP_TRACE_INFO1("pkt header = 0x%x", header);
#endif
        APP_TRACE_INFO2("type = %d, sn = %d", pkt->header.pktType, pkt->header.pktSn);
        APP_TRACE_INFO2("enc = %d, ackEnabled = %d", pkt->header.encrypted,  pkt->header.ackEnabled);
    }

    // make sure we have enough space for new data
    if (pkt->offset + len - dataIdx > AMDTP_PACKET_SIZE)
    {
        APP_TRACE_INFO0("not enough buffer size!!!");
        if (pkt->header.pktType == AMDTP_PKT_TYPE_DATA)
        {
            amdtpCb->rxState = AMDTP_STATE_RX_IDLE;
        }
        // reset pkt
        resetPkt(pkt);
        AmdtpSendReply(amdtpCb, AMDTP_STATUS_INSUFFICIENT_BUFFER, NULL, 0);
        return AMDTP_STATUS_INSUFFICIENT_BUFFER;
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
        BYTES_TO_UINT32(peerCrc, pkt->data + pkt->len - AMDTP_CRC_SIZE_IN_PKT);
        calDataCrc = CalcCrc32(0xFFFFFFFFU, pkt->len - AMDTP_CRC_SIZE_IN_PKT, pkt->data);
#ifdef AMDTP_DEBUG_ON
        APP_TRACE_INFO1("calDataCrc = 0x%x ", calDataCrc);
        APP_TRACE_INFO1("peerCrc = 0x%x", peerCrc);
        APP_TRACE_INFO1("len: %d", pkt->len);
#endif

        if (peerCrc != calDataCrc)
        {
            APP_TRACE_INFO0("crc error\n");

            if (pkt->header.pktType == AMDTP_PKT_TYPE_DATA)
            {
                amdtpCb->rxState = AMDTP_STATE_RX_IDLE;
            }
            // reset pkt
            resetPkt(pkt);

            AmdtpSendReply(amdtpCb, AMDTP_STATUS_CRC_ERROR, NULL, 0);

            return AMDTP_STATUS_CRC_ERROR;
        }

        return AMDTP_STATUS_RECEIVE_DONE;
    }

    return AMDTP_STATUS_RECEIVE_CONTINUE;
}

//*****************************************************************************
//
// AMDTP packet handler
//
//*****************************************************************************
void
AmdtpPacketHandler(amdtpCb_t *amdtpCb, eAmdtpPktType_t type, uint16_t len, uint8_t *buf)
{
    // APP_TRACE_INFO2("received packet type = %d, len = %d\n", type, len);

    switch(type)
    {
        case AMDTP_PKT_TYPE_DATA:
            //
            // data package recevied
            //
            // record packet serial number
            amdtpCb->lastRxPktSn = amdtpCb->rxPkt.header.pktSn;
            AmdtpSendReply(amdtpCb, AMDTP_STATUS_SUCCESS, NULL, 0);
            if (amdtpCb->recvCback)
            {
                amdtpCb->recvCback(buf, len);
            }

            amdtpCb->rxState = AMDTP_STATE_RX_IDLE;
            resetPkt(&amdtpCb->rxPkt);
            break;

        case AMDTP_PKT_TYPE_ACK:
        {
            eAmdtpStatus_t status = (eAmdtpStatus_t)buf[0];
            // stop tx timeout timer
            WsfTimerStop(&amdtpCb->timeoutTimer);

            if (amdtpCb->txState != AMDTP_STATE_TX_IDLE)
            {
                // APP_TRACE_INFO1("set txState back to idle, state = %d\n", amdtpCb->txState);
                amdtpCb->txState = AMDTP_STATE_TX_IDLE;
            }

            if (status == AMDTP_STATUS_CRC_ERROR || status == AMDTP_STATUS_RESEND_REPLY)
            {
                // resend packet
                AmdtpSendPacketHandler(amdtpCb);
            }
            else
            {
                // increase packet serial number if send successfully
                if (status == AMDTP_STATUS_SUCCESS)
                {
                    amdtpCb->txPktSn++;
                    if (amdtpCb->txPktSn == 16)
                    {
                        amdtpCb->txPktSn = 0;
                    }
                }

                // packet transfer successful or other error
                // reset packet
                resetPkt(&amdtpCb->txPkt);

                // notify application layer
                if (amdtpCb->transCback)
                {
                    amdtpCb->transCback(status);
                }
            }
            resetPkt(&amdtpCb->ackPkt);
        }
            break;

        case AMDTP_PKT_TYPE_CONTROL:
        {
            eAmdtpControl_t control = (eAmdtpControl_t)buf[0];
            uint8_t resendPktSn = buf[1];
            if (control == AMDTP_CONTROL_RESEND_REQ)
            {
                APP_TRACE_INFO2("resendPktSn = %d, lastRxPktSn = %d", resendPktSn, amdtpCb->lastRxPktSn);
                amdtpCb->rxState = AMDTP_STATE_RX_IDLE;
                resetPkt(&amdtpCb->rxPkt);
                if (resendPktSn > amdtpCb->lastRxPktSn)
                {
                    AmdtpSendReply(amdtpCb, AMDTP_STATUS_RESEND_REPLY, NULL, 0);
                }
                else if (resendPktSn == amdtpCb->lastRxPktSn)
                {
                    AmdtpSendReply(amdtpCb, AMDTP_STATUS_SUCCESS, NULL, 0);
                }
                else
                {
                    APP_TRACE_WARN2("resendPktSn = %d, lastRxPktSn = %d", resendPktSn, amdtpCb->lastRxPktSn);
                }
            }
            else
            {
                APP_TRACE_WARN1("unexpected contrl = %d\n", control);
            }
            resetPkt(&amdtpCb->ackPkt);
        }
            break;

        default:
        break;
    }
}

void
AmdtpBuildPkt(amdtpCb_t *amdtpCb, eAmdtpPktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len)
{
    uint16_t header = 0;
    uint32_t calDataCrc;
    amdtpPacket_t *pkt;

    if (type == AMDTP_PKT_TYPE_DATA)
    {
        pkt = &amdtpCb->txPkt;
        header = amdtpCb->txPktSn << PACKET_SN_BIT_OFFSET;
    }
    else
    {
        pkt = &amdtpCb->ackPkt;
    }

    //
    // Prepare header frame to be sent first
    //
    // length
    pkt->len = len + AMDTP_PREFIX_SIZE_IN_PKT + AMDTP_CRC_SIZE_IN_PKT;
    pkt->data[0]  = (len + AMDTP_CRC_SIZE_IN_PKT) & 0xff;
    pkt->data[1]  = ((len + AMDTP_CRC_SIZE_IN_PKT) >> 8) & 0xff;

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
    memcpy(&(pkt->data[AMDTP_PREFIX_SIZE_IN_PKT]), buf, len);
    calDataCrc = CalcCrc32(0xFFFFFFFFU, len, buf);

    // add checksum
    pkt->data[AMDTP_PREFIX_SIZE_IN_PKT + len] = (calDataCrc & 0xff);
    pkt->data[AMDTP_PREFIX_SIZE_IN_PKT + len + 1] = ((calDataCrc >> 8) & 0xff);
    pkt->data[AMDTP_PREFIX_SIZE_IN_PKT + len + 2] = ((calDataCrc >> 16) & 0xff);
    pkt->data[AMDTP_PREFIX_SIZE_IN_PKT + len + 3] = ((calDataCrc >> 24) & 0xff);
}

//*****************************************************************************
//
// Send Reply to Sender
//
//*****************************************************************************
void
AmdtpSendReply(amdtpCb_t *amdtpCb, eAmdtpStatus_t status, uint8_t *data, uint16_t len)
{
    uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN] = {0};
    eAmdtpStatus_t st;

    WSF_ASSERT(len < ATT_DEFAULT_PAYLOAD_LEN);

    buf[0] = status;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }
    st = amdtpCb->ack_sender_func(AMDTP_PKT_TYPE_ACK, false, false, buf, len + 1);
    if (st != AMDTP_STATUS_SUCCESS)
    {
        APP_TRACE_WARN1("AmdtpSendReply status = %d\n", status);
    }
}


//*****************************************************************************
//
// Send control message to Receiver
//
//*****************************************************************************
void
AmdtpSendControl(amdtpCb_t *amdtpCb, eAmdtpControl_t control, uint8_t *data, uint16_t len)
{
    uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN] = {0};
    eAmdtpStatus_t st;

    WSF_ASSERT(len < ATT_DEFAULT_PAYLOAD_LEN);

    buf[0] = control;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }
    st = amdtpCb->ack_sender_func(AMDTP_PKT_TYPE_CONTROL, false, false, buf, len + 1);
    if (st != AMDTP_STATUS_SUCCESS)
    {
        APP_TRACE_WARN1("AmdtpSendControl status = %d\n", st);
    }
}

void
AmdtpSendPacketHandler(amdtpCb_t *amdtpCb)
{
    uint16_t transferSize = 0;
    uint16_t remainingBytes = 0;
    amdtpPacket_t *txPkt = &amdtpCb->txPkt;

    if ( amdtpCb->txState == AMDTP_STATE_TX_IDLE )
    {
        txPkt->offset = 0;
        amdtpCb->txState = AMDTP_STATE_SENDING;
    }

    if ( txPkt->offset >= txPkt->len )
    {
        // done sent packet
        amdtpCb->txState = AMDTP_STATE_WAITING_ACK;
        // start tx timeout timer
        WsfTimerStartMs(&amdtpCb->timeoutTimer, amdtpCb->txTimeoutMs);
    }
    else
    {
        remainingBytes = txPkt->len - txPkt->offset;
        transferSize = ((amdtpCb->attMtuSize - 3) > remainingBytes)
                                            ? remainingBytes
                                            : (amdtpCb->attMtuSize - 3);
        // send packet
        amdtpCb->data_sender_func(&txPkt->data[txPkt->offset], transferSize);
        txPkt->offset += transferSize;
    }
}
