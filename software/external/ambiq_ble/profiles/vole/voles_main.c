//*****************************************************************************
//
//  voles_main.c
//! @file
//!
//! @brief This file provides the main application for the VOLE service.
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
#include "svc_amvole.h"
#include "app_api.h"
#include "app_hw.h"
#include "voles_api.h"
#include "am_util_debug.h"
#include "crc32.h"

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "sbc.h"
#include "vole_common.h"
#include "att_defs.h"
#include "vole_board_config.h"
#include "voice_data.h"

#include "ae_api.h"

//*****************************************************************************
//
// Global variables
//
//*****************************************************************************
eVoleCodecType g_vole_codec_type;
uint8_t g_ble_data_buffer[ATT_MAX_MTU];

extern bool_t g_start_voice_send;
//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

static sbc_t   g_SBCInstance;
int g_opusErr;
uint32_t g_audioSampleRate = 16000;
int32_t g_audioChannel = 1;

/* Control block */
static struct
{
    volesConn_t            conn[DM_CONN_MAX];      // connection control block
    bool_t                  txReady;                // TRUE if ready to send notifications
    wsfHandlerId_t          appHandlerId;
    VolesCfg_t             cfg;                    // configurable parameters
    voleCb_t               core;
}
volesCb;

extern bool am_app_KWD_AMA_tx_ver_exchange_send(void);
extern uint16_t am_app_KWD_AMA_stream_send(uint8_t* buf, uint32_t len);
extern void am_app_led_off(void);

static void voles_conn_parameter_req(void)
{
    hciConnSpec_t connSpec;
    connSpec.connIntervalMin = 6;   //(15/1.25);//(30/1.25);
    connSpec.connIntervalMax = 12;  //(15/1.25);//(30/1.25);
    connSpec.connLatency = 0;       //0;
    connSpec.supTimeout = 400;
    connSpec.minCeLen = 0;
    connSpec.maxCeLen = 0xffff;     //fixme
    DmConnUpdate(1, &connSpec);
}

//*****************************************************************************
//
// Connection Open event
//
//*****************************************************************************
static void
voles_conn_open(dmEvt_t *pMsg)
{
    hciLeConnCmplEvt_t *evt = (hciLeConnCmplEvt_t*) pMsg;

    APP_TRACE_INFO0("connection opened\n");
    APP_TRACE_INFO1("handle = 0x%x\n", evt->handle);
    APP_TRACE_INFO1("role = 0x%x\n", evt->role);
    APP_TRACE_INFO3("addrMSB = %02x%02x%02x%02x%02x%02x\n", evt->peerAddr[0], evt->peerAddr[1], evt->peerAddr[2]);
    APP_TRACE_INFO3("addrLSB = %02x%02x%02x%02x%02x%02x\n", evt->peerAddr[3], evt->peerAddr[4], evt->peerAddr[5]);
    APP_TRACE_INFO1("connInterval = %d x 1.25 ms\n", evt->connInterval);
    APP_TRACE_INFO1("connLatency = %d\n", evt->connLatency);
    APP_TRACE_INFO1("supTimeout = %d ms\n", evt->supTimeout * 10);

    if ( evt->connInterval > 12 )
    {
        voles_conn_parameter_req();
    }
}

//*****************************************************************************
//
// Connection Update event
//
//*****************************************************************************
static void
voles_conn_update(dmEvt_t *pMsg)
{
    hciLeConnUpdateCmplEvt_t *evt = (hciLeConnUpdateCmplEvt_t*) pMsg;

    APP_TRACE_INFO1("connection update status = 0x%x", evt->status);
    APP_TRACE_INFO1("handle = 0x%x", evt->handle);
    APP_TRACE_INFO1("connInterval = 0x%x", evt->connInterval);
    APP_TRACE_INFO1("connLatency = 0x%x", evt->connLatency);
    APP_TRACE_INFO1("supTimeout = 0x%x", evt->supTimeout);


    if ( evt->connInterval > (15 / 1.25) )
    {
        // retry
        voles_conn_parameter_req();
    }
}



int32_t voles_msbc_encode_voice_data(uint8_t *input, uint8_t *output, uint16_t len)
{
    int32_t i32CompressedLen = 0;

    if ((input == NULL) || (output == NULL))
    {
      return 0;
    }

    sbc_encoder_encode(&g_SBCInstance, input, len,
            output, CODEC_MSBC_OUTPUT_SIZE, (int *)&i32CompressedLen);

    APP_TRACE_INFO2("voles encode, input len:%d, compressedLen:%d", len, i32CompressedLen);

    return i32CompressedLen;
}

int voles_set_codec_type(eVoleCodecType codec_type)
{
    APP_TRACE_INFO1("set codec type:%s", ((codec_type == 0) ? "MSBC code" : "OPUS codec"));

    if (codec_type != VOLE_CODEC_TYPE_INVALID)
    {
        g_vole_codec_type = codec_type;
        return codec_type;
    }
    else
    {
        return VOLE_CODEC_TYPE_INVALID;
    }
}

eVoleCodecType voles_get_codec_type(void)
{
    return g_vole_codec_type;
}

// get the size for each sending packet
int voles_ble_buffer_size(void)
{
    eVoleCodecType codec_type = voles_get_codec_type();
    int buffer_size = 0;

    switch(codec_type)
    {
        case MSBC_CODEC_IN_USE:
            buffer_size = BLE_MSBC_DATA_BUFFER_SIZE;
        break;

        case OPUS_CODEC_IN_USE:
            buffer_size = BLE_OPUS_DATA_BUFFER_SIZE;
        break;

        default:
        break;
    }

    return buffer_size;
}

void voles_transmit_voice_data(void)
{
    volePacket_t *txPkt = &volesCb.core.txPkt;
    uint32_t      offset = txPkt->offset;
    int           remain_data_len = 0;
    int           enc_data_len = 0;
    uint8_t       output_data[100] = {0};
    int      output_len = 0;
    static int    index = 0;
    eVoleCodecType codec_type = voles_get_codec_type();

    txPkt->len = sizeof(voice_data);
    remain_data_len = txPkt->len - offset;

    if (codec_type == MSBC_CODEC_IN_USE)
    {
        enc_data_len = (remain_data_len>CODEC_MSBC_INPUT_SIZE)?CODEC_MSBC_INPUT_SIZE:remain_data_len;
    }
    else if (codec_type == OPUS_CODEC_IN_USE)
    {
        enc_data_len = (remain_data_len>CODEC_OPUS_INPUT_SIZE)?CODEC_OPUS_INPUT_SIZE:remain_data_len;
    }

    APP_TRACE_INFO3("send %s encode data, total len:%d, offset:%d", (codec_type == MSBC_CODEC_IN_USE) ? "mSBC" : "Opus",
                    txPkt->len, offset);

    if (offset >= txPkt->len)
    {
        am_app_led_off();
        g_start_voice_send = FALSE;
        memset(output_data, 0x0, sizeof(output_data));
        txPkt->offset = 0;
    }
    else
    {
        if (codec_type == MSBC_CODEC_IN_USE)
        {
            output_len = voles_msbc_encode_voice_data(&voice_data[offset + AUD_HEADER_LEN], output_data, enc_data_len);
            txPkt->offset += enc_data_len;
        }
        else if (codec_type == OPUS_CODEC_IN_USE)
        {
            output_len = audio_enc_encode_frame((short *)&voice_data[offset + AUD_HEADER_LEN], enc_data_len, output_data);
            APP_TRACE_INFO1("opus encode, output_len:%d", output_len);
            if (output_len == (CODEC_OPUS_OUTPUT_SIZE))
            {
                txPkt->offset += (2*CODEC_OPUS_INPUT_SIZE);
                //APP_TRACE_INFO1("opus_encode: ret:%d", output_len);
            }
            else
            {
                txPkt->offset += enc_data_len;
            }
        }
    }

    if ( output_len > 0 )
    {
        memcpy(&g_ble_data_buffer[index], output_data, output_len);
        index += output_len;
    }

    if ( index >= voles_ble_buffer_size() || output_len <= 0 )
    {
        if (index >0)
        {
            am_app_KWD_AMA_stream_send(g_ble_data_buffer, index);
            index = 0;
            memset(g_ble_data_buffer, 0x0, sizeof(g_ble_data_buffer));
        }
    }
    else
    {
        uint8_t output_size = ((voles_get_codec_type() == MSBC_CODEC_IN_USE) ? CODEC_MSBC_OUTPUT_SIZE : (CODEC_OPUS_OUTPUT_SIZE));

        if (output_len == output_size)
        {
            voles_transmit_voice_data();
        }
        else if (output_len>0 && output_len<output_size)
        {
            am_app_KWD_AMA_stream_send(output_data, output_len);
            index = 0;
        }
    }
}

//extern opus_encoder_t* octopus_encoder_create(int option);
/*************************************************************************************************/
/*!
 *  \fn     volesHandleValueCnf
 *
 *  \brief  Handle a received ATT handle value confirm.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void
volesHandleValueCnf(attEvt_t *pMsg)
{
    if (pMsg->hdr.status == ATT_SUCCESS)
    {
        if (g_start_voice_send)
        {
            voles_transmit_voice_data();
        }
    }
    else
    {
        APP_TRACE_WARN2("cnf status = %d, hdl = 0x%x\n", pMsg->hdr.status, pMsg->handle);
    }
}

void voles_opus_encoder_init(void)
{
    //
    // Opus codec init
    //
    audio_enc_init(0);

    APP_TRACE_INFO0("Opus encoder initialization is finished!\r\n\n");

}

//*****************************************************************************
//6
//! @brief initialize vole service
//!
//! @param handlerId - connection handle
//! @param pCfg - configuration parameters
//!
//! @return None
//
//*****************************************************************************
void voles_init(wsfHandlerId_t handlerId, eVoleCodecType codec_type)
{
    memset(&volesCb, 0, sizeof(volesCb));
    volesCb.appHandlerId = handlerId;

    volesCb.core.txPkt.data = voice_data;

    if (codec_type == MSBC_CODEC_IN_USE)
    {
        sbc_encode_init(&g_SBCInstance, 1);  //0: SBC
    }
    else if (codec_type == OPUS_CODEC_IN_USE)
    {
        voles_opus_encoder_init();
    }
}

static void
voles_conn_close(dmEvt_t *pMsg)
{
    hciDisconnectCmplEvt_t *evt = (hciDisconnectCmplEvt_t*) pMsg;
    dmConnId_t connId = evt->hdr.param;
    /* clear connection */
    volesCb.conn[connId - 1].connId = DM_CONN_ID_NONE;
    volesCb.conn[connId - 1].voleToSend = FALSE;

    WsfTimerStop(&volesCb.core.timeoutTimer);
    volesCb.core.txState = VOLE_STATE_INIT;
    volesCb.core.rxState = VOLE_STATE_RX_IDLE;
    volesCb.core.lastRxPktSn = 0;
    volesCb.core.txPktSn = 0;
    VoleResetPkt(&volesCb.core.rxPkt);
    VoleResetPkt(&volesCb.core.txPkt);
    VoleResetPkt(&volesCb.core.ackPkt);
}

uint8_t
voles_write_cback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                  uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    APP_TRACE_INFO3("write cb, len:%d, value %x %x", len, pValue[0], pValue[1]);

    return ATT_SUCCESS;
}

void
voles_start(dmConnId_t connId, uint8_t timerEvt, uint8_t voleCccIdx)
{
    //
    // set conn id
    //
    volesCb.conn[connId - 1].connId = connId;
    volesCb.conn[connId - 1].voleToSend = TRUE;

    volesCb.core.timeoutTimer.msg.event = timerEvt;
    volesCb.core.txState = VOLE_STATE_TX_IDLE;
    volesCb.txReady = true;

    volesCb.core.attMtuSize = AttGetMtu(connId);
    APP_TRACE_INFO1("MTU size = %d bytes", volesCb.core.attMtuSize);
}

void
voles_stop(dmConnId_t connId)
{
    //
    // clear connection
    //
    volesCb.conn[connId - 1].connId = DM_CONN_ID_NONE;
    volesCb.conn[connId - 1].voleToSend = FALSE;

    volesCb.core.txState = VOLE_STATE_INIT;
    volesCb.txReady = false;
}


void voles_proc_msg(wsfMsgHdr_t *pMsg)
{
    if (pMsg->event == DM_CONN_OPEN_IND)
    {
        voles_conn_open((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_CLOSE_IND)
    {
        voles_conn_close((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_UPDATE_IND)
    {
        voles_conn_update((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == ATTS_HANDLE_VALUE_CNF)
    {
        volesHandleValueCnf((attEvt_t *) pMsg);
    }
}
