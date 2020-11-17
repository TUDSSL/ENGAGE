// ****************************************************************************
//
//  vole_common.h
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

#ifndef VOLE_COMMON_H
#define VOLE_COMMON_H

#include "wsf_types.h"
#include "wsf_timer.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
#define VOLE_MAX_PAYLOAD_SIZE          10//2048//512
#define VOLE_PACKET_SIZE               (VOLE_MAX_PAYLOAD_SIZE + VOLE_PREFIX_SIZE_IN_PKT + VOLE_CRC_SIZE_IN_PKT)    // Bytes
#define VOLE_LENGTH_SIZE_IN_PKT        2
#define VOLE_HEADER_SIZE_IN_PKT        2
#define VOLE_CRC_SIZE_IN_PKT           4
#define VOLE_PREFIX_SIZE_IN_PKT        VOLE_LENGTH_SIZE_IN_PKT + VOLE_HEADER_SIZE_IN_PKT

#define PACKET_TYPE_BIT_OFFSET          12
#define PACKET_TYPE_BIT_MASK            (0xf << PACKET_TYPE_BIT_OFFSET)
#define PACKET_SN_BIT_OFFSET            8
#define PACKET_SN_BIT_MASK              (0xf << PACKET_SN_BIT_OFFSET)
#define PACKET_ENCRYPTION_BIT_OFFSET    7
#define PACKET_ENCRYPTION_BIT_MASK      (0x1 << PACKET_ENCRYPTION_BIT_OFFSET)
#define PACKET_ACK_BIT_OFFSET           6
#define PACKET_ACK_BIT_MASK             (0x1 << PACKET_ACK_BIT_OFFSET)

#define TX_TIMEOUT_DEFAULT              1000

#define MANUFACT_ADV          0x1234
#define AUD_HEADER_LEN        44


struct au_header
{
    uint32_t magic;         /* '.snd' */
    uint32_t hdr_size;      /* size of header (min 24) */
    uint32_t data_size;     /* size of data */
    uint32_t encoding;      /* see to AU_FMT_XXXX */
    uint32_t sample_rate;   /* sample rate */
    uint32_t channels;      /* number of channels (voices) */
};

typedef enum codecType
{
    MSBC_CODEC_IN_USE = 0,
    OPUS_CODEC_IN_USE = 1,

    VOLE_CODEC_TYPE_INVALID = 0xFF
}eVoleCodecType;

//
// Vole states
//
typedef enum eVoleState
{
    VOLE_STATE_INIT,
    VOLE_STATE_TX_IDLE,
    VOLE_STATE_RX_IDLE,
    VOLE_STATE_SENDING,
    VOLE_STATE_GETTING_DATA,
    VOLE_STATE_WAITING_ACK,
    VOLE_STATE_MAX
}eVoleState_t;

//
// Vole packet type
//
typedef enum eVolePktType
{
    VOLE_PKT_TYPE_UNKNOWN,
    VOLE_PKT_TYPE_DATA,
    VOLE_PKT_TYPE_ACK,
    VOLE_PKT_TYPE_CONTROL,
    VOLE_PKT_TYPE_MAX
}eVolePktType_t;

typedef enum eVoleControl
{
    VOLE_CONTROL_RESEND_REQ,
    VOLE_CONTROL_MAX
}eVoleControl_t;

//
// Vole status
//
typedef enum eVoleStatus
{
    VOLE_STATUS_SUCCESS,
    VOLE_STATUS_CRC_ERROR,
    VOLE_STATUS_INVALID_METADATA_INFO,
    VOLE_STATUS_INVALID_PKT_LENGTH,
    VOLE_STATUS_INSUFFICIENT_BUFFER,
    VOLE_STATUS_UNKNOWN_ERROR,
    VOLE_STATUS_BUSY,
    VOLE_STATUS_TX_NOT_READY,              // no connection or tx busy
    VOLE_STATUS_RESEND_REPLY,
    VOLE_STATUS_RECEIVE_CONTINUE,
    VOLE_STATUS_RECEIVE_DONE,
    VOLE_STATUS_MAX
}eVoleStatus_t;

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
volePktHeader_t;

//
// packet
//
typedef struct
{
    uint32_t            offset;
    uint32_t            len;                        // data plus checksum
    volePktHeader_t    header;
    uint8_t             *data;
}
volePacket_t;

/*! Application data reception callback */
typedef void (*voleRecvCback_t)(uint8_t *buf, uint16_t len);

/*! Application data transmission result callback */
typedef void (*voleTransCback_t)(eVoleStatus_t status);

typedef void (*vole_reply_func_t)(eVoleStatus_t status, uint8_t *data, uint16_t len);

typedef void (*vole_packet_handler_func_t)(eVolePktType_t type, uint16_t len, uint8_t *buf);

typedef eVoleStatus_t (*vole_ack_sender_func_t)(eVolePktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len);

typedef void (*vole_data_sender_func_t)(uint8_t *buf, uint16_t len);

typedef struct
{
    eVoleState_t               txState;
    eVoleState_t               rxState;
    volePacket_t               rxPkt;
    volePacket_t               txPkt;
    volePacket_t               ackPkt;
    uint8_t                     txPktSn;                // data packet serial number for Tx
    uint8_t                     lastRxPktSn;            // last received data packet serial number
    uint16_t                    attMtuSize;
   // int                         total_tx_len;
    wsfTimer_t                  timeoutTimer;           // timeout timer after DTP update done
    wsfTimerTicks_t             txTimeoutMs;
    voleRecvCback_t            recvCback;              // application callback for data reception
    voleTransCback_t           transCback;             // application callback for tx complete status
    vole_data_sender_func_t    data_sender_func;
    vole_ack_sender_func_t     ack_sender_func;
}
voleCb_t;

//*****************************************************************************
//
// function definitions
//
//*****************************************************************************

void
VoleBuildPkt(voleCb_t *voleCb, eVolePktType_t type, bool_t encrypted, bool_t enableACK, uint8_t *buf, uint16_t len);

eVoleStatus_t
VoleReceivePkt(voleCb_t *voleCb, volePacket_t *pkt, uint16_t len, uint8_t *pValue);

void
VoleSendReply(voleCb_t *voleCb, eVoleStatus_t status, uint8_t *data, uint16_t len);

void
VoleSendControl(voleCb_t *voleCb, eVoleControl_t control, uint8_t *data, uint16_t len);

void
VoleSendPacketHandler(voleCb_t *voleCb);

void
VolePacketHandler(voleCb_t *voleCb, eVolePktType_t type, uint16_t len, uint8_t *buf);

void
VoleResetPkt(volePacket_t *pkt);

#ifdef __cplusplus
}
#endif

#endif // VOLE_COMMON_H
