//*****************************************************************************
//
//! @file appl_ams.c
//!
//! @brief Provides functions for the AMS service.
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
 *  \file appl_ams.c
 *
 */


/* --------------------------------------------- Header File Inclusion */
#ifdef AMS
#include "appl.h"
#include "BT_common.h"
#include "BT_hci_api.h"
#include "BT_att_api.h"
#include "BT_smp_api.h"
#include "smp_pl.h"
#include "l2cap.h"
#include "fsm_defines.h"
#include "task.h"

/* ----------------------------------------- Configuration Defines */
extern uint32_t am_util_stdio_printf(const char *pcFmt, ...);
#undef APPL_TRC
#undef APPL_ERR
#define APPL_TRC am_util_stdio_printf
#define APPL_ERR am_util_stdio_printf


/* ----------------------------------------- Macro Defines */
/**@brief Entity IDs */
typedef enum
{
    AMS_ENTITY_ID_PLAYER,       /** The currently active media app. Attributes for this entity include values such as its name, playback state, and playback volume.  */
    AMS_ENTITY_ID_QUEUE,        /** The currently loaded playback queue. Attributes for this entity include values such as its size and its shuffle and repeat modes. */
    AMS_ENTITY_ID_TRACK,        /** The currently loaded track. Attributes for this entity include values such as its artist, title, and duration. */
} ams_entity_id_values_t;

/**@brief  Player AttributeID */
typedef enum
{
    AMS_PLAYER_ATTRID_NAME,         /** A string containing the localized name of the app  */
    AMS_PLAYER_ATTRID_PLAYBACKINFO, /** PlaybackState, PlaybackRate, ElapsedTime */
    AMS_PLAYER_ATTRID_VOLUME,           /** A string that represents the floating point value of the volume, ranging from 0 (silent) to 1 (full volume). */
} ams_player_attribute_id_values_t;

/**@brief  Queue AttributeID */
typedef enum
{
    AMS_QUEUE_ATTRID_INDEX,     /** A string containing the integer value of the queue index, zero-based.  */
    AMS_QUEUE_ATTRID_COUNT,     /** A string containing the integer value of the total number of items in the queue. */
    AMS_QUEUE_ATTRID_SHUFFLE,       /** A string containing the integer value of the shuffle mode. */
    AMS_QUEUE_ATTRID_REPEAT,        /** A string containing the integer value value of the repeat mode. */
} ams_queue_attribute_id_values_t;

/**@brief  Track AttributeID */
typedef enum
{
    AMS_TRACK_ATTRID_ARTIST,        /** A string containing the name of the artist.  */
    AMS_TRACK_ATTRID_ALBUM,     /** A string containing the name of the album. */
    AMS_TRACK_ATTRID_TITLE,         /** A string containing the title of the track. */
    AMS_TRACK_ATTRID_DURATION,      /** A string containing the floating point value of the total duration of the track in seconds. */
} ams_track_attribute_id_values_t;

typedef enum
{
    AMS_DISCONNECTED,
    AMS_CONNECTED,
    AMS_BONDED,
    AMS_DISCOVERED,
} ams_appl_state_t;

/* ----------------------------------------- External Global Variables */
void appl_profile_operations (void);
void appl_bond_with_peer(void);
void appl_discover_AMS(void);
extern BT_DEVICE_ADDR g_bd_addr;

/* ----------------------------------------- Exported Global Variables */


/* ----------------------------------------- Static Global Variables */
DECL_STATIC UCHAR write_response = 0;
DECL_STATIC API_RESULT gResult = 0;
DECL_STATIC ams_appl_state_t appl_ams_state = AMS_DISCONNECTED;
DECL_STATIC ATT_UUID ams_service_uuid128 = {.uuid_128.value = {0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89}};
DECL_STATIC ATT_UUID AMS_RemoteCommand_UUID = {.uuid_128.value = {0xC2, 0x51, 0xCA, 0xF7, 0x56, 0x0E, 0xDF, 0xB8, 0x8A, 0x4A, 0xB1, 0x57, 0xD8, 0x81, 0x3C, 0x9B}};
DECL_STATIC ATT_UUID AMS_EntityUpdate_UUID = {.uuid_128.value = {0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F}};
DECL_STATIC ATT_UUID AMS_EntityAttribute_UUID =     {.uuid_128.value = {0xD7, 0xD5, 0xBB, 0x70, 0xA8, 0xA3, 0xAB, 0xA6, 0xD8, 0x46, 0xAB, 0x23, 0x8C, 0xF3, 0xB2, 0xC6}};
/* BLE Connection Handle */
//DECL_STATIC UINT16 appl_ble_connection_handle;

/** AMS Characteristic related information */
typedef struct
{
    /* AMS Service */

    /* Remote Command */
    ATT_ATTR_HANDLE ams_remote_command_hdl;

    /* Remote Command - CCC */
    ATT_ATTR_HANDLE ams_remote_command_ccc_hdl;

    /* Entity Update */
    ATT_ATTR_HANDLE ams_entity_update_hdl;

    /* Entity Update - CCC */
    ATT_ATTR_HANDLE ams_entity_update_ccc_hdl;

    /* Entity Attribute */
    ATT_ATTR_HANDLE ams_entity_attribute_hdl;

} AMS_CHAR_INFO;

#define APPL_CLI_CNFG_VAL_LENGTH    2
DECL_STATIC AMS_CHAR_INFO ams_char_info;
#ifdef APPL_MENU_OPS
BT_DEVICE_ADDR g_peer_bd_addr;

static UCHAR ams_client_menu[] =
"\n\
\r\n\
   1.  Bond with peer \r\n\
\r\n\
   2.  Discover AMS \r\n\
\r\n\
   3.  Subscribe to Entity Update \r\n\
\r\n\
   4.  Write to Entity Update \r\n\
\r\n\
   5.  Write to Remote Command \r\n\
\r\n\
   6.  Write to Entity Attribute \r\n\
\r\n\
   7.  Read Entity Attribute \r\n\
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


void appl_ams_init(void)
{
    appl_ams_state = AMS_DISCONNECTED;
    APPL_TRC("\n\n<<appl_ams_init>>\n\n");
}

void appl_ams_connect(APPL_HANDLE  * appl_handle)
{
    APPL_STATE_T state = GET_APPL_STATE(*appl_handle);

    if ( state == SL_0_TRANSPORT_OPERATING  &&  appl_ams_state == AMS_DISCONNECTED )
    {
        appl_ams_state = AMS_CONNECTED;
        APPL_TRC("\n\n<<CONNECTED>>\n\n");
    }
    else if ( state == SL_0_TRANSPORT_OPERATING &&  appl_ams_state == AMS_DISCOVERED)
    {
        appl_ams_state = AMS_BONDED;
        APPL_TRC("\n\n<<BONDED>>\n\n");
    }
    else
    {
        appl_ams_state = AMS_DISCONNECTED;
        APPL_TRC("\n\n<<DISCONNECTED>>\n\n");
    }
}

void appl_search_complete(void)
{
    appl_ams_state = AMS_DISCOVERED;
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

void appl_send_ams_measurement (APPL_HANDLE   * handle)
{
    APPL_TRC("\n\n<<appl_send_ams_measurement>>\n\n");
}


void appl_timer_expiry_handler (void *data, UINT16 datalen)
{
    APPL_TRC("\n\n<<appl_timer_expiry_handler>>\n\n");
}

void appl_ams_server_reinitialize (void)
{
    appl_ams_state = AMS_DISCONNECTED;
    APPL_TRC("\n\n<<appl_ams_server_reinitialize>>\n\n");
}

void ams_mtu_update(APPL_HANDLE    * appl_handle, UINT16 t_mtu)
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
        if ( memcmp(&uuid, &AMS_RemoteCommand_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
        {
            ams_char_info.ams_remote_command_ccc_hdl = value_handle;
        }
        else if ( memcmp(&uuid, &AMS_EntityUpdate_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
        {
            ams_char_info.ams_entity_update_ccc_hdl = value_handle;
        }
    }
}

void appl_rcv_service_char (ATT_UUID uuid, UINT16 value_handle)
{
    if ( memcmp(&uuid, &AMS_RemoteCommand_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
    {
        ams_char_info.ams_remote_command_hdl = value_handle;
    }
    else if ( memcmp(&uuid, &AMS_EntityUpdate_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
    {
        ams_char_info.ams_entity_update_hdl = value_handle;
    }
    else if ( memcmp(&uuid, &AMS_EntityAttribute_UUID, ATT_128_BIT_UUID_SIZE) == 0 )
    {
        ams_char_info.ams_entity_attribute_hdl = value_handle;
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

void appl_discover_AMS(void)
{
    APPL_TRC("\n<<appl discover AMS>>\n");
    appl_discover_service
    (
        ams_service_uuid128,
        ATT_128_BIT_UUID_FORMAT
    );

    return;
}

void AmsWrite2EnityUpdate(uint8_t entityID)
{
    ATT_VALUE    att_value;
    uint8_t buf[5];

    APPL_TRC("\n++<<AmsWrite2EnityUpdate %d>>\n", entityID);

    if ( entityID == AMS_ENTITY_ID_PLAYER )
    {
        buf[0] = AMS_ENTITY_ID_PLAYER;
        buf[1] = AMS_PLAYER_ATTRID_NAME;
        buf[2] = AMS_PLAYER_ATTRID_PLAYBACKINFO;
        buf[3] = AMS_PLAYER_ATTRID_VOLUME;
        att_value.len = 4;
    }
    else if ( entityID == AMS_ENTITY_ID_QUEUE )
    {
        buf[0] = AMS_ENTITY_ID_QUEUE;
        buf[1] = AMS_QUEUE_ATTRID_INDEX;
        buf[2] = AMS_QUEUE_ATTRID_COUNT;
        buf[3] = AMS_QUEUE_ATTRID_SHUFFLE;
        buf[4] = AMS_QUEUE_ATTRID_REPEAT;
        att_value.len = 5;
    }
    else if ( entityID == AMS_ENTITY_ID_TRACK )
    {
        buf[0] = AMS_ENTITY_ID_TRACK;
        buf[1] = AMS_TRACK_ATTRID_ARTIST;
        buf[2] = AMS_TRACK_ATTRID_ALBUM;
        buf[3] = AMS_TRACK_ATTRID_TITLE;
        buf[4] = AMS_TRACK_ATTRID_DURATION;
        att_value.len = 5;
    }
    else
    {
        return;
    }

    att_value.val = buf;
    appl_write_req (ams_char_info.ams_entity_update_hdl, &att_value);

    Wait4WrtRsp();
    APPL_TRC("\n--<<AmsWrite2EnityUpdate %d>>\n", entityID);

}

void AmsWrite2RemoteCommand(uint8_t cmd)
{
    ATT_VALUE    att_value;
    uint8_t buf[1];   // retrieve the complete attribute list

    buf[0] = cmd;

    att_value.len = 1;
    att_value.val = buf;
    appl_write_req (ams_char_info.ams_remote_command_hdl, &att_value);

}

void AmsWrite2EntityAttribute(uint8_t entityID, uint8_t attrID)
{
    ATT_VALUE    att_value;
    uint8_t buf[2];   // retrieve the complete attribute list

    buf[0] = entityID;
    buf[1] = attrID;

    att_value.len = 2;
    att_value.val = buf;
    appl_write_req (ams_char_info.ams_entity_attribute_hdl, &att_value);

}

void AmsSubscribe2EnityUpdate(void)
{
    ATT_VALUE    att_value;
    UINT16       cli_cfg;
    UCHAR        cfg_val[APPL_CLI_CNFG_VAL_LENGTH];

    APPL_TRC("\n++<<AmsSubscribe2EnityUpdate>>\n");

    cli_cfg = GATT_CLI_CNFG_NOTIFICATION;
    BT_PACK_LE_2_BYTE(cfg_val, &cli_cfg);
    att_value.len = APPL_CLI_CNFG_VAL_LENGTH;
    att_value.val = cfg_val;
    appl_write_req (ams_char_info.ams_entity_update_ccc_hdl, &att_value);

    Wait4WrtRsp();

    APPL_TRC("\n--<<AmsSubscribe2EnityUpdate(0x%04X)>>\n", gResult);
}

void appl_profile_operations (void)
{

    const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
    write_response = 0;

    fsm_post_event
    (
        APPL_FSM_ID,
        ev_appl_device_init_req,
        NULL
    );

    while ( appl_ams_state != AMS_CONNECTED )
    {
        vTaskDelay( xDelay );
    }

    appl_discover_AMS();

    while ( appl_ams_state != AMS_DISCOVERED )
    {
        vTaskDelay( xDelay );
    }

    AmsSubscribe2EnityUpdate();

    if (gResult == 0x0305 /* insufficient authentication */ ||
        gResult == 0x030F /* insufficient Encryption */ )
    {
        appl_bond_with_peer();
        while ( appl_ams_state != AMS_BONDED )
        {
            vTaskDelay( xDelay );
        }
        AmsSubscribe2EnityUpdate();
    }
    else
    {
        appl_ams_state = AMS_BONDED;
    }

    AmsWrite2EnityUpdate(AMS_ENTITY_ID_PLAYER);

    AmsWrite2EnityUpdate(AMS_ENTITY_ID_QUEUE);

    AmsWrite2EnityUpdate(AMS_ENTITY_ID_TRACK);

    while ( appl_ams_state != AMS_DISCONNECTED )
    {
        vTaskDelay( xDelay );
    }

}

void AmsDumpBytes (UCHAR *buffer, UINT16 length)
{
    char hex_stream[49];
    char char_stream[17];
    UINT32 i;
    UINT16 offset, count;
    UCHAR c;

    APPL_TRC("\n");

    APPL_TRC (
    "-------------------------------------------------------------------\n");

    count = 0;
    offset = 0;
    for (i = 0; i < length; i ++ )
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
    "-------------------------------------------------------------------\n");

    APPL_TRC ("\n");

    return;
}


void AmsNotificationCb(UCHAR *event_data, UINT16 event_datalen)
{
    AmsDumpBytes(event_data + 2, (event_datalen - 2));
}

void AmsReadRSPCb(UCHAR *event_data, UINT16 event_datalen)
{
    ATT_ATTR_HANDLE  attr_handle;

    BT_UNPACK_LE_2_BYTE(&attr_handle, event_data);

    AmsDumpBytes(event_data + 2, (event_datalen - 2));
}
#endif /*  AMS */


