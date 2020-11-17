// ****************************************************************************
//
//  amota_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of AMOTA service.
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
#include "wsf_types.h"
#include "bstream.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "hci_api.h"
#include "dm_api.h"
#include "att_api.h"
#include "app_api.h"
#include "app_db.h"
#include "app_ui.h"
#include "app_hw.h"
#include "svc_ch.h"
#include "svc_core.h"
#include "svc_dis.h"
#include "amota_api.h"
#include "amotas_api.h"
#include "svc_amotas.h"
#include "gatt_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! WSF message event starting value */
#define AMOTA_MSG_START               0xA0

/*! WSF message event enumeration */
enum
{
    AMOTA_RESET_TIMER_IND = AMOTA_MSG_START,  /*! AMOTA reset timer expired */
    AMOTA_DISCONNECT_TIMER_IND,  /*! AMOTA disconnect timer expired */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Application message type */
typedef union
{
    wsfMsgHdr_t     hdr;
    dmEvt_t         dm;
    attsCccEvt_t    ccc;
    attEvt_t        att;
} amotaMsg_t;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t amotaAdvCfg =
{
    {    0,     0,     0},                  /*! Advertising durations in ms */
    {  800,   800,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t amotaSlaveCfg =
{
    AMOTA_CONN_MAX,                           /*! Maximum connections */
};

/*! configurable parameters for security */
static const appSecCfg_t amotaSecCfg =
{
    DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
    0,                                      /*! Initiator key distribution flags */
    DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
    FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
    FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for AMOTA connection parameter update */
static appUpdateCfg_t otaUpdateCfg =
{
    0,                                /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
    (15 / 1.25),                         /*! 15 ms */
    (30 / 1.25),                         /*! 30 ms */
    0,                                   /*! Connection latency */
    (6000 / 10),                         /*! Supervision timeout in 10ms units */
    5                                    /*! Number of update attempts before giving up */
};

/*! AMOTAS configuration */
static const AmotasCfg_t amotasCfg =
{
    0
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t amotaAdvDataDisc[] =
{
    /*! flags */
    2,                                      /*! length */
    DM_ADV_TYPE_FLAGS,                      /*! AD type */
    DM_FLAG_LE_GENERAL_DISC |               /*! flags */
    DM_FLAG_LE_BREDR_NOT_SUP,

    /*! tx power */
    2,                                      /*! length */
    DM_ADV_TYPE_TX_POWER,                   /*! AD type */
    0,                                      /*! tx power */

    /*! service UUID list */
    3,                                      /*! length */
    DM_ADV_TYPE_16_UUID,                    /*! AD type */
    UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE)
};

/*! scan data, discoverable mode */
static const uint8_t amotaScanDataDisc[] =
{
    /*! device name */
    15,                                     /*! length */
    DM_ADV_TYPE_LOCAL_NAME,                 /*! AD type */
    'a',
    'm',
    'b',
    'i',
    'q',
    'm',
    'i',
    'c',
    'r',
    'o',
    ' ',
    'o',
    't',
    'a'
};

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
    AMOTA_GATT_SC_CCC_IDX,                    /*! GATT service, service changed characteristic */
    AMOTA_AMOTAS_TX_CCC_IDX,                  /*! AMOTA service, tx characteristic */
    AMOTA_NUM_CCC_IDX
};

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t amotaCccSet[AMOTA_NUM_CCC_IDX] =
{
    /* cccd handle          value range               security level */
    {GATT_SC_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_NONE},   /* AMOTA_GATT_SC_CCC_IDX */
    {AMOTAS_TX_CH_CCC_HDL,  ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_NONE}    /* AMOTA_AMOTAS_TX_CCC_IDX */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! WSF handler ID */
wsfHandlerId_t amotaHandlerId;

/*************************************************************************************************/
/*!
 *  \fn     amotaDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaDmCback(dmEvt_t *pDmEvt)
{
    dmEvt_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
    {
        memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
        WsfMsgSend(amotaHandlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amotaAttCback
 *
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaAttCback(attEvt_t *pEvt)
{
    attEvt_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
    {
        memcpy(pMsg, pEvt, sizeof(attEvt_t));
        pMsg->pValue = (uint8_t *) (pMsg + 1);
        memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
        WsfMsgSend(amotaHandlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amotaCccCback
 *
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
 static void amotaCccCback(attsCccEvt_t *pEvt)
{
    attsCccEvt_t  *pMsg;
    appDbHdl_t    dbHdl;

    /* if CCC not set from initialization and there's a device record */
    if ((pEvt->handle != ATT_HANDLE_NONE) &&
        ((dbHdl = AppDbGetHdl((dmConnId_t) pEvt->hdr.param)) != APP_DB_HDL_NONE))
    {
        /* store value in device database */
        AppDbSetCccTblValue(dbHdl, pEvt->idx, pEvt->value);
    }

    if ((pMsg = WsfMsgAlloc(sizeof(attsCccEvt_t))) != NULL)
    {
        memcpy(pMsg, pEvt, sizeof(attsCccEvt_t));
        WsfMsgSend(amotaHandlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amotaProcCccState
 *
 *  \brief  Process CCC state change.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaProcCccState(amotaMsg_t *pMsg)
{
    APP_TRACE_INFO3("ccc state ind value:%d handle:%d idx:%d", pMsg->ccc.value, pMsg->ccc.handle, pMsg->ccc.idx);

    /* handle heart rate measurement CCC */
    /* AMOTAS TX CCC */
    if (pMsg->ccc.idx == AMOTA_AMOTAS_TX_CCC_IDX)
    {
        if (pMsg->ccc.value == ATT_CLIENT_CFG_NOTIFY)
        {
            // notify enabled
            amotas_start((dmConnId_t) pMsg->ccc.hdr.param,
                AMOTA_RESET_TIMER_IND, AMOTA_DISCONNECT_TIMER_IND,
                AMOTA_AMOTAS_TX_CCC_IDX);
        }
        else
        {
            // notify disabled
            amotas_stop((dmConnId_t) pMsg->ccc.hdr.param);
        }
        return;
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amotaClose
 *
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaClose(amotaMsg_t *pMsg)
{
    /* stop amotas */
    amotas_conn_close((dmConnId_t) pMsg->hdr.param);
}

/*************************************************************************************************/
/*!
 *  \fn     amotaSetup
 *
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaSetup(amotaMsg_t *pMsg)
{
    /* set advertising and scan response data for discoverable mode */
    AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(amotaAdvDataDisc), (uint8_t *) amotaAdvDataDisc);
    AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(amotaScanDataDisc), (uint8_t *) amotaScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, 0, NULL);

    /* start advertising; automatically set connectable/discoverable mode and bondable mode */
    AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     amotaBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaBtnCback(uint8_t btn)
{
    dmConnId_t      connId;

    /* button actions when connected */
    if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
    {
        switch (btn)
        {
            case APP_UI_BTN_1_SHORT:
            break;

            case APP_UI_BTN_1_MED:
            break;

            case APP_UI_BTN_1_LONG:
                AppConnClose(connId);
            break;

            case APP_UI_BTN_2_SHORT:
            break;

            default:
            break;
        }
    }
    /* button actions when not connected */
    else
    {
        switch (btn)
        {
            case APP_UI_BTN_1_SHORT:
                /* start or restart advertising */
                AppAdvStart(APP_MODE_AUTO_INIT);
            break;

            case APP_UI_BTN_1_MED:
                /* enter discoverable and bondable mode mode */
                AppSetBondable(TRUE);
                AppAdvStart(APP_MODE_DISCOVERABLE);
            break;

            case APP_UI_BTN_1_LONG:
                /* clear bonded device info and restart advertising */
                AppDbDeleteAllRecords();
                AppAdvStart(APP_MODE_AUTO_INIT);
            break;

            default:
            break;
        }
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amotaProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amotaProcMsg(amotaMsg_t *pMsg)
{
    uint8_t uiEvent = APP_UI_NONE;

    switch(pMsg->hdr.event)
    {
        case AMOTA_RESET_TIMER_IND:
        case AMOTA_DISCONNECT_TIMER_IND:
            amotas_proc_msg(&pMsg->hdr);
            break;

        case ATTS_HANDLE_VALUE_CNF:
        break;

        case ATTS_CCC_STATE_IND:
            amotaProcCccState(pMsg);
            break;

        case ATT_MTU_UPDATE_IND:
          APP_TRACE_INFO1("Negotiated MTU %d", ((attEvt_t *)pMsg)->mtu);
            break;

        case DM_RESET_CMPL_IND:
            AttsCalculateDbHash();
            DmSecGenerateEccKeyReq();
            amotaSetup(pMsg);
            uiEvent = APP_UI_RESET_CMPL;
            break;

        case DM_ADV_SET_START_IND:
            uiEvent = APP_UI_ADV_SET_START_IND;
            break;

        case DM_ADV_SET_STOP_IND:
            uiEvent = APP_UI_ADV_SET_STOP_IND;
            break;

        case DM_ADV_START_IND:
            uiEvent = APP_UI_ADV_START;
            break;

        case DM_ADV_STOP_IND:
            uiEvent = APP_UI_ADV_STOP;
            break;

        case DM_CONN_OPEN_IND:
            amotas_proc_msg(&pMsg->hdr);
            uiEvent = APP_UI_CONN_OPEN;
            break;

        case DM_CONN_CLOSE_IND:
        {
            hciDisconnectCmplEvt_t *evt = (hciDisconnectCmplEvt_t*) pMsg;
            // uiEvent = APP_UI_CONN_CLOSE;
            APP_TRACE_INFO1(">>> Connection closed reason 0x%x <<<",
                    evt->reason);
            amotaClose(pMsg);
        }
        break;

        case DM_CONN_UPDATE_IND:
            amotas_proc_msg(&pMsg->hdr);
        break;

        case DM_SEC_PAIR_CMPL_IND:
            DmSecGenerateEccKeyReq();
            uiEvent = APP_UI_SEC_PAIR_CMPL;
        break;

        case DM_SEC_PAIR_FAIL_IND:
            DmSecGenerateEccKeyReq();
            uiEvent = APP_UI_SEC_PAIR_FAIL;
        break;

        case DM_SEC_ENCRYPT_IND:
            uiEvent = APP_UI_SEC_ENCRYPT;
        break;

        case DM_SEC_ENCRYPT_FAIL_IND:
            uiEvent = APP_UI_SEC_ENCRYPT_FAIL;
        break;

        case DM_SEC_AUTH_REQ_IND:
            AppHandlePasskey(&pMsg->dm.authReq);
        break;

        case DM_SEC_ECC_KEY_IND:
            DmSecSetEccKey(&pMsg->dm.eccMsg.data.key);
          break;

        case DM_SEC_COMPARE_IND:
          AppHandleNumericComparison(&pMsg->dm.cnfInd);
          break;

        case DM_PRIV_CLEAR_RES_LIST_IND:
          APP_TRACE_INFO1("Clear resolving list status 0x%02x", pMsg->hdr.status);
          break;

        case DM_HW_ERROR_IND:
          uiEvent = APP_UI_HW_ERROR;
          break;
        case DM_VENDOR_SPEC_CMD_CMPL_IND:
          {
            #if defined(AM_PART_APOLLO) || defined(AM_PART_APOLLO2)

              uint8_t *param_ptr = &pMsg->dm.vendorSpecCmdCmpl.param[0];

              switch (pMsg->dm.vendorSpecCmdCmpl.opcode)
              {
                case 0xFC20: //read at address
                {
                  uint32_t read_value;

                  BSTREAM_TO_UINT32(read_value, param_ptr);

                  APP_TRACE_INFO3("VSC 0x%0x complete status %x param %x",
                    pMsg->dm.vendorSpecCmdCmpl.opcode,
                    pMsg->hdr.status,
                    read_value);
                }

                break;
                default:
                    APP_TRACE_INFO2("VSC 0x%0x complete status %x",
                        pMsg->dm.vendorSpecCmdCmpl.opcode,
                        pMsg->hdr.status);
                break;
              }

            #endif
          }
          break;
        default:
        break;
    }

    if (uiEvent != APP_UI_NONE)
    {
        AppUiAction(uiEvent);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AmotaHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmotaHandlerInit(wsfHandlerId_t handlerId)
{
    APP_TRACE_INFO0("AmotaHandlerInit");

    /* store handler ID */
    amotaHandlerId = handlerId;

    /* Set configuration pointers */
    pAppAdvCfg = (appAdvCfg_t *) &amotaAdvCfg;
    pAppSlaveCfg = (appSlaveCfg_t *) &amotaSlaveCfg;
    pAppSecCfg = (appSecCfg_t *) &amotaSecCfg;
    pAppUpdateCfg = (appUpdateCfg_t *) &otaUpdateCfg;

    /* Initialize application framework */
    AppSlaveInit();
    AppServerInit();

    /* initialize amota service server */
    amotas_init(handlerId, (AmotasCfg_t *) &amotasCfg);
}

/*************************************************************************************************/
/*!
 *  \fn     AmotaHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmotaHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
    if (pMsg != NULL)
    {
        // APP_TRACE_INFO1("Amota got evt %d", pMsg->event);

        /* process ATT messages */
        if (pMsg->event >= ATT_CBACK_START && pMsg->event <= ATT_CBACK_END)
        {
          /* process server-related ATT messages */
          AppServerProcAttMsg(pMsg);
        }
        /* process DM messages */
        else if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END)
        {
            /* process advertising and connection-related messages */
            AppSlaveProcDmMsg((dmEvt_t *) pMsg);

            /* process security-related messages */
            AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);
        }

        /* perform profile and user interface-related operations */
        amotaProcMsg((amotaMsg_t *) pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AmotaStart
 *
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmotaStart(void)
{
    /* Register for stack callbacks */
    DmRegister(amotaDmCback);
    DmConnRegister(DM_CLIENT_ID_APP, amotaDmCback);
    AttRegister(amotaAttCback);
    AttConnRegister(AppServerConnCback);
    AttsCccRegister(AMOTA_NUM_CCC_IDX, (attsCccSet_t *) amotaCccSet, amotaCccCback);

    /* Register for app framework callbacks */
    AppUiBtnRegister(amotaBtnCback);

    /* Initialize attribute server database */
    SvcCoreGattCbackRegister(GattReadCback, GattWriteCback);
    SvcCoreAddGroup();
    SvcDisAddGroup();
    SvcAmotasCbackRegister(NULL, amotas_write_cback);
    SvcAmotasAddGroup();

    /* Set Service Changed CCCD index. */
    GattSetSvcChangedIdx(AMOTA_GATT_SC_CCC_IDX);

    /* Reset the device */
    DmDevReset();
}
