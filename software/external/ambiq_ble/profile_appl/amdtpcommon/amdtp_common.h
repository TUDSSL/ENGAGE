//*****************************************************************************
//
//! @file amdtp_common.h
//!
//! @brief This file provides the shared functions for the AMDTP service.
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

#ifndef AMDTP_COMMON_H
#define AMDTP_COMMON_H

#include "BT_api.h"

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
#define BT_MODULE_BIT_MASK_AMDTPS       0x00020000
#define BT_MODULE_ID_AMDTPS             (BT_MODULE_PAGE_2 | BT_MODULE_BIT_MASK_AMDTPS)
#define AMDTP_ERR(...)                  BT_debug_error(BT_MODULE_ID_AMDTPS, __VA_ARGS__)
#define AMDTP_TRC(...)                  BT_debug_trace(BT_MODULE_ID_AMDTPS, __VA_ARGS__)

#define AMDTP_MAX_PAYLOAD_SIZE          2048 //512
#define AMDTP_PACKET_SIZE               (AMDTP_MAX_PAYLOAD_SIZE + AMDTP_PREFIX_SIZE_IN_PKT + AMDTP_CRC_SIZE_IN_PKT)    // Bytes
#define AMDTP_LENGTH_SIZE_IN_PKT        2
#define AMDTP_HEADER_SIZE_IN_PKT        2
#define AMDTP_CRC_SIZE_IN_PKT           4
#define AMDTP_PREFIX_SIZE_IN_PKT        AMDTP_LENGTH_SIZE_IN_PKT + AMDTP_HEADER_SIZE_IN_PKT

#define PACKET_TYPE_BIT_OFFSET          12
#define PACKET_TYPE_BIT_MASK            (0xf << PACKET_TYPE_BIT_OFFSET)
#define PACKET_SN_BIT_OFFSET            8
#define PACKET_SN_BIT_MASK              (0xf << PACKET_SN_BIT_OFFSET)
#define PACKET_ENCRYPTION_BIT_OFFSET    7
#define PACKET_ENCRYPTION_BIT_MASK      (0x1 << PACKET_ENCRYPTION_BIT_OFFSET)
#define PACKET_ACK_BIT_OFFSET           6
#define PACKET_ACK_BIT_MASK             (0x1 << PACKET_ACK_BIT_OFFSET)

#define TX_TIMEOUT_DEFAULT              1   // 1 second
#define ATT_DEFAULT_PAYLOAD_LEN         20

//
// amdtp states
//
typedef enum eAmdtpState
{
    AMDTP_STATE_INIT,
    AMDTP_STATE_TX_IDLE,
    AMDTP_STATE_RX_IDLE,
    AMDTP_STATE_SENDING,
    AMDTP_STATE_GETTING_DATA,
    AMDTP_STATE_WAITING_ACK,
    AMDTP_STATE_MAX
}eAmdtpState_t;

//
// amdtp packet type
//
typedef enum eAmdtpPktType
{
    AMDTP_PKT_TYPE_UNKNOWN,
    AMDTP_PKT_TYPE_DATA,
    AMDTP_PKT_TYPE_ACK,
    AMDTP_PKT_TYPE_CONTROL,
    AMDTP_PKT_TYPE_MAX
}eAmdtpPktType_t;

typedef enum eAmdtpControl
{
    AMDTP_CONTROL_RESEND_REQ,
    AMDTP_CONTROL_MAX
}eAmdtpControl_t;

//
// amdtp status
//
typedef enum eAmdtpStatus
{
    AMDTP_STATUS_SUCCESS,
    AMDTP_STATUS_CRC_ERROR,
    AMDTP_STATUS_INVALID_METADATA_INFO,
    AMDTP_STATUS_INVALID_PKT_LENGTH,
    AMDTP_STATUS_INSUFFICIENT_BUFFER,
    AMDTP_STATUS_UNKNOWN_ERROR,
    AMDTP_STATUS_BUSY,
    AMDTP_STATUS_TX_NOT_READY,              // no connection or tx busy
    AMDTP_STATUS_RESEND_REPLY,
    AMDTP_STATUS_RECEIVE_CONTINUE,
    AMDTP_STATUS_RECEIVE_DONE,
    AMDTP_STATUS_MAX
}eAmdtpStatus_t;

//
// packet prefix structure
//
typedef struct
{
    uint8_t     pktType : 4;
    uint8_t     pktSn   : 4;
    uint8_t     encrypted : 1;
    uint32_t    ackEnabled : 1;
    uint32_t    reserved : 6;               // Reserved for future usage
}
amdtpPktHeader_t;

//
// packet
//
typedef struct
{
    uint16_t            offset;
    uint16_t            len;                        // data plus checksum
    amdtpPktHeader_t    header;
    uint8_t             *data;
}
amdtpPacket_t;

/*! Application data reception callback */
typedef void (*amdtpRecvCback_t)(uint8_t *buf, uint16_t len);

/*! Application data transmission result callback */
typedef void (*amdtpTransCback_t)(eAmdtpStatus_t status);

typedef void (*amdtp_reply_func_t)(eAmdtpStatus_t status, uint8_t *data, uint16_t len);

typedef void (*amdtp_packet_handler_func_t)(eAmdtpPktType_t type, uint16_t len, uint8_t *buf);

typedef eAmdtpStatus_t (*amdtp_ack_sender_func_t)(eAmdtpPktType_t type, BOOLEAN encrypted, BOOLEAN enableACK, uint8_t *buf, uint16_t len);

typedef void (*amdtp_data_sender_func_t)(uint8_t *buf, uint16_t len);

typedef struct
{
    eAmdtpState_t               txState;
    eAmdtpState_t               rxState;
    amdtpPacket_t               rxPkt;
    amdtpPacket_t               txPkt;
    amdtpPacket_t               ackPkt;
    uint8_t                     txPktSn;                // data packet serial number for Tx
    uint8_t                     lastRxPktSn;            // last received data packet serial number
    uint16_t                    attMtuSize;
    BT_timer_handle             timeoutTimer;           // timeout timer after DTP update done
    uint8_t                     txTimeout;              // timeout in second unit
    amdtpRecvCback_t            recvCback;              // application callback for data reception
    amdtpTransCback_t           transCback;             // application callback for tx complete status
    amdtp_data_sender_func_t    data_sender_func;
    amdtp_ack_sender_func_t     ack_sender_func;
}
amdtpCb_t;

//*****************************************************************************
//
// function definitions
//
//*****************************************************************************

void
AmdtpBuildPkt(amdtpCb_t *amdtpCb, eAmdtpPktType_t type, BOOLEAN encrypted, BOOLEAN enableACK, uint8_t *buf, uint16_t len);

eAmdtpStatus_t
AmdtpReceivePkt(amdtpCb_t *amdtpCb, amdtpPacket_t *pkt, uint16_t len, uint8_t *pValue);

void
AmdtpSendReply(amdtpCb_t *amdtpCb, eAmdtpStatus_t status, uint8_t *data, uint16_t len);

void
AmdtpSendControl(amdtpCb_t *amdtpCb, eAmdtpControl_t control, uint8_t *data, uint16_t len);

void
AmdtpSendPacketHandler(amdtpCb_t *amdtpCb);

void
AmdtpPacketHandler(amdtpCb_t *amdtpCb, eAmdtpPktType_t type, uint16_t len, uint8_t *buf);

void
resetPkt(amdtpPacket_t *pkt);

#endif // AMDTP_COMMON_H
