//*****************************************************************************
//
//! @file appl_ancs.c
//!
//! @brief ANCS service.
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

/**
 *  \file appl_ancs.c
 *
 */

/* --------------------------------------------- Header File Inclusion */
#ifdef ANCS
#include "appl.h"
#include "BT_common.h"
#include "BT_hci_api.h"
#include "BT_att_api.h"
#include "BT_smp_api.h"
#include "smp_pl.h"
#include "l2cap.h"
#include "fsm_defines.h"
#include "task.h"
#include "queue.h"

/* ----------------------------------------- Configuration Defines */
extern uint32_t am_util_stdio_printf(const char *pcFmt, ...);
#undef APPL_TRC
#undef APPL_ERR
#define APPL_TRC am_util_stdio_printf
#define APPL_ERR am_util_stdio_printf


/* ----------------------------------------- Macro Defines */
/**@brief Category IDs for iOS notifications. */
typedef enum
{
    BLE_ANCS_CATEGORY_ID_OTHER,                     /**< The iOS notification belongs to the "other" category.  */
    BLE_ANCS_CATEGORY_ID_INCOMING_CALL,             /**< The iOS notification belongs to the "Incoming Call" category. */
    BLE_ANCS_CATEGORY_ID_MISSED_CALL,               /**< The iOS notification belongs to the "Missed Call" category. */
    BLE_ANCS_CATEGORY_ID_VOICE_MAIL,                /**< The iOS notification belongs to the "Voice Mail" category. */
    BLE_ANCS_CATEGORY_ID_SOCIAL,                    /**< The iOS notification belongs to the "Social" category. */
    BLE_ANCS_CATEGORY_ID_SCHEDULE,                  /**< The iOS notification belongs to the "Schedule" category. */
    BLE_ANCS_CATEGORY_ID_EMAIL,                     /**< The iOS notification belongs to the "E-mail" category. */
    BLE_ANCS_CATEGORY_ID_NEWS,                      /**< The iOS notification belongs to the "News" category. */
    BLE_ANCS_CATEGORY_ID_HEALTH_AND_FITNESS,        /**< The iOS notification belongs to the "Health and Fitness" category. */
    BLE_ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE,      /**< The iOS notification belongs to the "Buisness and Finance" category. */
    BLE_ANCS_CATEGORY_ID_LOCATION,                  /**< The iOS notification belongs to the "Location" category. */
    BLE_ANCS_CATEGORY_ID_ENTERTAINMENT              /**< The iOS notification belongs to the "Entertainment" category. */
} ancc_category_id_values_t;

/**@brief Event IDs for iOS notifications. */
typedef enum
{
    BLE_ANCS_EVENT_ID_NOTIFICATION_ADDED,           /**< The iOS notification was added. */
    BLE_ANCS_EVENT_ID_NOTIFICATION_MODIFIED,        /**< The iOS notification was modified. */
    BLE_ANCS_EVENT_ID_NOTIFICATION_REMOVED          /**< The iOS notification was removed. */
} ancc_evt_id_values_t;

/**@brief Control point command IDs that the Notification Consumer can send to the Notification Provider. */
typedef enum
{
    BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES,       /**< Requests attributes to be sent from the NP to the NC for a given notification. */
    BLE_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES,         /**< Requests attributes to be sent from the NP to the NC for a given iOS App. */
    BLE_ANCS_COMMAND_ID_GET_PERFORM_NOTIF_ACTION,   /**< Requests an action to be performed on a given notification, for example dismiss an alarm. */
} ancc_command_id_values_t;

/**@brief IDs for iOS notification attributes. */
typedef enum
{
    BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,          /**< Identifies that the attribute data is of an "App Identifier" type. */
    BLE_ANCS_NOTIF_ATTR_ID_TITLE,                   /**< Identifies that the attribute data is a "Title". Needs to be followed by a 2-bytes max length parameter*/
    BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE,                /**< Identifies that the attribute data is a "Subtitle". Needs to be followed by a 2-bytes max length parameter*/
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE,                 /**< Identifies that the attribute data is a "Message". Needs to be followed by a 2-bytes max length parameter*/
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE_SIZE,            /**< Identifies that the attribute data is a "Message Size". */
    BLE_ANCS_NOTIF_ATTR_ID_DATE,                    /**< Identifies that the attribute data is a "Date". */
    BLE_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL,   /**< The notification has a "Positive action" that can be executed associated with it. */
    BLE_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL,   /**< The notification has a "Negative action" that can be executed associated with it. */
} ancc_notif_attr_id_values_t;

/**@brief Typedef for iOS notifications. */
typedef struct
{
    uint8_t event_id;           /**< This field informs the accessory whether the given iOS notification was added, modified, or removed. The enumerated values for this field are defined in EventID Values.. */
    uint8_t event_flags;        /**< A bitmask whose set bits inform an NC of specificities with the iOS notification. */
    uint8_t category_id;        /**< A numerical value providing a category in which the iOS notification can be classified. */
    uint8_t category_count;     /**< The current number of active iOS notifications in the given category. */
    uint32_t notification_uid;  /**< A 32-bit numerical value that is the unique identifier (UID) for the iOS notification. */
} ancc_notif_t;

typedef enum
{
    ANCS_DISCONNECTED,
    ANCS_CONNECTED,
    ANCS_BONDED,
    ANCS_DISCOVERED,
} ancs_appl_state_t;

/* ----------------------------------------- External Global Variables */
void appl_profile_operations (void);
void AncsGetNotificationAttribute(uint32_t notiUid);
extern BT_DEVICE_ADDR g_bd_addr;

/* ----------------------------------------- Exported Global Variables */
QueueHandle_t xNtfUIDQueue;

/* ----------------------------------------- Static Global Variables */
DECL_STATIC UCHAR write_response = 0;
DECL_STATIC API_RESULT gResult = 0;
DECL_STATIC ancs_appl_state_t appl_ancs_state = ANCS_DISCONNECTED;
DECL_STATIC ATT_UUID ancs_service_uuid128 = {.uuid_128.value = {0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79}};
DECL_STATIC ATT_UUID ANCS_ControlPoint_UUID = {.uuid_128.value = {0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98, 0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69}};
DECL_STATIC ATT_UUID ANCS_NotificationSource_UUID = {.uuid_128.value = {0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C, 0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F}};
DECL_STATIC ATT_UUID ANCS_DataSource_UUID = {.uuid_128.value = {0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE, 0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22}};

/* BLE Connection Handle */
//DECL_STATIC UINT16 appl_ble_connection_handle;

/** ANCS Characteristic related information */
typedef struct
{
    /* ANCS Service */

    /* Notification Source */
    ATT_ATTR_HANDLE ancs_notification_source_hdl;

    /* Notification Source - CCC */
    ATT_ATTR_HANDLE ancs_notification_source_ccc_hdl;

    /* Control Point */
    ATT_ATTR_HANDLE ancs_control_point_hdl;

    /* Data Source */
    ATT_ATTR_HANDLE ancs_data_source_hdl;

    /* Data Source - CCC */
    ATT_ATTR_HANDLE ancs_data_source_ccc_hdl;

} ANCS_CHAR_INFO;

#define APPL_CLI_CNFG_VAL_LENGTH    2
DECL_STATIC ANCS_CHAR_INFO ancs_char_info;

#ifdef APPL_MENU_OPS
BT_DEVICE_ADDR g_peer_bd_addr;

static UCHAR ancs_client_menu[] =
"\n\
\r\n\
   1.  Bond with peer \r\n\
\r\n\
   2.  Discover ANCS \r\n\
\r\n\
   3.  Set Data Source CCCD \r\n\
\r\n\
   4.  Set Notification Source CCCD \r\n\
 \r\n\
   5.  Get Notification Attribute by UID \r\n\
\r\n\
   Your Option? \0";
#endif /* APPL_MENU_OPS */

/* --------------------------------------------- Constants */


/* --------------------------------------------- Static Global Variables */


/* --------------------------------------------- Functions */

void appl_bond_with_peer(void)
{
    API_RESULT retval;
    /* If connection is successful, initiate bonding [Step 2(c)] */

    SMP_AUTH_INFO auth;
    SMP_BD_ADDR   smp_peer_bd_addr;
    SMP_BD_HANDLE smp_bd_handle;

    APPL_TRC("\n<<appl_bond_with_peer>>\n");

    auth.param = 1;
    auth.bonding = 1;
    auth.ekey_size = 12;
    auth.security = SMP_SEC_LEVEL_1;

    BT_COPY_BD_ADDR(smp_peer_bd_addr.addr, g_bd_addr.addr);
    BT_COPY_TYPE(smp_peer_bd_addr.type, g_bd_addr.type);

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

void appl_ancs_init(void)
{
    appl_ancs_state = ANCS_DISCONNECTED;
    APPL_TRC("\n\n<<appl_ancs_init>>\n\n");
}

void appl_ancs_connect(APPL_HANDLE  * appl_handle)
{
    APPL_STATE_T state = GET_APPL_STATE(*appl_handle);

    if ( state == SL_0_TRANSPORT_OPERATING  &&  appl_ancs_state == ANCS_DISCONNECTED )
    {
        appl_ancs_state = ANCS_CONNECTED;
        APPL_TRC("\n\n<<CONNECTED>>\n\n");
    }
    else if ( state == SL_0_TRANSPORT_OPERATING &&  appl_ancs_state == ANCS_DISCOVERED )
    {
        appl_ancs_state = ANCS_BONDED;
        APPL_TRC("\n\n<<BONDED>>\n\n");
    }
    else
    {
        appl_ancs_state = ANCS_DISCONNECTED;
        APPL_TRC("\n\n<<DISCONNECTED>>\n\n");
    }
}

void appl_search_complete(void)
{
    appl_ancs_state = ANCS_DISCOVERED;
    APPL_TRC("\n\n<<DISCOVERED>>\n\n");
}

void appl_recvice_att_event(UCHAR att_event, API_RESULT event_result)
{
    if ( att_event == ATT_WRITE_RSP )
    {
        write_response = 1;
        gResult = 0;
    }
    else if ( att_event == ATT_ERROR_RSP )
    {
        gResult = event_result;
    }
}

void appl_manage_trasnfer (GATT_DB_HANDLE handle, UINT16 config)
{
    APPL_TRC("\n\n<<appl_manage_trasnfer>>\n\n");
}

void appl_send_ancs_measurement (APPL_HANDLE   * handle)
{
    APPL_TRC("\n\n<<appl_send_ancs_measurement>>\n\n");
}


void appl_timer_expiry_handler (void *data, UINT16 datalen)
{
    APPL_TRC("\n\n<<appl_timer_expiry_handler>>\n\n");
}

void appl_ancs_server_reinitialize (void)
{
    appl_ancs_state = ANCS_DISCONNECTED;
    APPL_TRC("\n\n<<appl_ancs_server_reinitialize>>\n\n");
}


void ancs_mtu_update(APPL_HANDLE    * appl_handle, UINT16 t_mtu)
{
    UINT16 mtu = 0;

    BT_att_access_mtu(&APPL_GET_ATT_INSTANCE(*appl_handle),
        &mtu);

    APPL_TRC("appl_handle 0x%x t_mtu = %d %d\n", *appl_handle, t_mtu, mtu);
}


/* ------------------------------- ATT related Functions */
void appl_rcv_service_desc (UINT16 config, ATT_UUID uuid, UINT16 value_handle)
{
    /* Populate Needed CCCDs here */
    if (GATT_CLIENT_CONFIG == config)
    {
        if ( memcmp(&uuid, &ANCS_NotificationSource_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
        {
            ancs_char_info.ancs_notification_source_ccc_hdl = value_handle;
        }
        else if ( memcmp(&uuid, &ANCS_DataSource_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
        {
            ancs_char_info.ancs_data_source_ccc_hdl = value_handle;
        }
    }
}

void appl_rcv_service_char (ATT_UUID uuid, UINT16 value_handle)
{
    if ( memcmp(&uuid, &ANCS_NotificationSource_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
    {
        ancs_char_info.ancs_notification_source_hdl = value_handle;
    }
    else if ( memcmp(&uuid, &ANCS_ControlPoint_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
    {
        ancs_char_info.ancs_control_point_hdl = value_handle;
    }
    else if ( memcmp(&uuid, &ANCS_DataSource_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
    {
        ancs_char_info.ancs_data_source_hdl = value_handle;
    }
}

void Wait4WrtRsp(void)
{
    const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
    UCHAR i = 0;

    while ( !write_response )
    {
        vTaskDelay( xDelay );
        if (i++ > 10 )
        {
            break;
        }
    }
    write_response = 0;
}

void appl_discover_ANCS(void)
{
    APPL_TRC("\n<<appl_discover_ANCS>>\n");
    appl_discover_service
    (
        ancs_service_uuid128,
        ATT_128_BIT_UUID_FORMAT
    );

    return;
}

void AncsSubscribeNotifications(void)
{
    ATT_VALUE    att_value;
    UINT16       cli_cfg;
    UCHAR        cfg_val[APPL_CLI_CNFG_VAL_LENGTH];

    APPL_TRC("\n++<<AncsSubscribeNotifications>>\n");

    cli_cfg = GATT_CLI_CNFG_NOTIFICATION;
    BT_PACK_LE_2_BYTE(cfg_val, &cli_cfg);
    att_value.len = APPL_CLI_CNFG_VAL_LENGTH;
    att_value.val = cfg_val;
    appl_write_req(ancs_char_info.ancs_data_source_ccc_hdl, &att_value);

    Wait4WrtRsp();

    cli_cfg = GATT_CLI_CNFG_NOTIFICATION;
    BT_PACK_LE_2_BYTE(cfg_val, &cli_cfg);
    att_value.len = APPL_CLI_CNFG_VAL_LENGTH;
    att_value.val = cfg_val;
    appl_write_req(ancs_char_info.ancs_notification_source_ccc_hdl, &att_value);

    Wait4WrtRsp();

    APPL_TRC("\n--<<AncsSubscribeNotifications(0x%04X)>>\n", gResult);
}

void appl_profile_operations (void)
{

    const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
    UINT32 NtfUID;
    write_response = 0;
    xNtfUIDQueue = 0;
    xNtfUIDQueue = xQueueCreate( 10, sizeof(UINT32) );

    fsm_post_event
    (
        APPL_FSM_ID,
        ev_appl_device_init_req,
        NULL
    );

    while ( appl_ancs_state != ANCS_CONNECTED)
    {
        vTaskDelay( xDelay );
    }

    appl_discover_ANCS();

    while ( appl_ancs_state != ANCS_DISCOVERED )
    {
        vTaskDelay( xDelay );
    }

    AncsSubscribeNotifications();

    if ( gResult == 0x0305 /* insufficient authentication */    ||
         gResult == 0x030F /* insufficient Encryption */ )
    {
        appl_bond_with_peer();
        while ( appl_ancs_state != ANCS_BONDED )
        {
            vTaskDelay( xDelay );
        }
        AncsSubscribeNotifications();
    }
    else
    {
        appl_ancs_state = ANCS_BONDED;
    }

    while ( appl_ancs_state != ANCS_DISCONNECTED )
    {
        if ( xQueueReceive( xNtfUIDQueue, &( NtfUID ), ( TickType_t ) 10 ) )
        {
            AncsGetNotificationAttribute(NtfUID);
        }
    }

    if ( xNtfUIDQueue != 0 )
    {
        vQueueDelete( xNtfUIDQueue);
    }

}

void AncsDumpBytes (UCHAR *buffer, UINT16 length)
{
    char hex_stream[49];
    char char_stream[17];
    UINT32 i;
    UINT16 offset, count;
    UCHAR c;

    APPL_TRC (
    "-------------------------------------------------------------------\n");

    count = 0;
    offset = 0;
    for ( i = 0; i < length; i++ )
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

            APPL_TRC ("%s   %s\n",
            hex_stream, char_stream);

            BT_mem_set(hex_stream, 0, 49);
            BT_mem_set(char_stream, 0, 17);
        }
    }

    if ( offset != 0 )
    {
        char_stream[count] = '\0';

        /* Maintain the alignment */
        APPL_TRC ("%-48s   %s\n",
        hex_stream, char_stream);
    }

    APPL_TRC (
    "-------------------------------------------------------------------\n\n");

    return;
}

void AncsGetNotificationAttribute(uint32_t notiUid)
{
    API_RESULT retval;
    ATT_VALUE    att_value;
    // An example to get notification attributes
    uint16_t max_len = 256;
    uint8_t buf[19];   // retrieve the complete attribute list

    APPL_TRC("++<<GetNtfUID:%X>>\n", notiUid);

    buf[0] = BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES;  // put command
    uint8_t *p = &buf[1];
    BT_UNPACK_LE_4_BYTE(p, (uint8_t*)(&notiUid));    // encode notification uid

    // encode attribute IDs
    buf[5] = BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER;
    buf[6] = BLE_ANCS_NOTIF_ATTR_ID_TITLE;
    // 2 byte length
    buf[7] = max_len;
    buf[8] = max_len >> 8;
    buf[9] = BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE;
    buf[10] = max_len;
    buf[11] = max_len >> 8;
    buf[12] = BLE_ANCS_NOTIF_ATTR_ID_MESSAGE;
    buf[13] = max_len;
    buf[14] = max_len >> 8;
    buf[15] = BLE_ANCS_NOTIF_ATTR_ID_MESSAGE_SIZE;
    buf[16] = BLE_ANCS_NOTIF_ATTR_ID_DATE;
    buf[17] = BLE_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL;
    buf[18] = BLE_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL;

    att_value.len = 19;
    att_value.val = buf;
    retval = appl_write_req(ancs_char_info.ancs_control_point_hdl, &att_value);

    //APPL_TRC ("retval 0x%04X\n", retval);
    if ( retval == API_SUCCESS )
    {
        Wait4WrtRsp();
    }
    APPL_TRC("--<<GetNtfUID:%X>>\n", notiUid);

}

void AncsNotificationCb(UCHAR *event_data, UINT16 event_datalen)
{
    ATT_ATTR_HANDLE  attr_handle;
    ancc_notif_t *NotificationSource;

    BT_UNPACK_LE_2_BYTE(&attr_handle, event_data);

    if ( attr_handle == ancs_char_info.ancs_notification_source_hdl )
    {
        NotificationSource = (ancc_notif_t *)(event_data + 2);

        switch(NotificationSource->category_id)
        {
            case(BLE_ANCS_CATEGORY_ID_OTHER):
                APPL_TRC("OTHER(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_INCOMING_CALL):
                APPL_TRC("INCOMING_CALL(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_MISSED_CALL):
                APPL_TRC("MISSED_CALL(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_VOICE_MAIL):
                APPL_TRC("VOICE_MAIL(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_SOCIAL):
                APPL_TRC("SOCIAL(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_SCHEDULE):
                APPL_TRC("SCHEDULE(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_EMAIL):
                APPL_TRC("EMAIL(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_NEWS):
                APPL_TRC("NEWS(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_HEALTH_AND_FITNESS):
                APPL_TRC("HEALTH_AND_FITNESS(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE):
                APPL_TRC("BUSINESS_AND_FINANCE(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_LOCATION):
                APPL_TRC("LOCATION(%X)\n", NotificationSource->notification_uid);
            break;

            case(BLE_ANCS_CATEGORY_ID_ENTERTAINMENT):
                APPL_TRC("ENTERTAINMENT(%X)\n", NotificationSource->notification_uid);
            break;

            default:
            break;
        }
        //APPL_TRC("Spaces %d\n", uxQueueSpacesAvailable(xNtfUIDQueue));
        xQueueSend( xNtfUIDQueue, ( void * ) &(NotificationSource->notification_uid), ( TickType_t ) 0 );
    }
#if 0
    else if ( attr_handle == ancs_char_info.ancs_data_source_hdl )
    {
        APPL_TRC("data source\n");
    }
#endif
    AncsDumpBytes(event_data + 2, (event_datalen-2));

}
#endif /*  ANCS */


