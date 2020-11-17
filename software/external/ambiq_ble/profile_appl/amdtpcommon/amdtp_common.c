//*****************************************************************************
//
//! @file amdtp_common.c
//!
//! @brief This file provides the shared functions for the AMDTP service.
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
#include <stdlib.h>
#include "amdtp_common.h"
#include "amota_crc32.h"
#include "am_util.h"

extern void amdtps_timeout_timer_expired(void *data, UINT16 datalen);

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
        AMDTP_TRC("Invalid packet!!!\n");
        AmdtpSendReply(amdtpCb, AMDTP_STATUS_INVALID_PKT_LENGTH, NULL, 0);
        return AMDTP_STATUS_INVALID_PKT_LENGTH;
    }

    // new packet
    if (pkt->offset == 0)
    {
        BT_UNPACK_LE_2_BYTE(&pkt->len, pValue);
        BT_UNPACK_LE_2_BYTE(&header, &pValue[2]);
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
        AMDTP_TRC("pkt len = 0x%x\n", pkt->len);
        AMDTP_TRC("pkt header = 0x%x\n", header);
#endif
        AMDTP_TRC("type = %d, sn = %d\n", pkt->header.pktType, pkt->header.pktSn);
        AMDTP_TRC("enc = %d, ackEnabled = %d\n", pkt->header.encrypted,  pkt->header.ackEnabled);
    }

    // make sure we have enough space for new data
    if (pkt->offset + len - dataIdx > AMDTP_PACKET_SIZE)
    {
        AMDTP_TRC("not enough buffer size!!!\n");
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
        BT_UNPACK_LE_4_BYTE(&peerCrc, pkt->data + pkt->len - AMDTP_CRC_SIZE_IN_PKT);
        calDataCrc = AmotaCrc32(0xFFFFFFFFU, pkt->len - AMDTP_CRC_SIZE_IN_PKT, pkt->data);
#ifdef AMDTP_DEBUG_ON
        AMDTP_TRC("calDataCrc = 0x%x\n", calDataCrc);
        AMDTP_TRC("peerCrc = 0x%x\n", peerCrc);
        AMDTP_TRC("len: %d\n", pkt->len);
#endif

        if (peerCrc != calDataCrc)
        {
            AMDTP_TRC("crc error\n");

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
#ifdef AMDTP_DEBUG_ON
    AMDTP_TRC("received packet type = %d, len = %d\n", type, len);
#endif
    
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
            if (BT_TIMER_HANDLE_INIT_VAL != amdtpCb->timeoutTimer)
            {
#ifdef AMDTP_DEBUG_ON                
                API_RESULT retval = API_SUCCESS;
                retval = BT_stop_timer (amdtpCb->timeoutTimer);
                AMDTP_TRC (
                "[AMDTP]: Stopping Timer with result 0x%04X, timer handle %p\n",
                retval, amdtpCb->timeoutTimer);
#else
                BT_stop_timer (amdtpCb->timeoutTimer);
#endif                
                amdtpCb->timeoutTimer = BT_TIMER_HANDLE_INIT_VAL;
            }

            if (amdtpCb->txState != AMDTP_STATE_TX_IDLE)
            {
#ifdef AMDTP_DEBUG_ON                
                AMDTP_TRC("set txState back to idle, state = %d\n", amdtpCb->txState);
#endif                
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
                AMDTP_TRC("resendPktSn = %d, lastRxPktSn = %d\n", resendPktSn, amdtpCb->lastRxPktSn);
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
                    AMDTP_ERR("resendPktSn = %d, lastRxPktSn = %d\n", resendPktSn, amdtpCb->lastRxPktSn);
                }
            }
            else
            {
                AMDTP_TRC("unexpected contrl = %d\n", control);
            }
            resetPkt(&amdtpCb->ackPkt);
        }
            break;

        default:
        break;
    }
}

void
AmdtpBuildPkt(amdtpCb_t *amdtpCb, eAmdtpPktType_t type, BOOLEAN encrypted, BOOLEAN enableACK, uint8_t *buf, uint16_t len)
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
    calDataCrc = AmotaCrc32(0xFFFFFFFFU, len, buf);

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

    buf[0] = status;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }
    st = amdtpCb->ack_sender_func(AMDTP_PKT_TYPE_ACK, false, false, buf, len + 1);
    if (st != AMDTP_STATUS_SUCCESS)
    {
        AMDTP_ERR("AmdtpSendReply status = %d\n", status);
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

    buf[0] = control;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }
    st = amdtpCb->ack_sender_func(AMDTP_PKT_TYPE_CONTROL, false, false, buf, len + 1);
    if (st != AMDTP_STATUS_SUCCESS)
    {
        AMDTP_ERR("AmdtpSendControl status = %d\n", st);
    }
}

void
AmdtpSendPacketHandler(amdtpCb_t *amdtpCb)
{
    uint16_t transferSize = 0;
    uint16_t remainingBytes = 0;
    amdtpPacket_t *txPkt = &amdtpCb->txPkt;
    API_RESULT retval = API_SUCCESS;

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
        if (BT_TIMER_HANDLE_INIT_VAL != amdtpCb->timeoutTimer)
        {
            retval = BT_stop_timer (amdtpCb->timeoutTimer);
            AMDTP_TRC (
            "[AMDTP]: Stopping Timer with result 0x%04X, timer handle %p\n",
            retval, amdtpCb->timeoutTimer);
            amdtpCb->timeoutTimer = BT_TIMER_HANDLE_INIT_VAL;
        }

        retval = BT_start_timer
             (
                 &amdtpCb->timeoutTimer,
                 amdtpCb->txTimeout,
                 amdtps_timeout_timer_expired,
                 NULL,
                 0
             );
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
