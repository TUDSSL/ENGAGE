//*****************************************************************************
//
//! @file appl_ancs_att_server.c
//!
//! @brief This file contains the ATT server and client application.
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
 *  \file appl_ancs_att_server.c
 *
 *  This file contains the ATT server and client application.
 */

/*
 *  Copyright (C) 2013. Mindtree Ltd.
 *  All rights reserved.
 */

/* --------------------------------------------- Header File Inclusion */
#include "appl.h"
#include "appl_ancs_att_server.h"
#include "appl_fsm_defines.h"

extern void AncsNotificationCb(UCHAR *event_data, UINT16 event_datalen);
extern void appl_search_complete(void);
extern void appl_recvice_att_event(UCHAR att_event, API_RESULT event_result);

#ifdef ATT

/* --------------------------------------------- External Global Variables */
ATT_UUID  init_uuid = {0};
/* --------------------------------------------- Exported Global Variables */
#ifdef APPL_VALIDATE_ATT_PDU_LEN
/* Application Error code for len validation */
#define APPL_ATT_IGNORE_PDU_LEN_CHECK   (0x00CC | APPL_ERR_ID)
#endif /* APPL_VALIDATE_ATT_PDU_LEN */

#define APPL_INVALID_UUID_FORMAT   (0x00CD | APPL_ERR_ID)

#ifdef ATT_CLIENT
#define APPL_MAX_CHARACTERISTICS         16

#define APPL_GD_INIT_STATE               0x00
#define APPL_GD_IN_SERV_DSCRVY_STATE     0x01
#define APPL_GD_IN_CHAR_DSCVRY_STATE     0x02
#define APPL_GD_IN_DESC_DSCVRY_STATE     0x03

typedef struct
{
    ATT_ATTR_HANDLE    start_handle;

    ATT_UUID         uuid;
}APPL_CHARACTERISTIC;

/*  Service Discovery State Information */
typedef struct
{
    ATT_ATTR_HANDLE    service_end_handle;

    ATT_UUID         service_uuid;

    /**
     * APPL_GD_INIT_STATE
     * APPL_GD_IN_SERV_DSCRVY_STATE
     * APPL_GD_IN_CHAR_DSRCVRY_STATE
     * APPL_GD_IN_DSEC_DSCVRY_STATE
     */
    UCHAR              state;

    UCHAR              current_char_index;

    UCHAR              char_count;

}APPL_SERVICE_DISCOVERY_STATE;

DECL_STATIC APPL_SERVICE_DISCOVERY_STATE appl_gatt_dscvry_state;

DECL_STATIC APPL_CHARACTERISTIC appl_char[APPL_MAX_CHARACTERISTICS];

/* This holds the descriptor discovery count */
UCHAR appl_char_descptr_discovery_count;

DECL_STATIC ATT_ATTR_HANDLE service_start_handle;
DECL_STATIC ATT_ATTR_HANDLE service_end_handle;

/* GATT Handle */
DECL_STATIC ATT_HANDLE appl_gatt_client_handle;

#endif /* ATT_CLIENT */

/* --------------------------------------------- Macros */
#ifdef ATT_CLIENT

#define APPL_INIT_GATT_DSCRY_STATE()\
        APPL_GET_SER_END_HANDLE() = 0x0000;\
        APPL_GET_CURRENT_DSCVRY_STATE() = APPL_GD_INIT_STATE;\
        APPL_CURRENT_SERVICE_UUID() = APPL_UUID_INIT_VAL;\
        APPL_GET_CURRENT_CHAR_INDEX() = APPL_INVALID_CHAR_INDEX;\
        APPL_GET_CHAR_COUNT() = APPL_CHAR_COUNT_INIT_VAL;

#define APPL_GET_CHAR_COUNT()\
        appl_gatt_dscvry_state.char_count

#define APPL_CHAR_GET_START_HANDLE(i)\
        appl_char[(i)].start_handle

#define APPL_GET_SER_END_HANDLE()\
        appl_gatt_dscvry_state.service_end_handle

#define APPL_GET_CURRENT_CHAR_INDEX()\
        appl_gatt_dscvry_state.current_char_index

#define APPL_GET_CURRENT_DSCVRY_STATE()\
        appl_gatt_dscvry_state.state

#define APPL_CHAR_GET_UUID(i)\
        appl_char[(i)].uuid

#define APPL_UUID_INIT_VAL               init_uuid
#define APPL_INVALID_CHAR_INDEX          0xFF
#define APPL_CHAR_COUNT_INIT_VAL         0x00

#define APPL_CURRENT_SERVICE_UUID()\
        appl_gatt_dscvry_state.service_uuid

#define APPL_CHAR_INIT(i)\
        APPL_CHAR_GET_START_HANDLE(i) = ATT_INVALID_ATTR_HANDLE_VAL;\
        APPL_CHAR_GET_UUID(i) = APPL_UUID_INIT_VAL;

#endif /* ATT_CLIENT */

/* --------------------------------------------- Static Global Variables */
#ifdef SMP_DATA_SIGNING
static UCHAR * s_data;
static UINT16 s_datalen;
static ATT_HANDLE appl_signed_write_server_handle;
#endif /* SMP_DATA_SIGNING */

#ifdef ATT_QUEUED_WRITE_SUPPORT
API_RESULT appl_att_prepare_write_cancel (ATT_HANDLE   *handle);
API_RESULT appl_att_prepare_write_execute (ATT_HANDLE   *handle);
API_RESULT appl_att_prepare_write_queue_init (void);

static ATT_HANDLE_VALUE_OFFSET_PARAM appl_att_write_queue[APPL_ATT_WRITE_QUEUE_SIZE];
static UCHAR appl_att_write_queue_buffer[APPL_ATT_MAX_WRITE_BUFFER_SIZE];
static UCHAR appl_att_write_buffer_ptr;
static UCHAR appl_att_write_queue_index;
static APPL_ATT_EXECUTE_WRITE_HANDLER appl_execute_write_handler[2] =
                  {
                      appl_att_prepare_write_cancel,
                      appl_att_prepare_write_execute
                  };
#endif /* ATT_QUEUED_WRITE_SUPPORT */
UCHAR appl_att_buffer[APPL_SERVER_BUFFER_SIZE];
UCHAR appl_hvc_flag;

#ifdef APPL_VALIDATE_ATT_PDU_LEN
static APPL_VALID_ATT_PDU_LEN appl_valid_att_pdu_len
                              [APPL_MAX_VALID_ATT_PDU_LEN_CHK] =
{
    {
        ATT_XCNHG_MTU_REQ,
        APPL_ATT_XCNHG_MTU_REQ_LEN,
        APPL_ATT_INVALID_LEN
    },
    {
        ATT_FIND_INFO_REQ,
        APPL_ATT_FIND_INFO_REQ_LEN,
        APPL_ATT_INVALID_LEN
    },
    {
        ATT_READ_BY_TYPE_REQ,
        APPL_ATT_READ_BY_TYPE_REQ_LEN_1,
        APPL_ATT_READ_BY_TYPE_REQ_LEN_2
    },
    {
        ATT_READ_REQ,
        APPL_ATT_READ_REQ_LEN,
        APPL_ATT_INVALID_LEN
    },
    {
        ATT_READ_BLOB_REQ,
        APPL_ATT_READ_BLOB_REQ_LEN,
        APPL_ATT_INVALID_LEN
    },
    {
        ATT_READ_BY_GROUP_REQ,
        APPL_ATT_READ_BY_GROUP_REQ_LEN_1,
        APPL_ATT_READ_BY_GROUP_REQ_LEN_2
    },
    {
        ATT_EXECUTE_WRITE_REQ,
        APPL_ATT_EXECUTE_WRITE_REQ_LEN,
        APPL_ATT_INVALID_LEN

    }
};
#endif /* APPL_VALIDATE_ATT_PDU_LEN */

/* --------------------------------------------- Functions */

API_RESULT appl_att_server_init(void)
{
#ifdef ATT_QUEUED_WRITE_SUPPORT
    appl_att_prepare_write_queue_init ();
#endif /* ATT_QUEUED_WRITE_SUPPORT */

    return API_SUCCESS;
}

#ifdef ATT_QUEUED_WRITE_SUPPORT
API_RESULT appl_att_prepare_write_queue_init (void)
{
    UINT32 index;

    index = 0;

    do
    {
        APPL_TRC (
            "[APPL]: Initializing Queue Element 0x%02X\n", index);

        APPL_ATT_PREPARE_QUEUE_INIT (index);
        index++;
    } while (index < APPL_ATT_WRITE_QUEUE_SIZE);

    appl_att_write_buffer_ptr = 0;
    appl_att_write_queue_index = 0;

    return API_SUCCESS;
}

API_RESULT appl_att_prepare_write_enqueue (ATT_HANDLE_VALUE_OFFSET_PARAM * param)
{
    API_RESULT retval;

    retval = API_FAILURE;

    APPL_TRC (
        "[APPL]: Queue index: 0x%02X Offset: 0x%04X Length: 0x%04X, Value "
        "Buffer Index 0x%02X", appl_att_write_queue_index,
        appl_att_write_queue[appl_att_write_queue_index].offset,
        appl_att_write_queue[appl_att_write_queue_index].handle_value.value.len,
        appl_att_write_buffer_ptr);

    if ((APPL_ATT_MAX_WRITE_BUFFER_SIZE > (appl_att_write_buffer_ptr + param->handle_value.value.len)) &&
       (APPL_ATT_WRITE_QUEUE_SIZE != appl_att_write_queue_index))
    {
        APPL_TRC (
            "[APPL]: Allocating Element 0x%02X, Buffer Index 0x%02X\n",
            appl_att_write_queue_index, appl_att_write_buffer_ptr);

        appl_att_write_queue[appl_att_write_queue_index] = (*param);
        appl_att_write_queue[appl_att_write_queue_index].handle_value.value.val\
            = &appl_att_write_queue_buffer[appl_att_write_buffer_ptr];

        BT_mem_copy
        (
            appl_att_write_queue[appl_att_write_queue_index].\
            handle_value.value.val,
            param->handle_value.value.val,
            param->handle_value.value.len
        );

        APPL_TRC (
            "[APPL]: Queued element Offset: 0x%04X, Length: 0x%04X\n",
            appl_att_write_queue[appl_att_write_queue_index].offset,
            appl_att_write_queue[appl_att_write_queue_index].handle_value.value.len);

        appl_att_write_queue_index++;
        appl_att_write_buffer_ptr += param->handle_value.value.len;

        APPL_TRC (
            "[APPL]: Queue index: 0x%02X , Value Buffer Index 0x%02X\n",
            appl_att_write_queue_index,
            appl_att_write_buffer_ptr);

        retval = API_SUCCESS;
    }
    else
    {
        APPL_TRC (
            "[APPL]: Cannot Enqueue value!");
    }

    return retval;
}

API_RESULT appl_att_prepare_write_cancel (ATT_HANDLE   * handle)
{
    API_RESULT retval;

    appl_att_prepare_write_queue_init ();

    /* Send response */
    retval = BT_att_send_execute_write_rsp (handle);

    APPL_TRC (
        "[APPL]: Sent Execute Write Response with result 0x%04X", retval);

    return retval;
}

API_RESULT appl_att_prepare_write_execute (ATT_HANDLE   * handle)
{
    UINT32                            index;
    API_RESULT                        retval;
    ATT_ERROR_RSP_PARAM               err_param;

    retval = API_SUCCESS;
    err_param.op_code    = ATT_EXECUTE_WRITE_REQ;
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;

    index = 0;

    APPL_TRC (
        "[APPL]: Number of elements to be executed = 0x%02X",
        appl_att_write_queue_index);

    while (index < appl_att_write_queue_index)
    {
        APPL_TRC (
            "[APPL]: Executing Element 0x%02X Handle 0x%04X, Value Length 0x%04X\n",
            index, appl_att_write_queue[index].handle_value.handle,
            appl_att_write_queue[index].handle_value.value.len);

        retval = BT_gatt_db_access_handle
                 (
                      &appl_att_write_queue[index].handle_value,
                      handle,
                      appl_att_write_queue[index].offset,
                      (GATT_DB_WRITE | GATT_DB_PEER_INITIATED)
                 );

        if (API_SUCCESS != retval)
        {
            APPL_TRC (
                "[APPL]: Failed to Execute write on handle 0x%04X, reason 0x%04X\n",
                appl_att_write_queue[index].handle_value.handle, retval);
            break;
        }
        else
        {
#ifdef OTS
            /* Passing requested Complete Execute writes with offset to Higer Layer */
            appl_ots_update_obj_metadata_val
            (
                handle,
                appl_att_write_queue[index].handle_value.handle,
                &appl_att_write_queue[index].handle_value.value,
                appl_att_write_queue[index].offset
            );
#endif /* OTS */
        }
        index++;
    }

    /* TBD: What happens if Write on or more but not all attributes fails? */
    if (API_SUCCESS == retval)
    {
        retval = BT_att_send_execute_write_rsp
                 (
                      handle
                 );

        APPL_TRC (
            "[APPL]: Execute Write Response sent with result 0x%04X\n", retval);
    }
    else
    {
        err_param.handle = appl_att_write_queue[index].handle_value.handle;
        if (GATT_DB_INVALID_OFFSET == retval)
        {
            err_param.error_code = ATT_INVALID_OFFSET;
        }
        else if (GATT_DB_INSUFFICIENT_BUFFER_LEN == retval)
        {
            err_param.error_code = ATT_INVALID_ATTRIBUTE_LEN;
        }
        else
        {
            err_param.error_code = (UCHAR)(0x00FF & retval);
        }
        retval = BT_att_send_error_rsp
                 (
                     handle,
                     &err_param
                 );
        APPL_TRC (
            "[APPL]: Sent Error Response with result 0x%04X\n", retval);
    }

    appl_att_prepare_write_queue_init ();

    return retval;
}
#endif /* ATT_QUEUED_WRITE_SUPPORT */

API_RESULT appl_handle_unsupported_op_code (ATT_HANDLE * handle, UCHAR op_code)
{
    ATT_ERROR_RSP_PARAM               err_param;
    API_RESULT                        retval;

    err_param.handle = ATT_INVALID_ATTR_HANDLE_VAL;
    err_param.op_code = op_code;
    err_param.error_code = ATT_REQUEST_NOT_SUPPORTED;

    retval = BT_att_send_error_rsp (handle, &err_param);

    if (API_SUCCESS != retval)
    {
        APPL_ERR (
        "[APPL]:[*** ERR ***]: Failed to send error response, reason 0x%04X\n",
        retval);
    }
    else
    {
        APPL_ERR (
        "[APPL]:Sent Error Response for unsupported Op Code\n");
    }

    return retval;
}


API_RESULT appl_handle_find_by_type_val_request
           (
                /* IN */ ATT_HANDLE          * handle,
                /* IN */ ATT_HANDLE_RANGE    * range,
                /* IN */ ATT_VALUE           * value,
                /* IN */ ATT_VALUE           * uuid
           )
{
    ATT_ERROR_RSP_PARAM               err_param;
    ATT_FIND_BY_TYPE_VAL_RSP_PARAM    list;
    ATT_HANDLE_RANGE                  group_list[APPL_MAX_GROUP_TYPE_QUERIED];
    API_RESULT                        retval;

    list.count = 0;

    err_param.op_code = ATT_FIND_BY_TYPE_VAL_REQ;
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
    err_param.handle = range->start_handle;

    APPL_TRC(
        "[APPL]: >>> Handling Find By Type Value request, Range 0x%04X-0x%04X\n",
        range->start_handle, range->end_handle);

    do
    {
        retval = BT_gatt_db_get_range_by_type_val
                 (
                     handle,
                     range,
                     uuid,
                     value,
                     &group_list[list.count]
                 );

        if ((API_SUCCESS == retval) ||
           (GATT_DB_MORE_MATCHING_RESULT_FOUND == retval))
        {
            list.count++;
        }
        if (GATT_DB_MORE_MATCHING_RESULT_FOUND != retval)
        {
            /**
             * This is an invalid sentence in the specification. An errata has
             * been raised and adopted
             * BT_Errata_ID:  4062
             */
#if 0
            if (list.count != 0)
            {
                group_list[list.count-1].end_handle = ATT_ATTR_HANDLE_END_RANGE;
            }
#endif /* 0 */
            break;
        }
    } while ((list.count < APPL_MAX_GROUP_TYPE_QUERIED) &&
            (range->end_handle >= range->start_handle));

    if (0 != list.count)
    {
        list.range = group_list;
        APPL_TRC(
            "[APPL]: Number of occurences of UUID 0x%04X = 0x%04X\n",
            *uuid->val, list.count);

        retval = BT_att_send_find_by_type_val_rsp
                 (
                     handle,
                     &list
                 );
    }
    else
    {
        retval = BT_att_send_error_rsp
                 (
                     handle,
                     &err_param
                 );
        APPL_TRC (
            "[APPL]: Sent Error Response with result 0x%04X\n", retval);
    }

    APPL_TRC(
        "[APPL]: <<< Handling Find By Type Value request, Range 0x%04X-0x%04X,"
        " with result 0x%04X\n", range->start_handle, range->end_handle, retval);

    return retval;
}


API_RESULT appl_handle_find_info_request
           (
                /* IN */ ATT_HANDLE         * handle,
                /* IN */ ATT_HANDLE_RANGE   * range
           )
{
    ATT_FIND_INFO_RSP_PARAM    param;
    ATT_ERROR_RSP_PARAM        err_param;
    ATT_HANDLE_VALUE_PAIR      handle_value_list[APPL_MAX_HNDL_UUID_LIST_SIZE];
    ATT_HANDLE_UUID_PAIR       handle_uuid_list[APPL_MAX_HNDL_UUID_LIST_SIZE];
    UINT32                     index;
    API_RESULT                 retval;

    param.uuid_format = ATT_16_BIT_UUID_FORMAT;

    APPL_TRC(
        "[APPL]: >>> Handling Find Info request, Range 0x%04X-0x%04X\n",
        range->start_handle, range->end_handle);


    index = 0;
    err_param.handle = range->start_handle;
    err_param.op_code = ATT_FIND_INFO_REQ;
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
    do
    {
        if (ATT_16_BIT_UUID_FORMAT == param.uuid_format)
        {
            handle_value_list[index].value.len = ATT_16_BIT_UUID_SIZE;
        }

        retval = BT_gatt_db_get_handle_uuid_pair
                 (
                     handle,
                     range,
                     &handle_value_list[index]
                 );

        if ((API_SUCCESS == retval) ||
           (GATT_DB_MORE_MATCHING_RESULT_FOUND == retval))
        {
#ifdef ATT_SUPPORT_128_BIT_UUID
            APPL_TRC (
                "[APPL]:[0x%04X]: Handle 0x%04X -> UUID Len 0x%04X\n", index,
                handle_value_list[index].handle,
                handle_value_list[index].value.len);
#else /* ATT_SUPPORT_128_BIT_UUID */
            APPL_TRC (
                "[APPL]:[0x%04X]: Handle 0x%04X -> UUID Len 0x%04X\n", index,
                handle_value_list[index].handle,
                handle_value_list[index].value.len);
#endif /* ATT_SUPPORT_128_BIT_UUID */
            index++;
        }

        if (GATT_DB_INCORRECT_UUID_FRMT == retval)
        {
            if (0 == index)
            {
                /* Search again, change the UUID format requested */
                param.uuid_format = ATT_128_BIT_UUID_FORMAT;
                handle_value_list[index].value.len = ATT_128_BIT_UUID_SIZE;
            }
            else
            {
                /* Else, pack no more, as mixed UUID format detected */
                break;
            }
        }
        else if (GATT_DB_INVALID_TRANSPORT_ACCESS == retval)
        {
            continue;
        }
        else if (GATT_DB_MORE_MATCHING_RESULT_FOUND != retval)
        {
            /* Search complete */
            APPL_TRC (
                "[APPL]:Search Complete, #Handle UUID List found = 0x%02X\n",
                index);
            break;
        }
    }while (index < APPL_MAX_HNDL_UUID_LIST_SIZE);

    if (0 != index)
    {
        param.handle_value_list.list_count = index;

        for (index = 0; index < param.handle_value_list.list_count; index++)
        {
            handle_uuid_list[index].handle = handle_value_list[index].handle;
#ifdef ATT_SUPPORT_128_BIT_UUID
            if (handle_value_list[index].value.len == ATT_16_BIT_UUID_SIZE)
            {
                BT_UNPACK_LE_2_BYTE
                (
                    &handle_uuid_list[index].uuid.uuid_16,
                    handle_value_list[index].value.val
                );
            }
            else
            {
                BT_UNPACK_LE_N_BYTE
                (
                    &handle_uuid_list[index].uuid.uuid_128,
                    handle_value_list[index].value.val,
                    ATT_128_BIT_UUID_SIZE
                );
            }
#else
            BT_UNPACK_LE_2_BYTE
            (
                &handle_uuid_list[index].uuid,
                handle_value_list[index].value.val
            );
#endif /* ATT_SUPPORT_128_BIT_UUID */
        }

        param.handle_value_list.list = &handle_uuid_list[0];

        retval = BT_att_send_find_info_rsp (handle, &param);
        APPL_TRC (
            "[APPL]: Sent Find Info Response with result 0x%04X\n", retval);
    }
    else
    {
        retval = BT_att_send_error_rsp
                 (
                     handle,
                     &err_param
                 );
        APPL_TRC (
            "[APPL]: Sent Error Response with result 0x%04X\n", retval);
    }

    APPL_TRC(
        "[APPL]: <<< Handling Find Info request, Range 0x%04X-0x%04X\n",
        range->start_handle, range->end_handle);

    return retval;
}

API_RESULT appl_handle_read_by_type_request
           (
               ATT_HANDLE          * handle,
               ATT_HANDLE_RANGE    * range,
               ATT_VALUE           * uuid
           )
{
    ATT_READ_BY_TYPE_RSP_PARAM    param;
    ATT_ERROR_RSP_PARAM           err_param;
    ATT_HANDLE_VALUE_PAIR         handle_value_list[APPL_MAX_HNDL_VALUE_SIZE];
    UINT32                        index;
    API_RESULT                    retval;
    UINT16                        len;

    APPL_TRC(
        "[APPL]: >>> Handling Read By Type request, Range 0x%04X-0x%04X\n",
        range->start_handle, range->end_handle);

    index = 0;
    len = 0;
    err_param.handle = range->start_handle;
    err_param.op_code = ATT_READ_BY_TYPE_REQ;
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
    do
    {
        handle_value_list[index].value.len = (APPL_SERVER_BUFFER_SIZE - len);

        retval = BT_gatt_db_get_handle_value_pair
                 (
                     handle,
                     range,
                     uuid,
                     &handle_value_list[index]
                 );

        if ((API_SUCCESS == retval) ||
           (GATT_DB_MORE_MATCHING_RESULT_FOUND == retval))
        {
            APPL_TRC (
                "[APPL]:[0x%04X]: Handle 0x%04X -> Value Len 0x%04X\n", index,
                handle_value_list[index].handle,
                handle_value_list[index].value.len);

#if 0
            /**
             * Commenting out, unnecessary checks will should ideally not be true.
             */
            if (handle_value_list[index].handle > range->end_handle)
            {
                break;
            }
#endif /* 0 */
            if (0 == index)
            {
                len = handle_value_list[index].value.len;
                index++;
            }
            else if (len == handle_value_list[index].value.len)
            {
                index++;
            }
            else
            {
                /* Record not of same length, skip */
                break;
            }
        }

        if (GATT_DB_INVALID_TRANSPORT_ACCESS == retval)
        {
            continue;
        }
        else if (GATT_DB_MORE_MATCHING_RESULT_FOUND != retval)
        {
            /* Search complete */
            APPL_TRC (
                "[APPL]:Search Complete, #Handle UUID List found = 0x%02X\n",
                index);
            break;
        }
    }while (index < APPL_MAX_HNDL_VALUE_SIZE);

    if (0 != index)
    {
        param.handle_value_list = handle_value_list;
        param.count = index;

        retval = BT_att_read_by_type_rsp (handle, &param);
        APPL_TRC (
            "[APPL]: Sent Read By Type Response with result 0x%04X\n", retval);
    }
    else
    {
        if (GATT_DB_INVALID_OPERATION == retval)
        {
            err_param.handle = handle_value_list[index].handle;
            err_param.error_code = ATT_READ_NOT_PERMITTED;
        }
        else if (GATT_DB_INSUFFICIENT_SECURITY == retval)
        {
            err_param.handle = handle_value_list[index].handle;
            err_param.error_code = ATT_INSUFFICIENT_AUTHENTICATION;
        }
        else if (GATT_DB_INSUFFICIENT_ENC_KEY_SIZE == retval)
        {
            err_param.handle = handle_value_list[index].handle;
            err_param.error_code = ATT_INSUFFICIENT_ENC_KEY_SIZE;
        }
        else if (GATT_DB_INSUFFICIENT_ENCRYPTION == retval)
        {
            err_param.handle = handle_value_list[index].handle;
            err_param.error_code = ATT_INSUFFICIENT_ENCRYPTION;
        }

        retval = BT_att_send_error_rsp
                 (
                     handle,
                     &err_param
                 );
        APPL_TRC (
            "[APPL]: Sent Error Response with result 0x%04X\n", retval);
    }

    APPL_TRC(
        "[APPL]: <<< Handling Read By Type request, Range 0x%04X-0x%04X\n",
        range->start_handle, range->end_handle);

    return retval;
}

API_RESULT appl_handle_read_by_group_type_request
           (
               ATT_HANDLE          * handle,
               ATT_HANDLE_RANGE    * range,
               ATT_VALUE           * uuid
           )
{
    ATT_ERROR_RSP_PARAM            err_param;
    ATT_GROUP_ATTR_DATA_LIST       list;
    ATT_GROUP_ATTR_DATA_ELEMENT    group_list[APPL_MAX_GROUP_TYPE_QUERIED];
    ATT_VALUE                      value;
    API_RESULT                     retval;

    list.count = 0;

    err_param.handle = range->start_handle;
    err_param.op_code = ATT_READ_BY_GROUP_REQ;
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
#ifdef ATT_SUPPORT_128_BIT_UUID
    value.len = ATT_128_BIT_UUID_SIZE;
#else /* ATT_SUPPORT_128_BIT_UUID */
    value.len = ATT_16_BIT_UUID_SIZE;
#endif /* ATT_SUPPORT_128_BIT_UUID */
    do
    {
        group_list[list.count].range = *range;
        /**
         *  Value length, for the first time is initialized to 128 Bit UUID
         *  Size, thereafter, should be left to what is the first Group value
         *  fetched as in the same response mixed format UUIDs cannot be packed
         */
        retval = BT_gatt_db_get_group_range_val_pair
                 (
                     handle,
                     range,
                     uuid,
                     &group_list[list.count].range,
                     &value
                 );

        if ((API_SUCCESS == retval) ||
           (GATT_DB_MORE_MATCHING_RESULT_FOUND == retval))
        {
            if (0 == list.count)
            {
                list.length = value.len;
            }
            else if (value.len != list.length)
            {
                /* Mixed formatting encountered, stop the search */
                break;
            }

            group_list[list.count].attr_value = value.val;
            list.count++;
        }

        if (GATT_DB_INVALID_TRANSPORT_ACCESS == retval)
        {
            continue;
        }
        else if (GATT_DB_MORE_MATCHING_RESULT_FOUND != retval)
        {
            /**
             * Spec mandates group end handle of last found group element to be
             * set to ATT_ATTR_END_RANGE to indicate 'Search Complete, however
             * no such mandate specified for Read By Group Type.
             */
#if 0
            if (list.count != 0)
            {
                group_list[list.count-1].range.end_handle =
                    ATT_ATTR_HANDLE_END_RANGE;
            }
#endif /* 0 */
            break;
        }
    } while (list.count < APPL_MAX_GROUP_TYPE_QUERIED);

    if (0 != list.count)
    {
        list.list = group_list;

        retval = BT_att_read_by_group_rsp
                 (
                     handle,
                     &list
                 );
    }
    else
    {
        if (GATT_DB_UNSUPPORTED_GROUP_TYPE == retval)
        {
            err_param.error_code = ATT_UNSUPPORTED_GROUP_TYPE;
        }

        retval = BT_att_send_error_rsp
                 (
                     handle,
                     &err_param
                 );
        APPL_TRC (
            "[APPL]: Sent Error Response with result 0x%04X\n", retval);
    }

    return retval;
}

API_RESULT appl_handle_read_multiple_request
           (
                /* IN */ ATT_HANDLE          * handle,
                /* IN */ ATT_HANDLE_LIST     * list
           )
{
    ATT_ERROR_RSP_PARAM            err_param;
    ATT_READ_MULTIPLE_RSP_PARAM    rsp_param;
    ATT_HANDLE_VALUE_PAIR          pair;
    ATT_VALUE                      value[APPL_MAX_MULTIPLE_READ_COUNT];
    UINT32                         index;
    API_RESULT                     retval;

    index = 0;

    do
    {
        pair.handle = list->handle_list[index];
        retval  = BT_gatt_db_access_handle
                  (
                      &pair,
                      handle,
                      0,
                      (GATT_DB_READ | GATT_DB_PEER_INITIATED)
                  );

        if (API_SUCCESS == retval)
        {
            value[index].val = pair.value.val;
            value[index].len = pair.value.len;
            index++;
        }
        else
        {
            break;
        }
    } while ( index < list->list_count );

    if (API_SUCCESS == retval)
    {
        rsp_param.count = index;
        rsp_param.value = value;

         retval = BT_att_read_multiple_rsp
                  (
                      handle,
                      &rsp_param
                  );

         if (API_SUCCESS == retval)
         {
             APPL_TRC (
                 "[APPL]: Successfully Sent Read Multiple Response, 0%04X of 0x%04X"
                 "values transferred!", rsp_param.actual_count, rsp_param.count);
         }
         else
         {
             APPL_ERR (
                 "[APPL]:[*** ERR ***]: Failed to send read Multiple Response, "
                 "Reason 0x%04X", retval);
         }
    }
    else
    {
        err_param.op_code = ATT_READ_MULTIPLE_REQ;
        err_param.handle = list->handle_list[index];

        if (GATT_DB_INVALID_OPERATION == retval)
        {
            err_param.error_code = ATT_READ_NOT_PERMITTED;
        }
        else if ((GATT_DB_INVALID_ATTR_HANDLE == retval) ||
            (GATT_DB_INVALID_TRANSPORT_ACCESS == retval))
        {
            err_param.error_code = ATT_INVALID_HANDLE;
        }
        else if (GATT_DB_INSUFFICIENT_SECURITY == retval)
        {
            err_param.error_code = ATT_INSUFFICIENT_AUTHENTICATION;
        }
        else if (GATT_DB_INSUFFICIENT_ENC_KEY_SIZE == retval)
        {
            err_param.error_code = ATT_INSUFFICIENT_ENC_KEY_SIZE;
        }

        /* Send Error Response */
        retval = BT_att_send_error_rsp
                 (
                     handle,
                     &err_param
                 );
        APPL_TRC (
            "[APPL]: Sent Error Response with result 0x%04X", retval);

    }

    return retval;
}


API_RESULT appl_handle_read_request
           (
               /* IN */ ATT_HANDLE    * handle,
               /* IN */ UINT16        attr_handle,
               /* IN */ UINT16        offset,
               /* IN */ UCHAR         direction
           )
{
    ATT_ERROR_RSP_PARAM      err_param;
    ATT_READ_RSP_PARAM       * rsp_param;
    ATT_HANDLE_VALUE_PAIR    pair;
    API_RESULT               retval;
    UINT16                   temp_offset;

    err_param.handle = attr_handle;
    temp_offset = offset;

    if (APPL_ATT_INVALID_OFFSET == offset)
    {
        err_param.op_code = ATT_READ_REQ;
        temp_offset = 0;
    }
    else
    {
        err_param.op_code = ATT_READ_BLOB_REQ;
    }
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;

    pair.value.len = APPL_SERVER_BUFFER_SIZE;
    pair.handle = attr_handle;

    retval  = BT_gatt_db_access_handle
              (
                  &pair,
                  handle,
                  temp_offset,
                  (GATT_DB_READ | direction)
              );

    if (API_SUCCESS == retval)
    {
        if (GATT_DB_PEER_INITIATED == direction)
        {
            rsp_param = &pair.value;

            if (APPL_ATT_INVALID_OFFSET == offset)
            {
                /* Read Request */
                retval = BT_att_read_rsp
                         (
                              handle,
                              rsp_param
                         );

                APPL_TRC (
                    "[APPL]:Read Response sent with result 0x%04X\n", retval);
            }
            else
            {
                /* Read Blob Response */
                retval = BT_att_read_blob_rsp
                         (
                              handle,
                              rsp_param
                         );

                APPL_TRC (
                    "[APPL]:Read Blob Response sent with result 0x%04X\n", retval);
            }
        }
        else
        {
            APPL_TRC (
                "[APPL] Value of local handle 0x%04X\n", attr_handle);
            appl_dump_bytes (pair.value.val, pair.value.len);
        }
    }
    else
    {
        if (GATT_DB_PEER_INITIATED == direction)
        {
            if (GATT_DB_INVALID_OPERATION == retval)
            {
                err_param.error_code = ATT_READ_NOT_PERMITTED;
            }
            else if ((GATT_DB_INVALID_ATTR_HANDLE == retval) ||
                (GATT_DB_INVALID_TRANSPORT_ACCESS == retval))
            {
                err_param.error_code = ATT_INVALID_HANDLE;
            }
            else if (GATT_DB_INSUFFICIENT_SECURITY == retval)
            {
                err_param.error_code = ATT_INSUFFICIENT_AUTHENTICATION;
            }
            else if (GATT_DB_INSUFFICIENT_ENC_KEY_SIZE == retval)
            {
                err_param.error_code = ATT_INSUFFICIENT_ENC_KEY_SIZE;
            }
            else if (GATT_DB_INSUFFICIENT_ENCRYPTION == retval)
            {
                err_param.error_code = ATT_INSUFFICIENT_ENCRYPTION;
            }
            else if (GATT_DB_HANDLE_NOT_FOUND == retval)
            {
                err_param.error_code = ATT_INVALID_HANDLE;
            }
            else
            {
                err_param.error_code = (UCHAR)(0x00FF & retval);
            }

            if (GATT_DB_DELAYED_RESPONSE != retval)
            {
                /* Send Error Response */
                retval = BT_att_send_error_rsp
                         (
                             handle,
                             &err_param
                         );
                APPL_TRC (
                    "[APPL]: Sent Error Response with result 0x%04X", retval);
            }
        }
        else
        {
            APPL_ERR (
            "[APPL]: Failed to read local handle 0x%04X. Reason 0x%04X\n",
            attr_handle, retval);
        }
    }

    return API_SUCCESS;
}


/*
 * direction: GATT_DB_PEER_INITIATED or GATT_DB_LOCALLY_INITIATED
 */
API_RESULT appl_handle_write_request
           (
               /* IN */ ATT_HANDLE    * handle,
               /* IN */ UINT16        attr_handle,
               /* IN */ UINT16        offset,
               /* IN */ ATT_VALUE     * value,
               /* IN */ UCHAR        flags
           )
{
    ATT_ERROR_RSP_PARAM            err_param;
    ATT_HANDLE_VALUE_PAIR          pair;
#ifdef ATT_QUEUED_WRITE_SUPPORT
    ATT_PREPARE_WRITE_RSP_PARAM    prepare_write_rsp;
#endif /* ATT_QUEUED_WRITE_SUPPORT */
    API_RESULT                     retval;

    APPL_TRC (
        "[APPL] Write Request from %s for handle 0x%04X of Length %d\n",
        ((GATT_DB_PEER_INITIATED == (flags & GATT_DB_PEER_INITIATED)) ? "Peer" :
        "Local"), attr_handle, value->len);

    appl_dump_bytes (value->val, value->len);

    err_param.handle = attr_handle;
    err_param.op_code = ATT_WRITE_REQ;
    err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;

    pair.value = *value;
    pair.handle = attr_handle;

    retval  = BT_gatt_db_access_handle
              (
                  &pair,
                  handle,
                  offset, /* Offset is zero */
                  (flags)
              );

    if ( GATT_DB_WRITE_WITHOUT_RSP != (GATT_DB_WRITE_WITHOUT_RSP & flags))
    {
        if (API_SUCCESS == retval)
        {
#ifdef ATT_QUEUED_WRITE_SUPPORT
            if (GATT_DB_PREPARE == (flags & GATT_DB_PREPARE))
            {
                err_param.op_code = ATT_PREPARE_WRITE_REQ;
                prepare_write_rsp.offset = offset;
                prepare_write_rsp.handle_value.handle = attr_handle;
                prepare_write_rsp.handle_value.value = (*value);
                retval = appl_att_prepare_write_enqueue (&prepare_write_rsp);
                if (API_SUCCESS == retval)
                {
                    retval = BT_att_send_prepare_write_rsp
                             (
                                  handle,
                                  &prepare_write_rsp
                             );
                }
                else
                {
                    err_param.handle = attr_handle;
                    err_param.error_code = ATT_PREPARE_WRITE_QUEUE_FULL;

                    retval = BT_att_send_error_rsp
                             (
                                 handle,
                                 &err_param
                             );
                }

                APPL_TRC (
                    "[APPL]: Prepare Response sent with result 0x%04X", retval);
            }
            else
#endif /* ATT_QUEUED_WRITE_SUPPORT */
            {
                retval = BT_att_write_rsp
                         (
                              handle
                         );

                APPL_TRC (
                    "[APPL]: Write Response sent with result 0x%04X\n", retval);
            }

        }
        else
        {
#ifdef ATT_QUEUED_WRITE_SUPPORT
            if (GATT_DB_PREPARE == (flags & GATT_DB_PREPARE))
            {
                err_param.op_code = ATT_PREPARE_WRITE_REQ;
            }
#endif /* ATT_QUEUED_WRITE_SUPPORT */

            if (GATT_DB_INVALID_OPERATION == retval)
            {
                err_param.error_code = ATT_WRITE_NOT_PERMITTED;
            }
            else if (GATT_DB_INSUFFICIENT_BUFFER_LEN == retval)
            {
                err_param.error_code = ATT_INVALID_ATTRIBUTE_LEN;
            }
            else if ((GATT_DB_INVALID_ATTR_HANDLE == retval) ||
                (GATT_DB_HANDLE_NOT_FOUND == retval) ||
                (GATT_DB_INVALID_TRANSPORT_ACCESS == retval))
            {
                err_param.error_code = ATT_INVALID_HANDLE;
            }
            else if (GATT_DB_INVALID_OFFSET == retval)
            {
                err_param.error_code = ATT_INVALID_OFFSET;
            }
            else if (GATT_DB_INSUFFICIENT_SECURITY == retval)
            {
                err_param.error_code = ATT_INSUFFICIENT_AUTHENTICATION;
            }
            else if (GATT_DB_INSUFFICIENT_ENC_KEY_SIZE == retval)
            {
                err_param.error_code = ATT_INSUFFICIENT_ENC_KEY_SIZE;
            }
            else if (GATT_DB_INSUFFICIENT_ENCRYPTION == retval)
            {
                err_param.error_code = ATT_INSUFFICIENT_ENCRYPTION;
            }
            else if (GATT_DB_HANDLE_NOT_FOUND == retval)
            {
                err_param.error_code = ATT_INVALID_HANDLE;
            }
            else
            {
                /* This is for Profile specific errors */
                err_param.error_code = (UCHAR) retval;
            }

            if (GATT_DB_DELAYED_RESPONSE != retval)
            {
                /* Send Error Response */
                retval = BT_att_send_error_rsp
                         (
                             handle,
                             &err_param
                         );

                APPL_TRC (
                    "[APPL]: Sent Error Response with result 0x%04X", retval);
            }
        }
    }

    return retval;
}
#ifdef ATT_CLIENT

/**
 *  Function to initiate Service Discovery for a specific service
 *  represented by the 'uuid'.
 *
 *  \param uuid: Universally Unique Identifier (16 or 128 bit).
 *  \param uuid_frmt: Identifier as 16 or 128 bit uuid.
 *
 *  \return   : API_SUCEESS on successful processing of the event.
 *              Appropriate error code otherwise.
 */
API_RESULT appl_discover_service
           (
               /* IN */ ATT_UUID           uuid,
               /* IN */ UCHAR                 uuid_frmt
           )
{
    ATT_FIND_BY_TYPE_VAL_REQ_PARAM    find_by_type_val_pram;
    UCHAR                             value[16];
    API_RESULT                        retval;

    find_by_type_val_pram.range.start_handle = ATT_ATTR_HANDLE_START_RANGE;
    find_by_type_val_pram.range.end_handle = ATT_ATTR_HANDLE_END_RANGE;
    find_by_type_val_pram.uuid = GATT_PRIMARY_SERVICE;

    if ( ATT_16_BIT_UUID_FORMAT == uuid_frmt )
    {
        BT_PACK_LE_2_BYTE (value, &(uuid.uuid_16));

        find_by_type_val_pram.value.val = value;
        find_by_type_val_pram.value.len = 2;
    }
    else if ( ATT_128_BIT_UUID_FORMAT == uuid_frmt )
    {
        BT_PACK_LE_16_BYTE (value, (const void*)(&(uuid.uuid_128)));

        find_by_type_val_pram.value.val = value;
        find_by_type_val_pram.value.len = 16;
    }
    else
    {
        return APPL_INVALID_UUID_FORMAT;
    }

    APPL_GET_CURRENT_DSCVRY_STATE() = APPL_GD_IN_SERV_DSCRVY_STATE;
    APPL_CURRENT_SERVICE_UUID() = uuid;

    retval = BT_att_send_find_by_type_val_req
             (
                  &appl_gatt_client_handle,
                  &find_by_type_val_pram
             );

    return retval;
}


void appl_reset_gatt_dscvry_info (void)
{
    UINT32    index;

    APPL_INIT_GATT_DSCRY_STATE();

    appl_char_descptr_discovery_count = 0;

    index = APPL_MAX_CHARACTERISTICS;
    do
    {
        index--;
        APPL_CHAR_INIT(index);
    } while (index > 0);
}


API_RESULT appl_find_info_req
           (
                ATT_HANDLE_RANGE    * range
           )
{
    API_RESULT    retval;

    APPL_TRC (
        "Initiating Descriptor Discovery from 0x%04X to 0x%04X\n",
        range->start_handle, range->end_handle);

    retval = BT_att_send_find_info_req
             (&appl_gatt_client_handle, range);

    if (API_SUCCESS != retval)
    {
        APPL_ERR ("[***ERR***]: Failed to to initiate Find Information Request,"
        "reason 0x%04X\n", retval);
    }
    else
    {
        APPL_TRC ("Successful sent Find Information Request\n");
    }

    return retval;
}

void appl_initiate_descriptor_discovery (void)
{
    ATT_HANDLE_RANGE   range;
    API_RESULT         retval;

    if (0 != APPL_GET_CHAR_COUNT())
    {
        do
        {
            range.start_handle =
                APPL_CHAR_GET_START_HANDLE(APPL_GET_CURRENT_CHAR_INDEX()) + 1;
            if ((APPL_GET_CURRENT_CHAR_INDEX() + 1) == APPL_GET_CHAR_COUNT())
            {
                range.end_handle = APPL_GET_SER_END_HANDLE();
            }
            else
            {
                range.end_handle =
                   (APPL_CHAR_GET_START_HANDLE(APPL_GET_CURRENT_CHAR_INDEX() + 1) - 1);
            }

            /**
             *  Characteristic array maintained by the application has the start
             *  handle initialized to Characteristic Value handle and not to
             *  Characteristic definition. Therefore, if between start handle
             *  and end handle (dervied from next characteristic's value
             *  handle), there exists only one attribute indicated by
             *  start_handle = end_handle, that one attribute can be only the
             *  Characteristic definition. However, the only one place where
             *  this funda does not apply is when it is the last characteristic,
             *  and end handle has been initialized to service end handle. Here
             *  start_handle = end_handle implies a definite descriptor.
             */
            if ((range.start_handle < range.end_handle) ||
               ((range.start_handle == range.end_handle) &&
               (range.end_handle == APPL_GET_SER_END_HANDLE())))
            {
                retval = appl_find_info_req (&range);
                if ( API_SUCCESS == retval )
                {
                    APPL_GET_CURRENT_DSCVRY_STATE() = APPL_GD_IN_DESC_DSCVRY_STATE;
                    break;
                }
            }
            APPL_GET_CURRENT_CHAR_INDEX() ++;
        }while (APPL_GET_CURRENT_CHAR_INDEX() < APPL_GET_CHAR_COUNT());

        if (APPL_GET_CURRENT_CHAR_INDEX() == APPL_GET_CHAR_COUNT())
        {
            if (0 == appl_char_descptr_discovery_count)
            {
                APPL_TRC (
                    "Search Complete, Service, Characteristic Discovered, "
                    "No Descriptors Found!\n");
            }
            else
            {
                APPL_TRC (
                    "Search Complete, Service, Characteristic & "
                    "Descriptors Discovered!!\n");
            }

            /* Reset GATT Discovery infomation */
            appl_reset_gatt_dscvry_info ();
        }
    }
    else
    {
        APPL_TRC (
            "Search Complete, No Characteristic Found!\n");

        /* Reset GATT Discovery infomation */
        appl_reset_gatt_dscvry_info ();
    }
}

void appl_handle_find_info_response (UCHAR * list, UINT16 length, UCHAR type)
{
    UCHAR               uuid_arr[16];
    ATT_HANDLE_RANGE    range;
    UINT32              index;
    UINT16              handle;
    UINT16              uuid;
    API_RESULT          retval;

    if ( 0 < length )
    {
        if ( 0x01 == type )
        {
            APPL_TRC ("\n\n16 bit UUIDs : \n");
            for ( index = 0; index < length; index = index + 4 )
            {
                BT_UNPACK_LE_2_BYTE(&handle, (list + index));
                BT_UNPACK_LE_2_BYTE(&uuid, (list + index + 2));
                APPL_TRC ("Handle : %04X -> ", handle);
                APPL_TRC ("UUID   : %04X \n", uuid);
                appl_rcv_service_desc (uuid, APPL_CHAR_GET_UUID(APPL_GET_CURRENT_CHAR_INDEX()), handle);
            }
        }
        else if ( 0x02 == type )
        {
            for ( index = 0; index < length; index = index + 18 )
            {
                APPL_TRC("\n\n128 bit UUIDs : \n");
                BT_UNPACK_LE_2_BYTE(&handle, (list + index));
                ATT_UNPACK_UUID(uuid_arr, (list + index + 2), 16);
                APPL_TRC ("Handle : %04X\n", handle);
                APPL_TRC ("UUID   : ");

                appl_dump_bytes(uuid_arr, 16);
            }
        }
        else
        {
            /* TODO: Check if this is correct interpretaion */
            APPL_TRC ("List of handles corrosponding to the Req UUID:\n");

            for ( index = 0; index < length; index += index )
            {

                BT_UNPACK_LE_2_BYTE(&handle, list + index);
                APPL_TRC ("Handle : %04X -> \n", handle);
            }
        }
    }

    /**
     * Continue Descriptor Discovery for Existig/Next Characteristic.
     */
    if ((APPL_GET_CURRENT_CHAR_INDEX() + 1) == APPL_GET_CHAR_COUNT())
    {
        range.end_handle = APPL_GET_SER_END_HANDLE();
    }
    else
    {
        range.end_handle =
           (APPL_CHAR_GET_START_HANDLE(APPL_GET_CURRENT_CHAR_INDEX() + 1) -1);
    }

    if (handle < range.end_handle)
    {
        /* More descriptors to be discovered */
        range.start_handle = handle + 1;
        retval = appl_find_info_req (&range);
        if (API_SUCCESS == retval)
        {
            APPL_TRC (
                "[APPL]: Follow-up Find Info Request to discover more "
                "characteristics\n");
        }
        else
        {
            APPL_ERR (
                "[APPL]: Failed to initiate follow request, reason 0x%04X\n", retval);
        }
    }
    else
    {
        if ((APPL_GET_CURRENT_CHAR_INDEX() + 1) == APPL_GET_CHAR_COUNT())
        {
            /* Discovery Complete */
            APPL_TRC (
                "[APPL]: Search Complete, Service, Characteristic & Descriptors Discovered!!!!\n");

            /* Reset GATT Discovery infomation */
            appl_reset_gatt_dscvry_info ();
            appl_search_complete();
        }
        else
        {
            appl_char_descptr_discovery_count ++;
            APPL_GET_CURRENT_CHAR_INDEX() ++;
            appl_initiate_descriptor_discovery ();
        }
    }
}

API_RESULT appl_att_read_by_type
     (
         /* IN */ ATT_HANDLE_RANGE * range,
         /* IN */ UINT16 uuid
     )
{
    ATT_READ_BY_TYPE_REQ_PARAM    read_by_type_req_param;
    API_RESULT   retval;

    APPL_TRC (
        "Searching for UUID 0x%04X, from handle 0x%04X to 0x%04X\n",
        uuid, range->start_handle, range->end_handle);

    read_by_type_req_param.range = *range;
    ATT_SET_16_BIT_UUID(&read_by_type_req_param.uuid, uuid);
    read_by_type_req_param.uuid_format = ATT_16_BIT_UUID_FORMAT;

    if (GATT_CHARACTERISTIC == uuid)
    {
        APPL_GET_CURRENT_DSCVRY_STATE() = APPL_GD_IN_CHAR_DSCVRY_STATE;
    }

    retval = BT_att_send_read_by_type_req
             (
                 &appl_gatt_client_handle,
                 &read_by_type_req_param
             );

    APPL_TRC(
        "[ATT]: Read by Type Request Initiated with result 0x%04X", retval);
    return retval;
}
API_RESULT appl_read_req
           (
                /* IN */ ATT_ATTR_HANDLE    handle
           )
{
    API_RESULT retval;

    retval = BT_att_send_read_req
             (
                 &appl_gatt_client_handle,
                 &handle
             );

    APPL_TRC (
        "[APPL]: Initiated Read Request with result 0x%04X\n", retval);

    return retval;
}

void appl_handle_find_by_type_val_rsp (UCHAR * data, UINT16 datalen)
{
    ATT_HANDLE_RANGE range;

    APPL_TRC ("Received Find By Type Value Response\n");
    appl_dump_bytes (data, datalen);

    /* For DIS, there should be only one Service */
    if (4 == datalen)
    {
        APPL_TRC ("Start Handle        End Handle\n");

        BT_UNPACK_LE_2_BYTE(&service_start_handle, data);
        APPL_TRC ("0x%04X", service_start_handle);
        BT_UNPACK_LE_2_BYTE(&service_end_handle, (data + 2));
        APPL_TRC ("              0x%04X\n\n", service_end_handle);

        range.start_handle = service_start_handle;
        range.end_handle = service_end_handle;
        APPL_GET_SER_END_HANDLE() = service_end_handle;
        APPL_GET_CHAR_COUNT() = 0;

        appl_att_read_by_type
        (
            &range,
            GATT_CHARACTERISTIC
        );
    }
    else
    {
        APPL_TRC ("**** Service Found More Than Once. FAILED ****\n");
    }
}

void appl_print_char_property (UCHAR property)
{
    APPL_TRC ("Characteristic Property 0x%02X\n", property);

    if (GATT_CH_PROP_BIT_BROADCAST == (GATT_CH_PROP_BIT_BROADCAST & property))
    {
        APPL_TRC ("Broadcast Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Broadcast Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_READ == (GATT_CH_PROP_BIT_READ & property))
    {
        APPL_TRC ("Read Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Read Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_WRITE_WO_RSP == (GATT_CH_PROP_BIT_WRITE_WO_RSP & property))
    {
        APPL_TRC ("Write Without Response Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Write Without Response Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_WRITE == (GATT_CH_PROP_BIT_WRITE & property))
    {
        APPL_TRC ("Write Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Write Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_NOTIFY == (GATT_CH_PROP_BIT_NOTIFY & property))
    {
        APPL_TRC ("Notify Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Notify Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_INDICATE == (GATT_CH_PROP_BIT_INDICATE & property))
    {
        APPL_TRC ("Indicate Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Indicate Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_SIGN_WRITE == (GATT_CH_PROP_BIT_SIGN_WRITE & property))
    {
        APPL_TRC ("Authenticated Signed Write Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Authenticated Signed Write Permitted: No\n");
    }

    if (GATT_CH_PROP_BIT_EXT_PROPERTY == (GATT_CH_PROP_BIT_EXT_PROPERTY & property))
    {
        APPL_TRC ("Extended Properties Permitted: Yes\n");
    }
    else
    {
        APPL_TRC ("Extended Properties Permitted: No\n");
    }
}

void appl_handle_read_by_type_response
     (
         /* IN */ UINT16    data_elem_size,
         /* IN */ UCHAR   * data,
         /* IN */ UINT16    data_len
     )
{
    ATT_HANDLE_RANGE range;
    UINT32 index;
    UINT16 handle;
    UINT16 diff_in_size;
    UCHAR  * value;
    UCHAR  search_again;

    UINT16 value_handle;
    UCHAR  c_property;
    ATT_UUID uuid;
    UCHAR  i;
    UINT16 att_mtu = ATT_DEFAULT_MTU;

    BT_att_access_mtu(&appl_gatt_client_handle, &att_mtu);

    search_again = 0;

    /**
     * Check if the response received was smaller than max MTU, then no further
     * searching is needed
     */
    diff_in_size = att_mtu - data_len -1;
    if (0 == diff_in_size || ((0 != diff_in_size)
       && (diff_in_size < data_elem_size)))
    {
        /**
         * There could be more requests, as the MTU has been optimally utilized
         */
        search_again = 1;
    }

    for (index = 0; index < (unsigned)(data_len / (data_elem_size)); index++ )
    {
        BT_UNPACK_LE_2_BYTE(&handle, (data + (index * data_elem_size)));
        APPL_TRC ("Handle - 0x%04X\n", handle);
        APPL_TRC ("Handle Value Received - \n");
        value = data + (2 + index * data_elem_size);
        appl_dump_bytes(value, data_elem_size-2);

        if (APPL_GD_IN_CHAR_DSCVRY_STATE == APPL_GET_CURRENT_DSCVRY_STATE())
        {
            /* Extract Property */
            c_property = value[0];

            BT_UNPACK_LE_2_BYTE
            (
                &value_handle,
                value + 1
            );

        if ( data_elem_size > 7 )
        {
                BT_UNPACK_LE_16_BYTE
            (
                &(uuid.uuid_128.value[0]),
                value + 3
            );
        }
        else
        {
            BT_UNPACK_LE_2_BYTE
            (
                &(uuid.uuid_16),
                value + 3
            );
        }

            APPL_TRC ("----\n");
            APPL_TRC ("Value Handle 0x%04X, Property 0x%02X, UUID 0x%04X\n",
                value_handle, c_property, uuid.uuid_16);

            appl_print_char_property (c_property);

        if ( data_elem_size > 7 )
        {
            APPL_TRC ("UUID: ");
            for ( i = 0; i < 16; i++ )
            {
                APPL_TRC (" 0x%02X,", uuid.uuid_128.value[i]);
            }
            APPL_TRC ("\n");
        }
        else
        {
            APPL_TRC ("UUID 0x%04X ", uuid.uuid_16);
        }
        appl_rcv_service_char (uuid, value_handle);
        }
        APPL_CHAR_GET_START_HANDLE (APPL_GET_CHAR_COUNT()) = value_handle;
        APPL_CHAR_GET_UUID (APPL_GET_CHAR_COUNT()) = uuid;
        APPL_GET_CHAR_COUNT()++;
    }

    if (1 == search_again)
    {
        if (value_handle + 1 < service_end_handle)
        {
            range.start_handle = value_handle + 1;
            range.end_handle = service_end_handle;

            appl_att_read_by_type
            (
                &range,
                GATT_CHARACTERISTIC
            );
        }
        else
        {
            search_again = 0;
        }
    }
    if (0 == search_again)
    {
        /* Initiate Descriptor Discovery */
        APPL_GET_CURRENT_CHAR_INDEX() = 0;
        appl_initiate_descriptor_discovery ();
    }
}

API_RESULT appl_write_req
           (
                /* IN */ ATT_ATTR_HANDLE     handle,
                /* IN */ ATT_VALUE           * value
           )
{
    ATT_WRITE_REQ_PARAM write_req_param;
    API_RESULT retval;

    write_req_param.handle = handle;
    write_req_param.value = (*value);

    retval = BT_att_send_write_req
             (
                 &appl_gatt_client_handle,
                 &write_req_param
             );

    APPL_TRC (
        "[APPL]: ATT Write Request Initiated with retval 0x%04X\n",
        retval);

    return retval;
}

API_RESULT appl_write_cmd
           (
                /* IN */ ATT_ATTR_HANDLE     handle,
                /* IN */ ATT_VALUE           * value
           )
{
    ATT_WRITE_CMD_PARAM write_cmd_param;
    API_RESULT retval;

    write_cmd_param.handle = handle;
    write_cmd_param.value = (*value);

    retval = BT_att_send_write_cmd
             (
                 &appl_gatt_client_handle,
                 &write_cmd_param
             );

    APPL_TRC (
        "[APPL]: ATT Write Command Initiated with retval 0x%04X\n",
        retval);

    return retval;
}

#endif /* ATT_CLIENT */

API_RESULT appl_att_cb
           (
               ATT_HANDLE    * handle,
               UCHAR         att_event,
               API_RESULT    event_result,
               UCHAR         * event_data,
               UINT16        event_datalen
           )
{
    ATT_ERROR_RSP_PARAM       err_param;
    ATT_XCHG_MTU_RSP_PARAM    xcnhg_rsp_param;
    ATT_HANDLE_RANGE          range;
    ATT_HANDLE_LIST           list;
    ATT_VALUE                 uuid;
    UINT16                    attr_handle_list[APPL_MAX_MULTIPLE_READ_COUNT];
    ATT_ATTR_HANDLE           attr_handle;
    ATT_VALUE                 value;
    APPL_EVENT_PARAM          fsm_param;
    UINT16                    offset;
    UINT16                    mtu;
    API_RESULT                retval;
    UCHAR                     send_err;

    APPL_TRC
        ("\n[ATT]:[0x%02X]: Received event 0x%02X with result 0x%04X\n",
        handle->device_id, att_event, event_result);

    appl_recvice_att_event(att_event, event_result);

    retval = API_SUCCESS;
    err_param.handle = ATT_INVALID_ATTR_HANDLE_VAL;
    err_param.op_code = att_event;
    err_param.error_code = ATT_REQUEST_NOT_SUPPORTED;
    send_err = BT_FALSE;
    APPL_EVENT_PARAM_INIT(&fsm_param);

    /* Update ATT status to be busy */
    BT_status_set_bit (STATUS_BIT_ATT_BUSY, STATUS_BIT_SET);

#ifdef APPL_VALIDATE_ATT_PDU_LEN
    /* Validate ATT PDU Request length */
    if (BT_FALSE != APPL_VALID_ATT_EVENT_FOR_LEN_CHK(att_event))
    {
        retval = appl_validate_att_pdu_req_len(att_event, event_datalen);

        if (API_FAILURE == retval)
        {
            err_param.error_code = ATT_INVALID_PDU;
            retval = BT_att_send_error_rsp (handle, &err_param);
            return retval;
        }
    }
#endif /* APPL_VALIDATE_ATT_PDU_LEN */

    appl_dump_bytes (event_data, event_datalen);

    switch (att_event)
    {
        case ATT_CONNECTION_IND:
            if (API_SUCCESS == event_result)
            {
                /* Cache the ATT Handle into the Global maintained for Client */
                appl_gatt_client_handle = *handle;

                appl_add_device(handle, &fsm_param.handle);

                /* Post Disconnection Event */
                fsm_post_event
                (
                    APPL_FSM_ID,
                    ev_appl_transport_connected_ind,
                    (void *)(&fsm_param)
                );

                /* Post Transport Operating Inciation */
                /**
                 *  Please note that in case of BLE Transport Connect and Transport
                 *  Operating mean the same. But this may not be true in case of
                 *  BR/EDR or someother transport for which this application could
                 *  be used.
                 */
                fsm_post_event
                (
                    APPL_FSM_ID,
                    ev_appl_transport_operating_ind,
                    (void *)(&fsm_param)
                );
            }
            break;

        case ATT_DISCONNECTION_IND:
            appl_gatt_client_handle.att_id = 0xFF;
            appl_gatt_client_handle.device_id = 0xFF;
            appl_get_handle_from_device_handle (handle->device_id, &fsm_param.handle);
            fsm_post_event
            (
                APPL_FSM_ID,
                ev_appl_transport_suspend_ind,
                (void *)(&fsm_param)
            );
            APPL_TRC (
                "Received ATT Disconnection Indication with result 0x%04X",
                event_result);
            BT_gatt_db_peer_session_shutdown_handler (handle);
            break;

        case ATT_XCNHG_MTU_REQ:
            if (ATT_INVALID_PARAMETER == event_result)
            {
                /* Error Response */
                /**
                 *  In case application does not support setting any value other
                 *  than default 23, an error respone with request not supported
                 *  would be more appropriate. This should be sent irrespective
                 *  of what event result is.
                 *  But in case it does support the request, but MTU requested is
                 *  detected to be invalid, response code, 'Invalid PDU' seems to
                 *  be most fitting currently for Error Response.
                 */
                err_param.error_code = ATT_INVALID_PDU;
                err_param.handle = 0x0000;
                err_param.op_code = ATT_XCNHG_MTU_REQ;
                retval = BT_att_send_error_rsp (handle, &err_param);
                APPL_ERR (
                "Invalid MTU Size received in Xchng MTU Req; "
                "Error Response sent with result %04X!\n",
                retval);
            }
            else if (API_SUCCESS == retval)
            {
                BT_UNPACK_LE_2_BYTE(&mtu, event_data);
                APPL_TRC (
                    "Received Exchange MTU Request with MTU Size "
                    "= 0x%04X!\n", mtu);

                xcnhg_rsp_param.mtu = APPL_ATT_MTU;

                retval = BT_att_send_mtu_xcnhg_rsp
                         (
                             handle,
                             &xcnhg_rsp_param
                         );

                APPL_TRC (
                    "Sent Response with retval 0x%04X\n", retval);

                if (API_SUCCESS == retval)
                {
                    UINT16 t_mtu;

                    appl_get_handle_from_device_handle (handle->device_id, &fsm_param.handle);

                    retval = BT_att_access_mtu
                             (
                                 handle,
                                 &t_mtu
                             );

                    if (API_SUCCESS == retval)
                    {
                        /* Save the Negotiated mtu to global */
                        APPL_SET_MTU(fsm_param.handle, t_mtu);

                        APPL_PROFILE_MTU_UPDT_COMPLETE_HANDLER
                        (
                            &fsm_param.handle,
                            t_mtu
                        );
                    }
                }
            }
            break;

        case ATT_FIND_INFO_REQ:
            APPL_TRC (
                "Received Find Info Request with result 0x%04X\n", event_result);

            if (API_SUCCESS == event_result)
            {
                offset = 0;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.start_handle,
                     event_data + offset
                );

                offset += 2;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.end_handle,
                     event_data + offset
                );

                if (ATT_CHECK_VALID_HANDLE_RANGE\
                (range.start_handle, range.end_handle))
                {
                    err_param.error_code = ATT_INVALID_HANDLE;
                    send_err = BT_TRUE;
                }
                else
                {
                    appl_handle_find_info_request (handle, &range);
                }
            }
            break;

        case ATT_FIND_BY_TYPE_VAL_REQ:
            APPL_TRC (
                "Received Find By Type Value Request with result 0x%04X\n",
                event_result);

            if (API_SUCCESS == event_result)
            {
                offset = 0;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.start_handle,
                     event_data + offset
                );

                offset += 2;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.end_handle,
                     event_data + offset
                );

                if (ATT_CHECK_VALID_HANDLE_RANGE\
                (range.start_handle, range.end_handle))
                {
                    err_param.error_code = ATT_INVALID_HANDLE;
                    send_err = BT_TRUE;
                    break;
                }

                offset += 2;
                uuid.len = ATT_16_BIT_UUID_SIZE;
                uuid.val = event_data + offset;

                offset += 2;
                value.len = event_datalen - offset;
                value.val = event_data + offset;

                if ((BT_FALSE != GATT_VERIFY_UUID_VALUE(uuid.val, GATT_PRIMARY_SERVICE_TYPE_OFFSET, ATT_16_BIT_UUID_SIZE)) ||\
                   (BT_FALSE != GATT_VERIFY_UUID_VALUE(uuid.val, GATT_SECONDARY_SERVICE_TYPE_OFFSET, ATT_16_BIT_UUID_SIZE)))
                {
                    /**
                     * For Service Type, only permissible values are 16 bit UUID or
                     * 128 bit UUID. Hence the following checks
                     */
                    if (ATT_128_BIT_UUID_SIZE == value.len)
                    {
                        retval =  BT_gatt_db_get_16_bit_uuid(&value, &value);
                        if (retval != API_SUCCESS)
                        {
#ifndef GATT_DB_SUPPORT_128_BIT_UUID
                            err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
                            send_err = BT_TRUE;
#endif /* GATT_DB_SUPPORT_128_BIT_UUID */
                        }
                    }
                    else if (ATT_16_BIT_UUID_SIZE != value.len)
                    {
                        /* Malformed PDU */
                        err_param.error_code = ATT_INVALID_ATTRIBUTE_LEN;
                        send_err = BT_TRUE;
                    }
                }

                if (BT_FALSE == send_err)
                {
                    appl_handle_find_by_type_val_request
                    (
                        handle,
                        &range,
                        &value,
                        &uuid
                    );
                }
            }
            break;

        case ATT_READ_BY_TYPE_REQ:
            if (API_SUCCESS == event_result)
            {
                offset = 0;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.start_handle,
                     event_data + offset
                );

                offset += 2;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.end_handle,
                     event_data + offset
                );

                if (ATT_CHECK_VALID_HANDLE_RANGE\
                (range.start_handle, range.end_handle))
                {
                    err_param.error_code = ATT_INVALID_HANDLE;
                    err_param.handle = range.start_handle;
                    send_err = BT_TRUE;
                    break;
                }

                offset += 2;
                uuid.val = event_data + offset;
                uuid.len = event_datalen - offset;

                if (ATT_128_BIT_UUID_SIZE == uuid.len)
                {
                    retval = BT_gatt_db_get_16_bit_uuid(&uuid, &uuid);
                    if (API_SUCCESS != retval)
                    {
#ifndef GATT_DB_SUPPORT_128_BIT_UUID
                        err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
                        send_err = BT_TRUE;
#endif /* GATT_DB_SUPPORT_128_BIT_UUID */
                    }
                }
                else if (ATT_16_BIT_UUID_SIZE != (event_datalen - offset))
                {
                    /* Incorrect PDU */
                    err_param.error_code = ATT_INVALID_ATTRIBUTE_LEN;
                    send_err = BT_TRUE;
                    retval = API_FAILURE;
                }

                if (BT_FALSE == send_err)
                {
                    appl_handle_read_by_type_request
                    (
                        handle,
                        &range,
                        &uuid
                    );
                }
            }
            break;

        case ATT_READ_REQ:
            if (API_SUCCESS == event_result)
            {
                BT_UNPACK_LE_2_BYTE
                (
                    &attr_handle,
                    event_data
                );

                appl_handle_read_request
                (
                     handle,
                     attr_handle,
                     APPL_ATT_INVALID_OFFSET,
                     GATT_DB_PEER_INITIATED
                );
            }
            break;

        case ATT_READ_BLOB_REQ:
            if (API_SUCCESS == event_result)
            {
                BT_UNPACK_LE_2_BYTE
                (
                    &attr_handle,
                    event_data
                );

                BT_UNPACK_LE_2_BYTE
                (
                    &offset,
                    event_data + 2
                );
                appl_handle_read_request
                (
                     handle,
                     attr_handle,
                     offset,
                     GATT_DB_PEER_INITIATED
                );
            }
            break;

        case ATT_READ_MULTIPLE_REQ:
        {
            APPL_TRC (
                "Received Read By Multiple Request with result 0x%04X\n",
                event_result);
            list.list_count = 0;
            do
            {
                BT_UNPACK_LE_2_BYTE
                (
                    &attr_handle_list[list.list_count],
                    (event_data + (list.list_count * 2))
                );
                list.list_count++;
            }while (event_datalen > (list.list_count * 2));

            list.handle_list = attr_handle_list;

            appl_handle_read_multiple_request (handle, &list);
        }
        break;

        case ATT_READ_BY_GROUP_REQ:
        {
            APPL_TRC (
                "Received Read By Group Type Request with result 0x%04X\n",
                event_result);
            if (API_SUCCESS == event_result)
            {
                offset = 0;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.start_handle,
                     event_data + offset
                );

                offset += 2;

                BT_UNPACK_LE_2_BYTE
                (
                     &range.end_handle,
                     event_data + offset
                );

                if (ATT_CHECK_VALID_HANDLE_RANGE\
                (range.start_handle, range.end_handle))
                {
                    err_param.error_code = ATT_INVALID_HANDLE;
                    send_err = BT_TRUE;
                    break;
                }

                offset += 2;
                uuid.val = event_data + offset;
                uuid.len = event_datalen - offset;

                if (ATT_128_BIT_UUID_SIZE == uuid.len)
                {
                    retval = BT_gatt_db_get_16_bit_uuid(&uuid, &uuid);
                    /**
                     *  Note, 128-bit UUID Group Types are not supported by the
                     *  Data Base, hence Queries with Non BT Space 128 bit will
                     *  be responded with error
                     */
                    if (API_SUCCESS != retval)
                    {
                        err_param.error_code = ATT_ATTRIBUTE_NOT_FOUND;
                        send_err = BT_TRUE;
                    }
                }
                else if (ATT_16_BIT_UUID_SIZE != (event_datalen - offset))
                {
                    /* Incorrect PDU */
                    err_param.error_code = ATT_INVALID_ATTRIBUTE_LEN;
                    send_err = BT_TRUE;
                }
                else
                {
                    /* Do Nothing */
                }

                if (API_SUCCESS == retval)
                {
                    appl_handle_read_by_group_type_request
                    (
                        handle,
                        &range,
                        &uuid
                    );
                }
            }
        }
        break;

        case ATT_WRITE_REQ:
            if (API_SUCCESS == event_result)
            {
                BT_UNPACK_LE_2_BYTE
                (
                    &attr_handle,
                    event_data
                );

                value.len = event_datalen - 2;
                value.val = appl_att_buffer;
                BT_mem_copy
                (
                    value.val,
                    event_data + 2,
                    value.len
                );
                appl_handle_write_request
                (
                    handle,
                    attr_handle,
                    0,
                    &value,
                    (GATT_DB_PEER_INITIATED | GATT_DB_WRITE)
                );
            }
            break;

        case ATT_WRITE_CMD:
            if (API_SUCCESS == event_result)
            {
                BT_UNPACK_LE_2_BYTE
                (
                    &attr_handle,
                    event_data
                );

                value.len = event_datalen - 2;
                value.val = appl_att_buffer;
                BT_mem_copy
                (
                    value.val,
                    event_data + 2,
                    value.len
                );

                appl_handle_write_request
                (
                    handle,
                    attr_handle,
                    0,
                    &value,
                    ( GATT_DB_PEER_INITIATED | GATT_DB_WRITE_WITHOUT_RSP)
                );

                APPL_TRC (
                    "[APPL] Received Write Command for Handle 0x%04X of Length %d\n",
                    attr_handle, event_datalen - 2);

                appl_dump_bytes (event_data + 2, event_datalen - 2);
            }
            break;

#ifdef ATT_QUEUED_WRITE_SUPPORT
        case ATT_PREPARE_WRITE_REQ:
        {
            BT_UNPACK_LE_2_BYTE
            (
                &attr_handle,
                event_data
            );

            BT_UNPACK_LE_2_BYTE
            (
                &offset,
                event_data + 2
            );

            value.len = event_datalen - 4;
            value.val = appl_att_buffer;
            BT_mem_copy
            (
                value.val,
                event_data + 4,
                value.len
            );

            appl_handle_write_request
            (
                handle,
                attr_handle,
                offset,
                &value,
                ( GATT_DB_PEER_INITIATED | GATT_DB_WRITE | GATT_DB_PREPARE)
            );
        }
        break;

        case ATT_EXECUTE_WRITE_REQ:
            if (NULL != event_data)
            {
                APPL_TRC  (
                    "[APPL]: ATT Execute Write Param 0x%02X", event_data[0]);

                if (0x02 > event_data[0])
                {
                    appl_execute_write_handler[event_data[0]](handle);
                }
            }
            else
            {
                /* TBD: Verify appropriate code for this */
                err_param.error_code = ATT_INVALID_PDU;
                send_err = BT_TRUE;
            }
            break;
#else /* ATT_QUEUED_WRITE_SUPPORT */
        case ATT_PREPARE_WRITE_REQ: /* Fall through */
        case ATT_EXECUTE_WRITE_REQ: /* Fall through */
            appl_handle_unsupported_op_code (handle, att_event);
            break;
#endif /* ATT_QUEUED_WRITE_SUPPORT */
    case ATT_HANDLE_VALUE_CNF:
        appl_hvc_flag = BT_FALSE;
        appl_dump_bytes(event_data, event_datalen);
        appl_get_handle_from_device_handle (handle->device_id, &fsm_param.handle);
        APPL_PROFILE_HVN_IND_COMPLETE_HANDLER
        (
            &fsm_param.handle,
            event_data,
            event_datalen
        );
        break;
    case ATT_SIGNED_WRITE_CMD:
        appl_dump_bytes(event_data, event_datalen);

#ifdef SMP_DATA_SIGNING
            {
                s_data = BT_alloc_mem (event_datalen + 1);
                s_datalen = event_datalen + 1;

                s_data[0] = ATT_SIGNED_WRITE_CMD;
                BT_mem_copy (&s_data[1], event_data, event_datalen);

                appl_signed_write_server_handle = *handle;

                appl_smp_verify_sign_data (s_data, s_datalen);
            }
#else /* SMP_DATA_SIGNING */
            appl_handle_unsupported_op_code (handle, att_event);
#endif /* SMP_DATA_SIGNING */

            break;

#ifdef ATT_CLIENT
        case ATT_XCHNG_MTU_RSP:
            if (NULL == event_data)
            {
                break;
            }

            BT_UNPACK_LE_2_BYTE(&mtu, event_data);
            APPL_TRC (
                "Received Exchange MTU Response with Result 0x%04X. MTU Size "
                "= 0x%04X!\n", event_result, mtu);
            break;

        case ATT_READ_BY_TYPE_RSP:
            APPL_TRC (
            "Received Read BY Type Response with Result 0x%04X!\n",
            event_result);
            if (NULL == event_data)
            {
                break;
            }
            appl_handle_read_by_type_response
            (
                event_data[0],
                &event_data[1],
                event_datalen--
            );
            break;

        case ATT_FIND_INFO_RSP:
            APPL_TRC(
                "Received Find Information Response Opcode!\n");

            if (NULL == event_data)
            {
                break;
            }

            appl_handle_find_info_response
            (
                &event_data[1],
                event_datalen-1,
                event_data[0]
            );
            break;

        case ATT_FIND_BY_TYPE_VAL_RSP:
            APPL_TRC ("Received Find by Type Value Response Opcode!\n");
            if (NULL == event_data)
            {
                break;
            }
            /**
             * Received response contains less than the maximum possible
             * number of handle range that can be packed in the response.
             *
             * This is observed, as a BLE device will have only one GAP service.
             */
            appl_handle_find_by_type_val_rsp (event_data, event_datalen);

            break;

        case ATT_HANDLE_VALUE_NTF:
#ifndef APPL_MENU_OPS
            APPL_TRC ("Received HVN\n");
            BT_UNPACK_LE_2_BYTE(&attr_handle, event_data);
            APPL_TRC ("Handle - 0x%04X\n", attr_handle);
            APPL_TRC ("Handle Value Received - \n");
            appl_dump_bytes(event_data + 2, (event_datalen - 2));
            APPL_TRC ("\n");
#else
            BT_UNPACK_LE_2_BYTE(&attr_handle, event_data);
            APPL_TRC("Handle - 0x%04X\r\n", attr_handle);
            APPL_TRC("Handle Value Received - 0x%02X 0x%02X 0x%02X 0x%02X\r\n"
            , event_data[2], event_data[3], event_data[4], event_data[5]);
#endif /* APPL_MENU_OPS */
                    AncsNotificationCb(event_data, event_datalen);
            break;

#endif /* ATT_CLIENT */

        case ATT_UNKNOWN_PDU_IND:
            /**
             * Check if request, then send an error response. Else, drop.
             * Assumption here is that future Client Initiated Request and server
             * response will follow the current odd & even pattern.
             * Please note that currently handle value confirmation is also an odd
             * value, which then belongs to a request category, so any such future
             * client initiated confirmations will be responded with error, which
             * may be considered a violation as application would be responding to
             * a response.
             */
            if (BT_FALSE != APPL_CHECK_IF_ATT_REQUEST(event_data[0]))
            {
                appl_handle_unsupported_op_code (handle, event_data[0]);
            }
            else
            {
                APPL_ERR (
                    "[APPL]:[*** ERR ****]: Received Unknown PDU (Response Op code "
                    "0x%02X?) from Client!\n", event_data[0]);
                err_param.error_code = ATT_INVALID_PDU;
                err_param.op_code = event_data[0];
                err_param.handle = ATT_INVALID_ATTR_HANDLE_VAL;

                send_err = BT_TRUE;
            }
            break;

        /**
         * Typically ATT Server Shall not receive an ATT_HANDLE_VALUE_IND as it
         * will not configure the Peer CCCD for Service Changed indications.
         * It is observed that few popular Mobile Phones/OS/Apps incorrectly
         * sendout ATT_HANDLE_VALUE_IND after LL connection.
         * The following code has been added to tackle this erronous behaviour to
         * avoid IoP issues.
         * The application, may choose to have a counter to blacklist the device
         * if this option is used by peer device to cause DoS attacks, where
         * in repeated unsupported PDUs are sent.
         */
        case ATT_HANDLE_VALUE_IND:
#ifdef ATT_CLIENT
            APPL_TRC ("Received HVI\n");
            BT_UNPACK_LE_2_BYTE(&attr_handle, event_data);
            APPL_TRC ("Handle - 0x%04X\n", attr_handle);
            APPL_TRC ("Handle Value Received - \n");
            appl_dump_bytes(event_data + 2, (event_datalen-2));

            retval = BT_att_send_hndl_val_cnf (&appl_gatt_client_handle);
            break;
#else
            BT_UNPACK_LE_2_BYTE
            (
                &attr_handle,
                event_data
            );
            err_param.error_code = ATT_REQUEST_NOT_SUPPORTED;
            err_param.op_code = ATT_HANDLE_VALUE_IND;
            err_param.handle = attr_handle;

            send_err = BT_TRUE;
            break;
#endif /* ATT_CLIENT */
    case ATT_HANDLE_VALUE_NTF_TX_COMPLETE:
        APPL_TRC("Received HVN Tx Complete (Locally generated)\n");
        appl_dump_bytes(event_data, event_datalen);
        appl_get_handle_from_device_handle (handle->device_id, &fsm_param.handle);
        APPL_PROFILE_HVN_NTF_COMPLETE_HANDLER
        (
            &fsm_param.handle,
            event_data,
            event_datalen
        );
        break;

        default:
            break;
    }

    if (BT_FALSE != send_err)
    {
         retval = BT_att_send_error_rsp
                  (
                      handle,
                      &err_param
                  );
         APPL_ERR (
            "[APPL]: Error Response sent with result 0x%04X\n", retval);
    }

    /* Update ATT status to be free */
    BT_status_set_bit (STATUS_BIT_ATT_BUSY, STATUS_BIT_RESET);

    return API_SUCCESS;
}

#ifdef SMP_DATA_SIGNING
void appl_gatt_signing_verification_complete
     (
         API_RESULT status,
         UCHAR * data,
         UINT16 datalen
     )
{
    ATT_ATTR_HANDLE attr_handle;
    ATT_VALUE value;

    if (API_SUCCESS == status)
    {
        /* Extract handle */
        BT_UNPACK_LE_2_BYTE
        (
            &attr_handle,
            (s_data + 1)
        );

        /* Extract value */
        /**
         * ATT SIGNED WRITE DATA comprises of
         *    1. ATT_SIGNED_WRITE_CMD OPCODE    (1 byte)
         *    2. Attribute Handle               (2 bytes)
         *    3. Attribute Value                (N bytes)
         *    4. Signed write Counter           (4 bytes)
         *    5. MAC                            (8 bytes)
         *    thus Attribute Value len(N) = Total Len - (1 + 2 + 4 + 8)
         */
        value.len = (s_datalen - (1 + 2 + sizeof(UINT32) + SMP_MAC_SIZE));
        value.val = appl_att_buffer;

        BT_mem_copy
        (
            value.val,
            (s_data + 3),
            value.len
        );

        appl_handle_write_request
        (
            &appl_signed_write_server_handle,
            attr_handle,
            0,
            &value,
            (GATT_DB_PEER_INITIATED | GATT_DB_WRITE_WITHOUT_RSP)
        );
    }
    else
    {
        /* Data signing verification procedure failed */
    }

    if (NULL != s_data)
    {
        BT_free_mem (s_data);
        s_data = NULL;
    }
}
#endif /* SMP_DATA_SIGNING */


#ifdef APPL_VALIDATE_ATT_PDU_LEN
API_RESULT appl_validate_att_pdu_req_len
           (
               UCHAR     att_event,
               UINT16    event_datalen
           )
{
    API_RESULT    retval;
    UINT32        index;

    retval = APPL_ATT_IGNORE_PDU_LEN_CHECK;

    for (index = 0; index < APPL_MAX_VALID_ATT_PDU_FOR_LEN_CHK; index++)
    {
        if (appl_valid_att_pdu_len[index].att_event == att_event)
        {

            if ((appl_valid_att_pdu_len[index].len1 == event_datalen) ||
                ((appl_valid_att_pdu_len[index].len2 == event_datalen) &&
                (appl_valid_att_pdu_len[index].len2 != APPL_ATT_INVALID_LEN)))
            {
                retval = API_SUCCESS;
            }
            else
            {
                retval = API_FAILURE;
                break;
            }
        }
    }
    return retval;
}
#endif /* APPL_VALIDATE_ATT_PDU_LEN */

#endif /* ATT */

