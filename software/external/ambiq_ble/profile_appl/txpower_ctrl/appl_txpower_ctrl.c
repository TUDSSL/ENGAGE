//*****************************************************************************
//
//  appl_txpower_ctrl.c
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
 *  \file appl_txpower_ctrl.c
 *
 *  This is a stand-alone Bluetooth tx power control application.
 *
 *
 */

/*
 *  Copyright (C) 2018. AmbiqMicro Ltd.
 *  All rights reserved.
 */

/* ----------------------------------------- Header File Inclusion */
#include "BT_common.h"
#include "BT_hci_api.h"
#include "BT_att_api.h"
#include "BT_smp_api.h"
#include "smp_pl.h"
#include "l2cap.h"
#include "gatt_defines.h"

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#if AM_PART_APOLLO2
#include "hci_spi_internal.h"
#include "am_devices_em9304.h"
#include "hci_apollo_config.h"
#include "hci_drv_em9304.h"
#endif

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
#include "hci_drv_apollo3.h"
#endif
#include "BT_api.h"

/* ----------------------------------------- Configuration Defines */
/* Scan parameters */

/* Scan Type. 0x00 => Passive Scanning. 0x01 => Active Scanning. */
#define APPL_GAP_GET_SCAN_TYPE()               0x00

/* Scan Interval */
#define APPL_GAP_GET_SCAN_INTERVAL()           0x0040

/* Scan Window */
#define APPL_GAP_GET_SCAN_WINDOW()             0x0040

/* Own Address Type. 0x00 => Public. 0x01 => Random */
#define APPL_GAP_GET_OWN_ADDR_TYPE_IN_SCAN()   0x00

/* Scan Filter Policy. 0x00 => Accept All. 0x01 => Use White List. */
#define APPL_GAP_GET_SCAN_FILTER_POLICY()      0x00

#define APPL_GAP_GET_WHITE_LIST_SCAN_FILTER_POLICY()          0x01

#define APPL_GAP_GET_NON_WHITE_LIST_SCAN_FILTER_POLICY()      0x00


/* Connection Paramters */
/* Scan Interval */
#define APPL_GAP_GET_CONN_SCAN_INTERVAL()               0x0040

/* Scan Window */
#define APPL_GAP_GET_CONN_SCAN_WINDOW()                 0x0040

/**
 * Initiator Filter Policy.
 * 0x00 => Do not use White List.
 * 0x01 => Use White List.
 */
#define APPL_GAP_GET_CONN_INITIATOR_FILTER_POLICY()     0x00

/* Own Address Type. 0x00 => Public. 0x01 => Random */
#define APPL_GAP_GET_OWN_ADDR_TYPE_AS_INITIATOR()       0x00

/* Minimum value of connection event interval */
#define APPL_GAP_GET_CONN_INTERVAL_MIN()                0x96

/* Maximum value of connection event interval */
#define APPL_GAP_GET_CONN_INTERVAL_MAX()                0x96

/* Slave Latency */
#define APPL_GAP_GET_CONN_LATENCY()                     0x00

/* Link Supervision Timeout */
#define APPL_GAP_GET_CONN_SUPERVISION_TIMEOUT()         0x03E8

/* Informational Parameter */
#define APPL_GAP_GET_CONN_MIN_CE_LENGTH()               0x0000
#define APPL_GAP_GET_CONN_MAX_CE_LENGTH()               0xFFFF

/* Connection Update Parameters */

/* Minimum value of connection event interval for Connection Update */
#define APPL_GAP_GET_CONN_INTERVAL_MIN_FOR_CU()                0x20

/* Maximum value of connection event interval for Connection Update */
#define APPL_GAP_GET_CONN_INTERVAL_MAX_FOR_CU()                0x40

/* Slave Latency for Connection Update */
#define APPL_GAP_GET_CONN_LATENCY_FOR_CU()                     0x00

/* Link Supervision Timeout for Connection Update */
#define APPL_GAP_GET_CONN_SUPERVISION_TIMEOUT_FOR_CU()         0x0200

/* Informational Parameter for Connection Update */
#define APPL_GAP_GET_CONN_MIN_CE_LENGTH_FOR_CU()               0x0000
#define APPL_GAP_GET_CONN_MAX_CE_LENGTH_FOR_CU()               0xFFFF

/* ----------------------------------------- Macro Defines */
/* Encoding of application PDU parameters */
#define appl_pack_1_byte_param(dest, src) \
    *((dest) + 0) = (UCHAR)(*((UCHAR *)(src)));

#define appl_pack_2_byte_param(dest, src) \
    *((dest) + 0) = (UCHAR)(*((UINT16 *)(src))); \
    *((dest) + 1) = (UCHAR)(*((UINT16 *)(src)) >> 8);

#define appl_pack_3_byte_param(dest, src) \
    *((dest) + 0) = (UCHAR)(*((UINT32 *)(src)));\
    *((dest) + 1) = (UCHAR)(*((UINT32 *)(src)) >> 8);\
    *((dest) + 2) = (UCHAR)(*((UINT32 *)(src)) >> 16);

#define appl_pack_4_byte_param(dest, src) \
    *((dest) + 0) = (UCHAR)(*((UINT32 *)(src)));\
    *((dest) + 1) = (UCHAR)(*((UINT32 *)(src)) >> 8);\
    *((dest) + 2) = (UCHAR)(*((UINT32 *)(src)) >> 16);\
    *((dest) + 3) = (UCHAR)(*((UINT32 *)(src)) >> 24);

/* Decoding of application PDU parameters */
#define appl_unpack_1_byte_param(dest, src) \
        (dest) = (src)[0];

#define appl_unpack_2_byte_param(dest, src) \
        (dest)  = (src)[1]; \
        (dest)  = ((dest) << 8); \
        (dest) |= (src)[0];

#define appl_unpack_4_byte_param(dest, src) \
        (dest)  = (src)[3]; \
        (dest)  = ((dest) << 8); \
        (dest) |= (src)[2]; \
        (dest)  = ((dest) << 8); \
        (dest) |= (src)[1]; \
        (dest)  = ((dest) << 8); \
        (dest) |= (src)[0];

/* TBD: */
#define DEVICE_CONNECTED     0x00
#define CONNECTION_INITIATED 0x00
#define APPL_SET_STATE(mask) 0x00
#define APPL_GET_STATE(mask) 0x00

#ifndef DONT_USE_STANDARD_IO
#define APPL_TRC printf
#define APPL_ERR printf
#else
#define APPL_TRC(...)
#define APPL_ERR(...)
#endif /* DONT_USE_STANDARD_IO */

/* ----------------------------------------- External Global Variables */

/* TBD: Re-arrange function definition. Avoid explicit function declaration. */
void appl_poweron_stack(void);
void appl_master_menu_handler(void);
API_RESULT appl_bluetooth_on_complete_callback (void);
API_RESULT appl_hci_callback
           (
               UCHAR    event_type,
               UCHAR  * event_data,
               UCHAR    event_datalen
           );

void appl_dump_bytes (UCHAR *buffer, UINT16 length);
char * appl_hci_get_command_name (UINT16 opcode);
const char * appl_get_profile_menu (void);

/* ----------------------------------------- Exported Global Variables */


/* ----------------------------------------- Static Global Variables */
/* Application specific information used to register with ATT */


#define APPL_MAX_CHARACTERISTICS         16
#define APPL_UUID_INIT_VAL               0x0000
#define APPL_INVALID_CHAR_INDEX          0xFF
#define APPL_CHAR_COUNT_INIT_VAL         0x00

#define APPL_GD_INIT_STATE               0x00
#define APPL_GD_IN_SERV_DSCRVY_STATE     0x01
#define APPL_GD_IN_CHAR_DSCVRY_STATE     0x02
#define APPL_GD_IN_DESC_DSCVRY_STATE     0x03

#define APPL_CHAR_GET_START_HANDLE(i)\
        appl_char[(i)].start_handle

#define APPL_CHAR_GET_UUID(i)\
        appl_char[(i)].uuid

#define APPL_GET_SER_END_HANDLE()\
        appl_gatt_dscvry_state.service_end_handle

#define APPL_GET_CURRENT_DSCVRY_STATE()\
        appl_gatt_dscvry_state.state

#define APPL_CURRENT_SERVICE_UUID()\
        appl_gatt_dscvry_state.service_uuid

#define APPL_GET_CURRENT_CHAR_INDEX()\
        appl_gatt_dscvry_state.current_char_index

#define APPL_GET_CHAR_COUNT()\
        appl_gatt_dscvry_state.char_count

#define APPL_CHAR_INIT(i)\
        APPL_CHAR_GET_START_HANDLE(i) = ATT_INVALID_ATTR_HANDLE_VAL;\
        APPL_CHAR_GET_UUID(i) = APPL_UUID_INIT_VAL;

#define APPL_INIT_GATT_DSCRY_STATE()\
        APPL_GET_SER_END_HANDLE() = APPL_UUID_INIT_VAL;\
        APPL_GET_CURRENT_DSCVRY_STATE() = APPL_GD_INIT_STATE;\
        APPL_CURRENT_SERVICE_UUID() = APPL_UUID_INIT_VAL;\
        APPL_GET_CURRENT_CHAR_INDEX() = APPL_INVALID_CHAR_INDEX;\
        APPL_GET_CHAR_COUNT() = APPL_CHAR_COUNT_INIT_VAL;

/* DIS - Device Information Service */


#define APPL_CLI_CNFG_VAL_LENGTH    2

/*
 * Connection Complete Count.
 * Used to control the application scenario.
 */
DECL_STATIC UCHAR appl_connection_complete_count;


/* ------------------------------- Functions */


/* --------------------------------------------- Header File Inclusion */

//*****************************************************************************
//
// Global variables
//
//*****************************************************************************

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

/* --------------------------------------------- Static Global Variables */

/* --------------------------------------------- Functions */

TimerHandle_t xButtonTimer;

#if AM_PART_APOLLO2
txPowerLevel_t  tx_power_level = TX_POWER_LEVEL_PLUS_6P2_dBm;
#endif

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
txPowerLevel_t  tx_power_level = TX_POWER_LEVEL_PLUS_3P0_dBm;
#endif

uint32_t        dtm_in_progress = false;

//*****************************************************************************
//
// Poll the buttons.
//
//*****************************************************************************
void
button_timer_handler(TimerHandle_t xTimer)
{
    //
    // Every time we get a button timer tick, check all of our buttons.
    //
    am_devices_button_array_tick(am_bsp_psButtons, AM_BSP_NUM_BUTTONS);

    //
    // If we got a a press, do something with it.
    //
    if ( am_devices_button_released(am_bsp_psButtons[0]) )
    {
      am_util_debug_printf("Got Button 0 Press\n");
    }

    if ( am_devices_button_released(am_bsp_psButtons[1]) )
    {

#if AM_PART_APOLLO2
      HciVsEM_SetRfPowerLevelEx(tx_power_level);
      switch ( tx_power_level )
      {
        case TX_POWER_LEVEL_MINOR_33P5_dBm:
            am_util_debug_printf("Current Tx Power is -33.5 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_29P0_dBm:
            am_util_debug_printf("Current Tx Power is -29.0 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_17P9_dBm:
            am_util_debug_printf("Current Tx Power is -17.9 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_16P4_dBm:
            am_util_debug_printf("Current Tx Power is -16.4 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_14P6_dBm:
            am_util_debug_printf("Current Tx Power is -14.6 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_13P1_dBm:
            am_util_debug_printf("Current Tx Power is -13.1 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_11P4_dBm:
            am_util_debug_printf("Current Tx Power is -11.4 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_9P9_dBm:
            am_util_debug_printf("Current Tx Power is -9.9 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_8P4_dBm:
            am_util_debug_printf("Current Tx Power is -8.4 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_6P9_dBm:
            am_util_debug_printf("Current Tx Power is -6.9 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_5P5_dBm:
            am_util_debug_printf("Current Tx Power is -5.5 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_4P0_dBm:
            am_util_debug_printf("Current Tx Power is -4.0 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_2P6_dBm:
            am_util_debug_printf("Current Tx Power is -2.6 dBm\n");
            break;
        case TX_POWER_LEVEL_MINOR_1P4_dBm:
            am_util_debug_printf("Current Tx Power is -1.4 dBm\n");
            break;
        case TX_POWER_LEVEL_PLUS_0P4_dBm:
            am_util_debug_printf("Current Tx Power is +0.4 dBm\n");
            break;
        case TX_POWER_LEVEL_PLUS_2P5_dBm:
            am_util_debug_printf("Current Tx Power is +2.5 dBm\n");
            break;
        case TX_POWER_LEVEL_PLUS_4P6_dBm:
            am_util_debug_printf("Current Tx Power is +4.6 dBm\n");
            break;
        case TX_POWER_LEVEL_PLUS_6P2_dBm:
            am_util_debug_printf("Current Tx Power is +6.2 dBm\n");
            break;
        default:
            am_util_debug_printf("Invalid Tx power level\n");
            break;
      }

      if ( tx_power_level == 0 )
      {
        tx_power_level = TX_POWER_LEVEL_PLUS_6P2_dBm;
      }
      else
      {
        tx_power_level--;
      }
#endif

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    HciVsA3_SetRfPowerLevelEx(tx_power_level);

    switch ( tx_power_level )
    {
        case TX_POWER_LEVEL_MINUS_10P0_dBm:
            am_util_debug_printf("Current Tx Power is -10.0 dBm\n");
            tx_power_level = TX_POWER_LEVEL_0P0_dBm;
            break;
        case TX_POWER_LEVEL_0P0_dBm:
            am_util_debug_printf("Current Tx Power is 0.0 dBm\n");
            tx_power_level = TX_POWER_LEVEL_PLUS_3P0_dBm;
            break;
        case TX_POWER_LEVEL_PLUS_3P0_dBm:
            am_util_debug_printf("Current Tx Power is +3.0 dBm\n");
            tx_power_level = TX_POWER_LEVEL_MINUS_10P0_dBm;
            break;
        default:
            am_util_debug_printf("Invalid Tx power level\n");
            break;
    }
#endif

    }

    if ( am_devices_button_released(am_bsp_psButtons[2]) )
    {
        if ( dtm_in_progress )
        {
            BT_hci_le_test_end();
            dtm_in_progress = false;
            am_util_debug_printf("Transmitter test ended\n");
#if AM_PART_APOLLO2
            tx_power_level = TX_POWER_LEVEL_PLUS_6P2_dBm;
#endif
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
            tx_power_level = TX_POWER_LEVEL_PLUS_3P0_dBm;
#endif
        }
        else
        {
            // Resetting controller first.
            BT_hci_reset();

            BT_hci_le_transmitter_test_command(0, 255, 7);

            dtm_in_progress = true;
            am_util_debug_printf("Transmitter test started\n");
        }
    }
}

//*****************************************************************************
//
// Sets up a button interface.
//
//*****************************************************************************
void
setup_buttons(void)
{
    //
    // Enable the buttons for user interaction.
    //
    am_devices_button_array_init(am_bsp_psButtons, AM_BSP_NUM_BUTTONS);

    //
    // Start a timer.
    //

    // Create a FreeRTOS Timer
    xButtonTimer = xTimerCreate("Button Timer", pdMS_TO_TICKS(20),
            pdTRUE, NULL, button_timer_handler);

    if (xButtonTimer != NULL)
    {
        xTimerStart(xButtonTimer, 100);
    }
}


/**
 *  \brief To poweron the stack.
 *
 *  \par Description:
 *  This routine will turn on the Bluetooth service.
 */
void appl_poweron_stack(void)
{
    /* Initialize Connection Complete Count */
    appl_connection_complete_count = 0x00;

    /**
     *  Turn on the Bluetooth service.
     *
     *  appl_hci_callback - is the application callback function that is
     *  called when any HCI event is received by the stack
     *
     *  appl_bluetooth_on_complete_callback - is called by the stack when
     *  the Bluetooth service is successfully turned on
     *
     *  Note: After turning on the Bluetooth Service, stack will call
     *  the registered bluetooth on complete callback.
     *  This sample application will initiate Scanning to look for devices
     *  in vicinity [Step 2(a)] from bluetooth on complete callback
     *  'appl_bluetooth_on_complete_callback'
     */
    BT_bluetooth_on
    (
        appl_hci_callback,
        appl_bluetooth_on_complete_callback,
        "EtherMind-BPC"
    );

    return;
}


/**
 *  \brief Entry point of the sample application.
 */
int appl_init(void)
{
    /* Initialize SNOOP Related Functions */
    em_snoop_init();
    snoop_bt_init();

    /* Step 1. Initialize and power on the stack */
    BT_ethermind_init ();

    #ifdef BT_ANALYZER
        init_analyzer();
    #endif /* BT_ANALYZER */

#ifdef HCI_TX_RUN_TIME_SELECTION
#ifdef BT_USB
    BT_hci_register_tx_cb(hci_usb_send_data);
#endif /* BT_USB */

#ifdef BT_UART
    BT_hci_register_tx_cb(hci_uart_send_data);
#endif /* BT_UART */

#ifdef BT_SOCKET
    BT_hci_register_tx_cb(hci_socket_send_data);
#endif /* BT_SOCKET */
#endif /* HCI_TX_RUN_TIME_SELECTION */

    appl_poweron_stack();
    return 0;
}


/**
 *  \brief Bluetooth ON callback handler.
 *
 *  \par Description:
 *  This is the callback funtion called by the stack after
 *  completion of Bluetooth ON operation.
 *
 *  Perform following operation
 *  - Register Callback with L2CAP, ATT and SMP.
 *  - Initiate Scanning
 *
 *  \param None
 *
 *  \return API_SUCCESS
 */
API_RESULT appl_bluetooth_on_complete_callback (void)
{
    API_RESULT retval = 0;
    APPL_TRC (
    "Turned ON Bluetooth.\n");

    setup_buttons();
    return retval;
}


/* ------------------------------- HCI Callback Function */

/*
 *  This is a callback function registered with the HCI layer during
 *  bluetooth ON.
 *
 *  \param event_type: Type of HCI event.
 *  \param event_data: Parameters for the event 'event_type'.
 *  \param event_datalen: Length of the parameters for the event 'event_type'.
 *
 *  \return   : API_SUCEESS on successful processing of the event.
 *              API_FAILURE otherwise
 */
API_RESULT appl_hci_callback
           (
               UCHAR    event_type,
               UCHAR  * event_data,
               UCHAR    event_datalen
           )
{
    UINT32              count;
    API_RESULT          retval;
    UINT16 connection_handle, value_2;
    UCHAR  status, value_1;

    UCHAR    sub_event_code;
    UCHAR    num_reports;
    UCHAR    address_type;
    UCHAR    clock_accuracy;
    UCHAR    length_data;
    UCHAR    peer_addr_type;
    UCHAR    role;
    UCHAR    rssi;
    UCHAR  * address;
    UCHAR  * data;
    UCHAR  * le_feature;
    UCHAR  * peer_addr;
    UCHAR  * random_number;
    UINT16   conn_interval;
    UINT16   conn_latency;
    UINT16   encripted_diversifier;
    UINT16   supervision_timeout;

    /* Num completed packets event, can be ignored by the application */
    if (HCI_NUMBER_OF_COMPLETED_PACKETS_EVENT == event_type)
    {
        return API_SUCCESS;
    }

    APPL_TRC ("\n");

    /* Switch based on the Event Code */
    switch (event_type)
    {
        case HCI_DISCONNECTION_COMPLETE_EVENT:
            APPL_TRC (
            "Received HCI_DISCONNECTION_COMPLETE_EVENT.\n");

            /* Status */
            hci_unpack_1_byte_param(&status, event_data);
            APPL_TRC (
            "\tStatus: 0x%02X\n", status);
            event_data += 1;

            /* Connection Handle */
            hci_unpack_2_byte_param(&connection_handle, event_data);
            APPL_TRC (
            "\tConnection Handle: 0x%04X\n", connection_handle);
            event_data += 2;

            /* Reason */
            hci_unpack_1_byte_param(&value_1, event_data);
            APPL_TRC (
            "\tReason: 0x%02X\n", value_1);
            event_data += 1;

            break;

        case HCI_ENCRYPTION_CHANGE_EVENT:
            APPL_TRC (
            "Received HCI_ENCRYPTION_CHANGE_EVENT.\n");

            /* Status */
            hci_unpack_1_byte_param(&status, event_data);
            APPL_TRC (
            "\tStatus: 0x%02X\n", status);
            event_data += 1;

            /* Connection Handle */
            hci_unpack_2_byte_param(&connection_handle, event_data);
            APPL_TRC (
            "\tConnection Handle: 0x%04X\n", connection_handle);
            event_data += 2;

            /* Encryption Enable */
            hci_unpack_1_byte_param(&value_1, event_data);
            APPL_TRC (
            "\tEcnryption Enable: 0x%02X", value_1);
            switch (value_1)
            {
                case 0x00:
                    APPL_TRC (
                    " -> Encryption OFF\n");
                    break;
                case 0x01:
                    APPL_TRC (
                    " -> Encryption ON\n");
                    break;
                default:
                    APPL_TRC (
                    " -> ???\n");
                    break;
            }
            event_data += 1;

            break;

        case HCI_COMMAND_COMPLETE_EVENT:
            APPL_TRC (
            "Received HCI_COMMAND_COMPLETE_EVENT.\n");

            /* Number of Command Packets */
            hci_unpack_1_byte_param(&value_1, event_data);
            APPL_TRC (
            "\tNum Command Packets: %d (0x%02X)\n", value_1, value_1);
            event_data += 1;

            /* Command Opcode */
            hci_unpack_2_byte_param(&value_2, event_data);
            APPL_TRC (
            "\tCommand Opcode: 0x%04X -> %s\n",
            value_2, appl_hci_get_command_name(value_2));
            event_data += 2;

            /* Command Status */
            hci_unpack_1_byte_param(&status, event_data);
            APPL_TRC (
            "\tCommand Status: 0x%02X\n", status);
            event_data += 1;

            /* Check for HCI_LE_SET_ADVERTISING_DATA_OPCODE */
            if (HCI_LE_SET_ADVERTISING_DATA_OPCODE == value_2)
            {
                APPL_TRC (
                "Set Advertising Parameters.\n");
                APPL_TRC (
                "Status = 0x%02X\n", status);

                if (API_SUCCESS == status)
                {
#ifdef APPL_AUTO_ADVERTISE
#if ((defined APPL_GAP_BROADCASTER_SUPPORT) || (defined APPL_GAP_PERIPHERAL_SUPPORT))
                    appl_gap_set_adv_data_complete (APPL_GAP_GET_ADV_PARAM_ID);
#endif /* APPL_GAP_BROADCASTER_SUPPORT || APPL_GAP_PERIPHERAL_SUPPORT */
#endif /* APPL_AUTO_ADVERTISE */
                }
            }
            /* Check for HCI_LE_SET_ADVERTISING_PARAMETERS_OPCODE */
            else if (HCI_LE_SET_ADVERTISING_PARAMETERS_OPCODE == value_2)
            {
                APPL_TRC (
                "Enabling Advertising\n");
                APPL_TRC (
                "Status = 0x%02X\n", status);

                if (API_SUCCESS == status)
                {
#ifdef APPL_AUTO_ADVERTISE
#if ((defined APPL_GAP_BROADCASTER_SUPPORT) || (defined APPL_GAP_PERIPHERAL_SUPPORT))
                    appl_gap_set_adv_param_complete ();
#endif /* APPL_GAP_BROADCASTER_SUPPORT || APPL_GAP_PERIPHERAL_SUPPORT */
#endif /* APPL_AUTO_ADVERTISE */
                }
            }
            else if (HCI_LE_SET_ADVERTISING_ENABLE_OPCODE == value_2)
            {
                if (API_SUCCESS == status)
                {
                    APPL_TRC (
                    "Enabled Advertising...\n");
                    APPL_TRC (
                    "EtherMind Server is now Ready\n");
                }
            }
            /* Check for HCI_LE_SET_SCAN_PARAMETERS_OPCODE */
            else if (HCI_LE_SET_SCAN_PARAMETERS_OPCODE == value_2)
            {
                APPL_TRC (
                "Set Scan Parameters Complete.\n");
                APPL_TRC (
                "Status = 0x%02X\n", status);

            }
            else if (HCI_LE_SET_SCAN_ENABLE_OPCODE == value_2)
            {
                if (API_SUCCESS == status)
                {
                    APPL_TRC (
                    "Enabled Scanning ...\n");

                    /**
                     *  Once peer device starts advertising
                     *  local device will receive
                     *  'HCI_LE_ADVERTISING_REPORT_SUBEVENT'
                     *  and initiate connection.
                     */
                    APPL_TRC (
                    "Wait for the Advertising Events\n");
                }
            }

            /* Command Return Parameters */
            if (event_datalen > 4)
            {
                APPL_TRC (
                "\tReturn Parameters: ");
                for (count = 4; count < event_datalen; count ++)
                {
                    APPL_TRC (
                    "%02X ", *event_data);
                    event_data += 1;
                }
                APPL_TRC ("\n");
            }

            break;

        case HCI_COMMAND_STATUS_EVENT:
            APPL_TRC (
            "Received HCI_COMMAND_STATUS_EVENT.\n");

            /* Status */
            hci_unpack_1_byte_param(&status, event_data);
            APPL_TRC (
            "\tCommand Status: 0x%02X\n", status);
            event_data += 1;

            /* Number of Command Packets */
            hci_unpack_1_byte_param(&value_1, event_data);
            APPL_TRC (
            "\tNum Command Packets: %d (0x%02X)\n", value_1, value_1);
            event_data += 1;

            /* Command Opcode */
            hci_unpack_2_byte_param(&value_2, event_data);
            APPL_TRC (
            "\tCommand Opcode: 0x%04X (%s)\n",
            value_2, appl_hci_get_command_name(value_2));
            event_data += 2;

            break;

        case HCI_HARDWARE_ERROR_EVENT:
            APPL_TRC (
            "Received HCI_HARDWARE_ERROR_EVENT.\n");

            /* Hardware Code */
            hci_unpack_1_byte_param(&value_1, event_data);
            APPL_TRC (
            "\tHardware Code: 0x%02X\n", value_1);
            event_data += 1;

            break;

        case HCI_NUMBER_OF_COMPLETED_PACKETS_EVENT:
            APPL_TRC (
            "Received HCI_NUMBER_OF_COMPLETED_PACKETS_EVENT.\n");
            break;

        case HCI_DATA_BUFFER_OVERFLOW_EVENT:
            APPL_TRC (
            "Received HCI_DATA_BUFFER_OVERFLOW_EVENT.\n");

            /* Link Type */
            hci_unpack_1_byte_param(&value_1, event_data);
            APPL_TRC (
            "\tLink Type: 0x%02X", value_1);
            switch (value_1)
            {
                case 0x00:
                    APPL_TRC (
                    " -> Synchronous Buffer Overflow\n");
                    break;
                case 0x01:
                    APPL_TRC (
                    " -> ACL Buffer Overflow\n");
                    break;
                default:
                    APPL_TRC (
                    " -> ???\n");
                    break;
            }
            event_data += 1;

            break;

        case HCI_ENCRYPTION_KEY_REFRESH_COMPLETE_EVENT:
            APPL_TRC (
            "Received HCI_ENCRYPTION_KEY_REFRESH_COMPLETE_EVENT\n");

            /* Status */
            hci_unpack_1_byte_param(&status, event_data);
            APPL_TRC (
            "Status: 0x%02X\n", status);
            event_data ++;

            /* Connection Handle */
            hci_unpack_2_byte_param(&value_2, event_data);
            APPL_TRC (
            "\tConnection Handle: 0x%04X\n", value_2);

            break;

        case HCI_LE_META_EVENT:
            APPL_TRC (
            "Receaved HCI_LE_META_EVENT.\n");
            hci_unpack_1_byte_param(&sub_event_code, event_data);
            event_data = event_data + 1;
            switch ( sub_event_code )
            {
                case HCI_LE_CONNECTION_COMPLETE_SUBEVENT:
                    APPL_TRC (
                    "Subevent : HCI_LE_CONNECTION_COMPLETE_SUBEVENT.\n");
                    hci_unpack_1_byte_param(&status, event_data + 0);
                    hci_unpack_2_byte_param(&connection_handle, event_data + 1);
                    hci_unpack_1_byte_param(&role, event_data + 3);
                    hci_unpack_1_byte_param(&peer_addr_type, event_data + 4);
                    peer_addr = 5 + event_data;
                    hci_unpack_2_byte_param(&conn_interval, event_data + 11);
                    hci_unpack_2_byte_param(&conn_latency, event_data + 13);
                    hci_unpack_2_byte_param(&supervision_timeout, event_data + 15);
                    hci_unpack_1_byte_param(&clock_accuracy, event_data + 17);

                    /* Print the parameters */
                    APPL_TRC (
                    "status = 0x%02X\n", status);
                    APPL_TRC (
                    "connection_handle = 0x%04X\n", connection_handle);
                    APPL_TRC (
                    "role = 0x%02X\n", role);
                    APPL_TRC (
                    "peer_addr_type = 0x%02X\n", peer_addr_type);
                    APPL_TRC (
                    "peer_addr = \n");
                    appl_dump_bytes(peer_addr, 6);
                    APPL_TRC (
                    "conn_interval = 0x%04X\n", conn_interval);
                    APPL_TRC (
                    "conn_latency = 0x%04X\n", conn_latency);
                    APPL_TRC (
                    "supervision_timeout = 0x%04X\n", supervision_timeout);
                    APPL_TRC (
                    "clock_accuracy = 0x%02X\n", clock_accuracy);

                    clock_accuracy = clock_accuracy;
                    /* Save connection complete count */
                    appl_connection_complete_count ++;

                    /* If connection is successful, initiate bonding [Step 2(c)] */
                    if (0x00 == status)
                    {
                        SMP_AUTH_INFO auth;
                        SMP_BD_ADDR   smp_peer_bd_addr;
                        SMP_BD_HANDLE smp_bd_handle;

                        auth.param = 1;
                        auth.bonding = 1;
                        auth.ekey_size = 12;
                        auth.security = SMP_SEC_LEVEL_1;
                        BT_COPY_BD_ADDR(smp_peer_bd_addr.addr, peer_addr);
                        BT_COPY_TYPE(smp_peer_bd_addr.type, peer_addr_type);

                        retval = BT_smp_get_bd_handle
                                 (
                                     &smp_peer_bd_addr,
                                     &smp_bd_handle
                                 );

                        if (API_SUCCESS == retval)
                        {
                            retval = BT_smp_authenticate (&smp_bd_handle, &auth);
                        }

                        if (API_SUCCESS != retval)
                        {
                            APPL_TRC (
                            "Initiation of Authentication Failed. Reason 0x%04X\n",
                            retval);
                        }

                        /**
                         *  Application will receive authentication complete event,
                         *  in SMP Callback.
                         *
                         *  Look for 'SMP_AUTHENTICATION_COMPLETE' event handling in
                         *  'appl_smp_callback'.
                         */
                    }

                    break;

                case HCI_LE_ADVERTISING_REPORT_SUBEVENT:
                    APPL_TRC("Subevent : HCI_LE_ADVERTISING_REPORT_SUBEVENT.\n");
                    /* Number of Responses */
                    hci_unpack_1_byte_param(&num_reports, event_data);
                    APPL_TRC("num_reports = 0x%02X\n", num_reports);
                    event_data += 1;

                    /* For each Response, Print the Inquiry Result */
                    for (count = 0; count < num_reports; count ++)
                    {
                        hci_unpack_1_byte_param(&event_type, event_data);
                        event_data += 1;
                        hci_unpack_1_byte_param(&address_type, event_data);
                        event_data += 1;
                        address = event_data;
                        event_data += 6;
                        hci_unpack_1_byte_param(&length_data, event_data);
                        event_data += 1;
                        data = event_data;
                        event_data += length_data;
                        hci_unpack_1_byte_param(&rssi, event_data);
                        event_data += 1;

                        /* Print the parameters */
                        APPL_TRC("event_type = 0x%02X\n", event_type);
                        APPL_TRC("address_type = 0x%02X\n", address_type);
                        APPL_TRC("address = \n");
                        appl_dump_bytes(address, 6);
                        APPL_TRC("length_data = 0x%02X\n", length_data);
                        APPL_TRC("data = \n");
                        appl_dump_bytes(data, length_data);
                        APPL_TRC("rssi = 0x%02X\n", rssi);
                        address_type = address_type;
                        rssi = rssi;
                        role = role;
                    }

                    /* If already connection initiated, do not try to initiate again */
                    if (0 != APPL_GET_STATE(CONNECTION_INITIATED))
                    {
                        break;
                    }
                    /* Stop Scanning */
                    BT_hci_le_set_scan_enable (0, 1);


                    break;

                case HCI_LE_CONNECTION_UPDATE_COMPLETE_SUBEVENT:
                    APPL_TRC (
                    "Subevent : HCI_LE_CONNECTION_UPDATE_COMPLETE_SUBEVENT.\n");
                    hci_unpack_1_byte_param(&status, event_data + 0);
                    hci_unpack_2_byte_param(&connection_handle, event_data + 1);
                    hci_unpack_2_byte_param(&conn_interval, event_data + 3);
                    hci_unpack_2_byte_param(&conn_latency, event_data + 5);
                    hci_unpack_2_byte_param(&supervision_timeout, event_data + 7);

                    /* Print the parameters */
                    APPL_TRC (
                    "status = 0x%02X\n", status);
                    APPL_TRC (
                    "connection_handle = 0x%04X\n", connection_handle);
                    APPL_TRC (
                    "conn_interval = 0x%04X\n", conn_interval);
                    APPL_TRC (
                    "conn_latency = 0x%04X\n", conn_latency);
                    APPL_TRC (
                    "supervision_timeout = 0x%04X\n", supervision_timeout);

                    break;

                case HCI_LE_READ_REMOTE_USED_FEATURES_COMPLETE_SUBEVENT:
                    APPL_TRC (
                    "Subevent: HCI_LE_READ_REMOTE_USED_FEATURES_COMPLETE_SUBEVENT.\n");

                    hci_unpack_1_byte_param(&status, event_data + 0);
                    hci_unpack_2_byte_param(&connection_handle, event_data + 1);
                    le_feature = 3 + event_data;

                    /* Print the parameters */
                    APPL_TRC (
                    "status = 0x%02X\n", status);
                    APPL_TRC (
                    "connection_handle = 0x%04X\n", connection_handle);
                    APPL_TRC (
                    "le_feature = \n");
                    appl_dump_bytes(le_feature, 8);
                    break;

                case HCI_LE_LONG_TERM_KEY_REQUESTED_SUBEVENT:
                    APPL_TRC("Subevent : HCI_LE_LONG_TERM_KEY_REQUESTED_SUBEVENT.\n");
                    hci_unpack_2_byte_param(&connection_handle, event_data + 0);
                    random_number = 2 + event_data;
                    hci_unpack_2_byte_param(&encripted_diversifier, event_data + 10);

                    /* Print the parameters */
                    APPL_TRC (
                    "connection_handle = 0x%04X\n", connection_handle);
                    APPL_TRC (
                    "random_number = \n");
                    appl_dump_bytes(random_number, 8);
                    APPL_TRC (
                    "encrypted_diversifier = 0x%04X\n", encripted_diversifier);

                    break;

                default:
                    APPL_ERR (
                    "Unknown/Unhandled LE SubEvent Code 0x%02X Received.\n",
                    sub_event_code);
                    break;
            } // switch ( sub_event_code )
            break;

        default:
            APPL_ERR (
            "Unknown/Unhandled Event Code 0x%02X Received.\n", event_type);
            break;
    } // switch (event_type)

    return API_SUCCESS;
}

/* ------------------------------- Utility Functions */
void appl_dump_bytes (UCHAR *buffer, UINT16 length)
{
    char hex_stream[49];
    char char_stream[17];
    UINT32 i;
    UINT16 offset, count;
    UCHAR c;

    APPL_TRC ("\n");
    APPL_TRC (
    "-- Dumping %d Bytes --\n",
    (int)length);

    APPL_TRC (
    "-------------------------------------------------------------------\n");

    count = 0;
    offset = 0;
    for (i = 0; i < length; i ++)
    {
        c =  buffer[i];
        sprintf(hex_stream + offset, "%02X ", c);

        if ( (c >= 0x20) && (c <= 0x7E) )
        {
            char_stream[count] = c;
        }
        else
        {
            char_stream[count] = '.';
        }

        count ++;
        offset += 3;

        if ( 16 == count )
        {
            char_stream[count] = '\0';
            count = 0;
            offset = 0;

            APPL_TRC("%s   %s\n",
            hex_stream, char_stream);

            BT_mem_set(hex_stream, 0, 49);
            BT_mem_set(char_stream, 0, 17);
        }
    }

    if ( offset != 0 )
    {
        char_stream[count] = '\0';

        /* Maintain the alignment */
        APPL_TRC (
        "%-48s   %s\n",
        hex_stream, char_stream);
    }

    APPL_TRC (
    "-------------------------------------------------------------------\n");

    APPL_TRC ("\n");

    return;
}

/* ------------------------------- Dummy Functions */
#include "fsm_defines.h"
DECL_CONST DECL_CONST_QUALIFIER STATE_EVENT_TABLE appl_state_event_table[] =
{
    {
        /*0*/ 0x00,
        0,
        0
    }
};

DECL_CONST DECL_CONST_QUALIFIER EVENT_HANDLER_TABLE appl_event_handler_table[] =
{
    {
        /*0*/ 0x0000,
        NULL
    }
};

API_RESULT appl_access_state_handler
           (
                void       * param,
                STATE_T    * state
           )
{
    return API_SUCCESS;
}

char * appl_hci_get_command_name (UINT16 opcode)
{
    return "DUMMY";
}


void appl_bluetooth_on(void)
{
     /**
     *  Turn on the Bluetooth service.
     *
     *  appl_hci_callback - is the application callback function that is
     *  called when any HCI event is received by the stack
     *
     *  appl_bluetooth_on_complete_callback - is called by the stack when
     *  the Bluetooth service is successfully turned on
     *
     *  Note: After turning on the Bluetooth Service, stack will call
     *  the registered bluetooth on complete callback.
     *  This sample application will initiate Scanning to look for devices
     *  in vicinity [Step 2(a)] from bluetooth on complete callback
     *  'appl_bluetooth_on_complete_callback'
     */
    BT_bluetooth_on
    (
        appl_hci_callback,
        appl_bluetooth_on_complete_callback,
        "EtherMind-BPC"
    );

}

void appl_bluetooth_off(void)
{
    BT_bluetooth_off ();
}

void appl_set_scan_parameters(UCHAR read)
{
    UCHAR scan_filter_policy;
    API_RESULT retval;

    scan_filter_policy = (UCHAR)((read)?
                         APPL_GAP_GET_WHITE_LIST_SCAN_FILTER_POLICY():
                         APPL_GAP_GET_NON_WHITE_LIST_SCAN_FILTER_POLICY());

    /**
    *  Step 2(a). Initiate Scanning.
    *
    *  The operation is performed in two parts.
    *  First set the scan parameters.
    *  On completion of set scan parameter, enable scanning.
    *
    *  Completion of set scan parameter is informed through
    *  'HCI_COMMAND_COMPLETE_EVENT' in HCI Callback 'appl_hci_callback'.
    *  Look for the handling of opcode
    *  'HCI_LE_SET_SCAN_PARAMETERS_OPCODE'.
    */
    retval = BT_hci_le_set_scan_parameters
             (
                 APPL_GAP_GET_SCAN_TYPE(),
                 APPL_GAP_GET_SCAN_INTERVAL(),
                 APPL_GAP_GET_SCAN_WINDOW(),
                 APPL_GAP_GET_OWN_ADDR_TYPE_IN_SCAN(),
                 scan_filter_policy
             );

    if (API_SUCCESS != retval)
    {
        APPL_ERR (
        "Failed to set scan parameters. Result = 0x%04X\n", retval);
    }
}

void appl_set_scan_mode(UCHAR mode)
{
    BT_hci_le_set_scan_enable (mode, 1);
}

