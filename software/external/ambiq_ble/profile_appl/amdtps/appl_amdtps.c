//*****************************************************************************
//
//  appl_amdtps.c
//! @file
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

/**
 *  \file appl_amdtps.c
 *
 *  This file contains the AMDTP application.
  *  Sample applications detailed below:
 *      a. The Sensor, as defined by the Sepcification plays the GAP Peripheral
 *         role.
 *      b. The Sensor application has following sevice records:
 *           - GAP
 *           - GATT
 *           - Battery
 *           - Device Information and
 *           - AMDTP
 *         [NOTE]: Please see gatt_db.c for more details of the record.
 *      c. appl_manage_transfer routine takes care of handling peer
 *         configuration. This handling would be needed:
 *           - When Peer Configures Measurement Transfer by writting to the
 *             Characteristic Client Configuration of AMOTA Tx.
 *           - Subsequent reconnection with bonded device that had already
 *             configured the device for transfer. Please note it is mandatory
 *             for GATT Servers to remember the configurations of bonded GATT
 *             clients.
 *         In order to ensure the above mentioned configurations are correctly
 *         handled, the routine, appl_manage_transfer, is therefore called from:
 *           - gatt_db_amotas_handler and
 *           - appl_amdtps_connect
 *         [NOTE]: If link does not have the needed secruity for the service,
 *         transfer will not be initiated.
 */



/* --------------------------------------------- Header File Inclusion */
#include "appl.h"

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

#ifdef AMDTPS

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
    BOOLEAN                 txReady;              // TRUE if ready to send notifications
    AmdtpsCfg_t             cfg;                  // configurable parameters
    amdtpCb_t               core;
}
amdtpsCb;

/* --------------------------------------------- Static Global Variables */
static GATT_DB_HANDLE appl_amdtps_handle;
static ATT_ATTR_HANDLE appl_tx_hndl;
static ATT_ATTR_HANDLE appl_ack_hndl;

/* --------------------------------------------- Functions */

static bool sendDataContinuously = false;
static uint32_t counter = 0;

static void AmdtpsSendTestData(void)
{
    uint8_t data[236] = {0};
    eAmdtpStatus_t status;

    sendDataContinuously = true;
    *((uint32_t*)&(data[0])) = counter;

    status = AmdtpsSendPacket(AMDTP_PKT_TYPE_DATA, false, true, data, sizeof(data));
    if (status != AMDTP_STATUS_SUCCESS)
    {
        AMDTP_TRC("[AMDTP]: AmdtpsSendTestData() failed, status = %d\n", status);
    }
    else
    {
        counter++;
    }
}

void amdtpsDtpRecvCback(uint8_t * buf, uint16_t len)
{
    if (buf[0] == 1)
    {
        AMDTP_TRC("[AMDTP]: send test data\n");
        AmdtpsSendTestData();
    }
    else if (buf[0] == 2)
    {
        AMDTP_TRC("[AMDTP]: send test data stop\n");
        sendDataContinuously = false;
    }
}

void amdtpsDtpTransCback(eAmdtpStatus_t status)
{
#ifdef AMDTP_DEBUG_ON
    AMDTP_TRC("[AMDTP]: amdtpDtpTransCback =%d\n", status);
#endif
    if (status == AMDTP_STATUS_SUCCESS && sendDataContinuously)
    {
        AmdtpsSendTestData();
    }
}

void appl_amdtps_init(void)
{
    appl_amdtp_server_reinitialize();
}

void appl_amdtps_connect(APPL_HANDLE  * appl_handle)
{
    ATT_VALUE         value;
    UINT16            cli_cnfg;

    cli_cnfg = 0;

    appl_amdtps_handle.device_id = APPL_GET_DEVICE_HANDLE((*appl_handle));
    appl_amdtps_handle.char_id = GATT_CHAR_AMDTP_TX;
    appl_amdtps_handle.service_id = GATT_SER_AMDTP_INST;

    BT_gatt_db_get_char_val_hndl(&appl_amdtps_handle, &appl_tx_hndl);
    BT_gatt_db_get_char_cli_cnfg(&appl_amdtps_handle, &value);
    BT_UNPACK_LE_2_BYTE (&cli_cnfg, value.val);


    appl_amdtps_handle.char_id = GATT_CHAR_AMDTP_ACK;
    BT_gatt_db_get_char_val_hndl(&appl_amdtps_handle, &appl_ack_hndl);
    BT_gatt_db_get_char_cli_cnfg(&appl_amdtps_handle, &value);
    BT_UNPACK_LE_2_BYTE (&cli_cnfg, value.val);

    AMDTP_TRC (
    "[APPL]: Fetched Client Configuration (0x%04X) for Device (0x%02X)\n",
    cli_cnfg, APPL_GET_DEVICE_HANDLE((*appl_handle)));
    appl_manage_trasnfer(appl_amdtps_handle, cli_cnfg);
}

void appl_manage_trasnfer(GATT_DB_HANDLE handle, UINT16 config)
{
    APPL_HANDLE    appl_handle;
    API_RESULT     retval;

    UCHAR          security, ekey_size;

    AMDTP_TRC("[AMDTP]: appl_manage_trasnfer+ \n");
    /* Get required security for service */
    /* Get required security level */
    BT_gatt_db_get_service_security (&handle, &security);
    /* Get required encryption key size */
    BT_gatt_db_get_service_enc_key_size (&handle, &ekey_size);

    /* Verify if security requirements are available with the link */
    retval = appl_smp_assert_security
             (
                 &handle.device_id,
                 security,
                 ekey_size
             );

    /* Security requirements satisfied? */
    if (API_SUCCESS != retval)
    {
        /* No. Return */
        return;
    }

    /* Security requirements satisfied, go ahead with data transfer */

    retval = appl_get_handle_from_device_handle(handle.device_id, &appl_handle);

    if (API_SUCCESS != retval)
    {
        return;
    }

    if (GATT_CLI_CNFG_NOTIFICATION == config)
    {
        amdtpsCb.txReady = true;
        amdtpsCb.core.txState = AMDTP_STATE_TX_IDLE;
        AMDTP_TRC("[AMDTP]: notify registered \n");
#if defined(AMDTPS_TXTEST)
        counter = 0;
        AmdtpsSendTestData(); //fixme
#endif
    }
    else if (GATT_CLI_CNFG_DEFAULT == config)
    {
        amdtpsCb.txReady = false;
        AMDTP_TRC("[AMDTP]: notify unregistered \n");
    }
    else
    {
        /* Incorrect Configuration */
    }
}

//*****************************************************************************
//
// Send Notification to Client
//
//*****************************************************************************
static void amdtpsSendData(uint8_t *buf, uint16_t len)
{
    ATT_HANDLE_VALUE_PAIR hndl_val_param;
    API_RESULT retval;
    APPL_HANDLE appl_handle;

#ifdef AMDTP_DEBUG_ON
    AMDTP_TRC("Sending Tx On Handle 0x%04X\n", appl_tx_hndl);
#endif

    appl_get_handle_from_device_handle (appl_amdtps_handle.device_id, &appl_handle);
    hndl_val_param.handle =  appl_tx_hndl;
    hndl_val_param.value.val = buf;
    hndl_val_param.value.len = len;

    retval = BT_att_send_hndl_val_ntf
             (
                 &APPL_GET_ATT_INSTANCE(appl_handle),
                 &hndl_val_param
              );

    if (API_SUCCESS != retval)
    {
        AMDTP_ERR( "[** ERR **]: Failed to send, reason 0x%04X", retval);
    }
}

static eAmdtpStatus_t
amdtpsSendAck(eAmdtpPktType_t type, BOOLEAN encrypted, BOOLEAN enableACK, uint8_t *buf, uint16_t len)
{
    ATT_HANDLE_VALUE_PAIR hndl_val_param;
    API_RESULT retval;
    APPL_HANDLE appl_handle;

    AmdtpBuildPkt(&amdtpsCb.core, type, encrypted, enableACK, buf, len);

#ifdef AMDTP_DEBUG_ON
    AMDTP_TRC("Sending Ack On Handle 0x%04X\n", appl_ack_hndl);
#endif
    /* send notification */
    appl_get_handle_from_device_handle (appl_amdtps_handle.device_id, &appl_handle);
    hndl_val_param.handle =  appl_ack_hndl;
    hndl_val_param.value.val = amdtpsCb.core.ackPkt.data;
    hndl_val_param.value.len = amdtpsCb.core.ackPkt.len;

    retval = BT_att_send_hndl_val_ntf
             (
                 &APPL_GET_ATT_INSTANCE(appl_handle),
                 &hndl_val_param
              );

    if (API_SUCCESS != retval)
    {
        AMDTP_ERR( "[** ERR **]: Failed to send measurement, reason 0x%04X", retval);
        return AMDTP_STATUS_TX_NOT_READY;
    }

    return AMDTP_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Timer Expiration handler
//
//*****************************************************************************
void amdtps_timeout_timer_expired(void *data, UINT16 datalen)
{
    API_RESULT retval = API_SUCCESS;
    uint8_t ack[1];
    ack[0] = amdtpsCb.core.txPktSn;
    AMDTP_TRC("amdtps tx timeout, txPktSn = %d\n", amdtpsCb.core.txPktSn);
    AmdtpSendControl(&amdtpsCb.core, AMDTP_CONTROL_RESEND_REQ, ack, 1);
    // fire a timer for receiving an AMDTP_STATUS_RESEND_REPLY ACK
    if (BT_TIMER_HANDLE_INIT_VAL != amdtpsCb.core.timeoutTimer)
    {
        retval = BT_stop_timer (amdtpsCb.core.timeoutTimer);
        AMDTP_TRC (
        "[AMDTP]: Stopping Timer with result 0x%04X, timer handle %p\n",
        retval, amdtpsCb.core.timeoutTimer);
        amdtpsCb.core.timeoutTimer = BT_TIMER_HANDLE_INIT_VAL;
    }

    retval = BT_start_timer
         (
             &amdtpsCb.core.timeoutTimer,
             amdtpsCb.core.txTimeout,
             amdtps_timeout_timer_expired,
             NULL,
             0
         );
    AMDTP_TRC (
    "[AMDTP]: Started Timer with result 0x%04X, timer handle %p\n",
    retval, amdtpsCb.core.timeoutTimer);
}

void amdtpsHandleValueCnf(
               APPL_HANDLE    * appl_handle,
               UCHAR         * event_data,
               UINT16        event_datalen
               )
{
    ATT_ATTR_HANDLE  attr_handle;

    BT_UNPACK_LE_2_BYTE(&attr_handle, event_data);

#ifdef AMDTP_DEBUG_ON
    AMDTP_TRC("appl_handle 0x%x attr_handle = 0x%x\n", *appl_handle, attr_handle);
#endif

#if !defined(AMDTPS_RXONLY) && !defined(AMDTPS_RX2TX)
    if ( attr_handle == appl_tx_hndl )
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

void amdtps_mtu_update(APPL_HANDLE    * appl_handle, UINT16 t_mtu)
{
    UINT16 mtu = 0;

    BT_att_access_mtu(&APPL_GET_ATT_INSTANCE(*appl_handle),
        &mtu);

    AMDTP_TRC("appl_handle 0x%x t_mtu = %d %d\n", *appl_handle, t_mtu, mtu);
}

void appl_amdtp_server_reinitialize(void)
{
    API_RESULT retval = API_SUCCESS;

    memset(&amdtpsCb, 0, sizeof(amdtpsCb));
    amdtpsCb.txReady = false;
    amdtpsCb.core.txState = AMDTP_STATE_INIT;
    amdtpsCb.core.rxState = AMDTP_STATE_RX_IDLE;

    amdtpsCb.core.lastRxPktSn = 0;
    amdtpsCb.core.txPktSn = 0;

    resetPkt(&amdtpsCb.core.rxPkt);
    amdtpsCb.core.rxPkt.data = rxPktBuf;

    resetPkt(&amdtpsCb.core.txPkt);
    amdtpsCb.core.txPkt.data = txPktBuf;

    resetPkt(&amdtpsCb.core.ackPkt);
    amdtpsCb.core.ackPkt.data = ackPktBuf;

    amdtpsCb.core.recvCback = amdtpsDtpRecvCback;
    amdtpsCb.core.transCback = amdtpsDtpTransCback;

    amdtpsCb.core.data_sender_func = amdtpsSendData;
    amdtpsCb.core.ack_sender_func = amdtpsSendAck;

    amdtpsCb.core.txTimeout = TX_TIMEOUT_DEFAULT;

    if (BT_TIMER_HANDLE_INIT_VAL != amdtpsCb.core.timeoutTimer)
    {
        retval = BT_stop_timer (amdtpsCb.core.timeoutTimer);
        AMDTP_TRC (
        "[AMDTP]: Stopping Timer with result 0x%04X, timer handle %p\n",
        retval, amdtpsCb.core.timeoutTimer);
        amdtpsCb.core.timeoutTimer = BT_TIMER_HANDLE_INIT_VAL;
    }

#if defined(AMDTPS_RXONLY) || defined(AMDTPS_RX2TX)
    AMDTP_TRC("*** RECEIVED TOTAL %d ***\n", totalLen);
    totalLen = 0;
#endif
}

API_RESULT appl_amdtps_write_cback
           (
                GATT_DB_HANDLE  * handle,
                ATT_VALUE       * value
           )
{
    API_RESULT retval = API_SUCCESS;
    eAmdtpStatus_t status = AMDTP_STATUS_UNKNOWN_ERROR;
    amdtpPacket_t *pkt = NULL;

#if 0
    uint16_t i = 0;
    AMDTP_TRC("============= data arrived start ===============\n");
    for (i = 0; i < value->len; i++)
    {
        AMDTP_TRC("%x\t", value->val[i]);
    }
    AMDTP_TRC("============= data arrived end ===============\n");
#endif

    if (GATT_CHAR_AMDTP_RX == handle->char_id)
    {
#if defined(AMDTPS_RX2TX)
        amdtpsSendData(value->val, value->len);
#endif
#if defined(AMDTPS_RXONLY) || defined(AMDTPS_RX2TX)
        totalLen += value->len;
        AMDTP_TRC("received data len %d, total %d\n", value->len, totalLen);
        return API_SUCCESS;
#else /* RXONLY && RX2TX */
        status = AmdtpReceivePkt(&amdtpsCb.core, &amdtpsCb.core.rxPkt, value->len, value->val);
#endif
    }
    else if (GATT_CHAR_AMDTP_ACK == handle->char_id)
    {
        status = AmdtpReceivePkt(&amdtpsCb.core, &amdtpsCb.core.ackPkt, value->len, value->val);
    }

    if (status == AMDTP_STATUS_RECEIVE_DONE)
    {
        if (GATT_CHAR_AMDTP_RX == handle->char_id)
        {
            pkt = &amdtpsCb.core.rxPkt;
        }
        else if (GATT_CHAR_AMDTP_ACK == handle->char_id)
        {
            pkt = &amdtpsCb.core.ackPkt;
        }

        AmdtpPacketHandler(&amdtpsCb.core, (eAmdtpPktType_t)pkt->header.pktType, pkt->len - AMDTP_CRC_SIZE_IN_PKT, pkt->data);
    }

    return retval;
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
AmdtpsSendPacket(eAmdtpPktType_t type, BOOLEAN encrypted, BOOLEAN enableACK, uint8_t *buf, uint16_t len)
{
    //
    // Check if ready to send notification
    //
    if ( !amdtpsCb.txReady )
    {
        //set in callback amdtpsHandleValueCnf
        AMDTP_TRC("data sending failed, not ready for notification.\n", NULL);
        return AMDTP_STATUS_TX_NOT_READY;
    }

    //
    // Check if the service is idle to send
    //
    if ( amdtpsCb.core.txState != AMDTP_STATE_TX_IDLE )
    {
        AMDTP_TRC("data sending failed, tx state = %d\n", amdtpsCb.core.txState);
        return AMDTP_STATUS_BUSY;
    }

    //
    // Check if data length is valid
    //
    if ( len > AMDTP_MAX_PAYLOAD_SIZE )
    {
        AMDTP_TRC("data sending failed, exceed maximum payload, len = %d.\n", len);
        return AMDTP_STATUS_INVALID_PKT_LENGTH;
    }

    AmdtpBuildPkt(&amdtpsCb.core, type, encrypted, enableACK, buf, len);

    // send packet
    AmdtpSendPacketHandler(&amdtpsCb.core);

    return AMDTP_STATUS_SUCCESS;
}

#endif /* AMDTPS */

