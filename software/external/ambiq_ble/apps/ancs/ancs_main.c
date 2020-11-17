// ****************************************************************************
//
//  ancs_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of ANCSC.
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
#include "wsf_assert.h"
#include "hci_api.h"
#include "dm_api.h"
#include "smp_api.h"
#include "att_api.h"
#include "app_cfg.h"
#include "app_api.h"
#include "app_db.h"
#include "app_ui.h"
#include "app_hw.h"
#include "svc_ch.h"
#include "svc_core.h"
#include "svc_dis.h"

#include "gatt_api.h"

// ancs
#include "ancc_api.h"
#include "ancs_api.h"

#include "am_util.h"

static volatile bool ph_incoming = false;
static uint32_t ph_notiuid;
/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! WSF message event starting value */
#define ANCS_MSG_START               0xA0

/*! WSF message event enumeration */
enum
{
    ANCC_ACTION_TIMER_IND = ANCS_MSG_START,            /*! ANCC action timer expired */
    ANCC_DISCOVER_TIMER_IND,                           /*! ANCC discover delay timer expired */
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
} ancsMsg_t;


/**************************************************************************************************
  Global Variables
**************************************************************************************************/
/*! application control block */
static struct
{
  /* ancs tracking variables */
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];
  wsfHandlerId_t    handlerId;
  uint8_t           discState;
  uint16_t          connInterval;     /* connection interval */
} ancsCb;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t ancsAdvCfg =
{
    {60000,     0,     0},                  /*! Advertising durations in ms */
    {  800,     0,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t ancsSlaveCfg =
{
    ANCS_CONN_MAX,                           /*! Maximum connections */
};

/*! configurable parameters for security */
static const appSecCfg_t ancsSecCfg =
{
    DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
    0,                                      /*! Initiator key distribution flags */
    DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
    FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
    FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for ANCS connection parameter update */
static const appUpdateCfg_t ancsUpdateCfg =
{
    3000,                                   /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
    6,                                      /*! 7.5ms */
    15,                                     /*! 18.75ms */
    0,                                      /*! Connection latency */
    600,                                    /*! Supervision timeout in 10ms units */
    5                                       /*! Number of update attempts before giving up */
};

/*! SMP security parameter configuration */
/* Configuration structure */
static const smpCfg_t ancsSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,                    /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0                                       /*! Device authentication requirements */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t ancsDiscCfg =
{
  TRUE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

/*! ANCC Configurable parameters */
static const anccCfg_t ancsAnccCfg =
{
   200         /*! action timer expiration period in ms */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t ancsAdvDataDisc[] =
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
    UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE),

    /*! device name */
    5,                                     /*! length */
    DM_ADV_TYPE_LOCAL_NAME,                /*! AD type */
    'A',                                   //advertise data has to be shorter than 31 bytes.
    'N',
    'C',
    'S'
};

/*! scan data, discoverable mode */
static const uint8_t ancsScanDataDisc[] =
{
    /*! service UUID */
    17,                                /*! length */
    DM_ADV_TYPE_128_SOLICIT,           /*! AD type */
    ATT_UUID_ANCS_SERVICE,
};

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  ANCS_DISC_GATT_SVC,      /* GATT service */
  ANCS_DISC_ANCS_SVC,      /* discover ANCS service */
  ANCS_DISC_SVC_MAX        /* Discovery complete */
};


/*! Start of each service's handles in the the handle list */
#define ANCS_DISC_GATT_START        0
#define ANCS_DISC_ANCS_START        (ANCS_DISC_GATT_START + GATT_HDL_LIST_LEN)
#define ANCS_DISC_HDL_LIST_LEN      (ANCS_DISC_ANCS_START + ANCC_HDL_LIST_LEN)

/*! Pointers into handle list for each service's handles */
static uint16_t *pAncsGattHdlList = &ancsCb.hdlList[ANCS_DISC_GATT_START];
static uint16_t *pAncsAnccHdlList = &ancsCb.hdlList[ANCS_DISC_ANCS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(ANCS_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/*
 * Data for configuration after service discovery
 */

/* Default value for CCC notifications */
static const uint8_t ancsCccNtfVal[] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t ancsDiscCfgList[] =
{
  /* Write:  GATT service changed ccc descriptor */
  {ancsCccNtfVal, sizeof(ancsCccNtfVal), (GATT_SC_CCC_HDL_IDX + ANCS_DISC_GATT_START)},

  /* Write:  ANCS setting ccc descriptor */
  {ancsCccNtfVal, sizeof(ancsCccNtfVal), (ANCC_NOTIFICATION_SOURCE_CCC_HDL_IDX + ANCS_DISC_ANCS_START)},

  /* Write:  ANCS setting ccc descriptor */
  {ancsCccNtfVal, sizeof(ancsCccNtfVal), (ANCC_DATA_SOURCE_CCC_HDL_IDX + ANCS_DISC_ANCS_START)},
};

/* Characteristic configuration list length */
#define ANCS_DISC_CFG_LIST_LEN   (sizeof(ancsDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(ANCS_DISC_CFG_LIST_LEN <= ANCS_DISC_HDL_LIST_LEN);

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
    ANCS_GATT_SC_CCC_IDX,               /*! GATT service, service changed characteristic */
    ANCS_NUM_CCC_IDX
};

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t ancsCccSet[ANCS_NUM_CCC_IDX] =
{
    /* cccd handle          value range               security level */
    {GATT_SC_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_NONE},    /* ANCS_GATT_SC_CCC_IDX */
};

/*************************************************************************************************/
/*!
 *  \fn     ancsDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsDmCback(dmEvt_t *pDmEvt)
{
    dmEvt_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
    {
        memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
        WsfMsgSend(ancsCb.handlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     ancsAttCback
 *
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsAttCback(attEvt_t *pEvt)
{
    attEvt_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
    {
        memcpy(pMsg, pEvt, sizeof(attEvt_t));
        pMsg->pValue = (uint8_t *) (pMsg + 1);
        memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
        WsfMsgSend(ancsCb.handlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     ancsCccCback
 *
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
 static void ancsCccCback(attsCccEvt_t *pEvt)
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
        WsfMsgSend(ancsCb.handlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     ancsProcCccState
 *
 *  \brief  Process CCC state change.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsProcCccState(ancsMsg_t *pMsg)
{
    APP_TRACE_INFO3("ccc state ind value:%d handle:%d idx:%d", pMsg->ccc.value, pMsg->ccc.handle, pMsg->ccc.idx);
}

/*************************************************************************************************/
/*!
 *  \fn     ancsClose
 *
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsClose(ancsMsg_t *pMsg)
{
}

/*************************************************************************************************/
/*!
 *  \fn     ancsSetup
 *
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsSetup(ancsMsg_t *pMsg)
{
    /* set advertising and scan response data for discoverable mode */
    AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(ancsAdvDataDisc), (uint8_t *) ancsAdvDataDisc);
    AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(ancsScanDataDisc), (uint8_t *) ancsScanDataDisc);

    /* set advertising and scan response data for connectable mode */
    AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);
    AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, 0, NULL);

    /* start advertising; automatically set connectable/discoverable mode and bondable mode */
    AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     ancsValueUpdate
 *
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsValueUpdate(attEvt_t *pMsg)
{
    /* iOS notification */
    if ((pMsg->handle == pAncsAnccHdlList[ANCC_NOTIFICATION_SOURCE_HDL_IDX]) ||
        (pMsg->handle == pAncsAnccHdlList[ANCC_DATA_SOURCE_HDL_IDX]) ||
        (pMsg->handle == pAncsAnccHdlList[ANCC_CONTROL_POINT_HDL_IDX]))
    {
        AnccNtfValueUpdate(pAncsAnccHdlList, pMsg, ANCC_ACTION_TIMER_IND);
    }
    else
    {
        APP_TRACE_INFO0("Data received from other other handle");
    }

    /* GATT */
    if (GattValueUpdate(pAncsGattHdlList, pMsg) == ATT_SUCCESS)
    {
        return;
    }
}
/*************************************************************************************************/
/*!
 *  \fn     ancsBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsBtnCback(uint8_t btn)
{
    dmConnId_t      connId;
    APP_TRACE_INFO1("btn = %d", btn);

    /* button actions when connected */
    if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
    {
        switch (btn)
        {
            case APP_UI_BTN_1_DOWN:
            break;
            case APP_UI_BTN_1_SHORT:
            break;

            case APP_UI_BTN_1_MED:
            break;

            case APP_UI_BTN_1_LONG:
                AppConnClose(connId);
            break;

            case APP_UI_BTN_2_DOWN:
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
 *  \fn     ancsDiscCback
 *
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, ANCS_DISC_HDL_LIST_LEN, ancsCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* request security */
      AppSlaveSecurityReq(connId);
      break;

    case APP_DISC_READ_DATABASE_HASH:
      /* Read peer's database hash */
      AppDiscReadDatabaseHash(connId);
      break;
    case APP_DISC_START:
      /* initialize discovery state */
      ancsCb.discState = ANCS_DISC_GATT_SVC;

      /* discover GATT service */
      GattDiscover(connId, pAncsGattHdlList);
      break;

    case APP_DISC_FAILED:
      APP_TRACE_INFO1("!!!!!Disc Failed. discState = %d!!!!!", ancsCb.discState);
    case APP_DISC_CMPL:
      //expecting only ancs service to be discovered
      ancsCb.discState++;
      if (ancsCb.discState == ANCS_DISC_ANCS_SVC)
      {
        /* discover ANCS service */
        AnccSvcDiscover(connId, pAncsAnccHdlList);
        APP_TRACE_INFO0("Discovering ANCS.");
      }
      else
      {
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);
        APP_TRACE_INFO0("Finished ANCS discovering.");

        /* start configuration */
        APP_TRACE_INFO0("Disc CFG start.");
        AppDiscConfigure(connId, APP_DISC_CFG_START, ANCS_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) ancsDiscCfgList, ANCS_DISC_HDL_LIST_LEN, ancsCb.hdlList);
      }
      break;

    case APP_DISC_CFG_START:
      /* start configuration */
      AppDiscConfigure(connId, APP_DISC_CFG_START, ANCS_DISC_CFG_LIST_LEN,
                       (attcDiscCfg_t *) ancsDiscCfgList, ANCS_DISC_HDL_LIST_LEN, ancsCb.hdlList);
      break;

    case APP_DISC_CFG_CMPL:
      AppDiscComplete(connId, status);
      APP_TRACE_INFO0("Finished Disc CFG.");
      break;

    case APP_DISC_CFG_CONN_START:
      /* start connection setup configuration */
      AppDiscConfigure(connId, APP_DISC_CFG_CONN_START, ANCS_DISC_CFG_LIST_LEN,
                       (attcDiscCfg_t *) ancsDiscCfgList, ANCS_DISC_HDL_LIST_LEN, ancsCb.hdlList);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     ancsProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ancsProcMsg(ancsMsg_t *pMsg)
{
    uint8_t uiEvent = APP_UI_NONE;

    switch(pMsg->hdr.event)
    {
        case ANCC_ACTION_TIMER_IND:
            AnccActionHandler(ANCC_ACTION_TIMER_IND);
            break;

        case ANCC_DISCOVER_TIMER_IND:
            AnccStartServiceDiscovery();
            break;
        case ATTC_READ_RSP:
        case ATTC_HANDLE_VALUE_NTF:
        case ATTC_HANDLE_VALUE_IND:
            APP_TRACE_INFO0("------------ATTC_HANDLE_VALUE_NTF/IND------------");
            ancsValueUpdate((attEvt_t *) pMsg);
          break;

        case ATTC_WRITE_RSP:    // write respose after Control point operation.
            APP_TRACE_INFO0("------------ATTC_WRITE_RSP------------");
          break;

        case ATTS_HANDLE_VALUE_CNF:
        break;

        case ATTS_CCC_STATE_IND:
            ancsProcCccState(pMsg);
        break;

        case ATT_MTU_UPDATE_IND:
          APP_TRACE_INFO1("Negotiated MTU %d", ((attEvt_t *)pMsg)->mtu);
        break;

        case DM_RESET_CMPL_IND:
            AttsCalculateDbHash();
            DmSecGenerateEccKeyReq();
            ancsSetup(pMsg);
            uiEvent = APP_UI_RESET_CMPL;
        break;

        case DM_ADV_START_IND:
            uiEvent = APP_UI_ADV_START;
        break;

        case DM_ADV_STOP_IND:
            uiEvent = APP_UI_ADV_STOP;
        break;

        case DM_CONN_OPEN_IND:
            /* set bondable here to enable bond/pair after disconnect */
            AnccConnOpen(pMsg->hdr.param, pAncsAnccHdlList);
            uiEvent = APP_UI_CONN_OPEN;
        break;

        case DM_CONN_CLOSE_IND:
            ancsClose(pMsg);
            AnccConnClose();
            uiEvent = APP_UI_CONN_CLOSE;
        break;

        case DM_CONN_UPDATE_IND:
        break;

        case DM_SEC_PAIR_CMPL_IND:
            DmSecGenerateEccKeyReq();
            uiEvent = APP_UI_SEC_PAIR_CMPL;
            APP_TRACE_INFO1("------------MTU SIZE = %d------------", AttGetMtu(pMsg->hdr.param));
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
 *  \fn     AncsHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AncsHandlerInit(wsfHandlerId_t handlerId)
{
    APP_TRACE_INFO0("AncsHandlerInit");

    /* store handler ID */
    ancsCb.handlerId = handlerId;

    /* Set configuration pointers */
    pAppAdvCfg = (appAdvCfg_t *) &ancsAdvCfg;
    pAppSlaveCfg = (appSlaveCfg_t *) &ancsSlaveCfg;
    pAppSecCfg = (appSecCfg_t *) &ancsSecCfg;
    pAppUpdateCfg = (appUpdateCfg_t *) &ancsUpdateCfg;
    pAppDiscCfg = (appDiscCfg_t *) &ancsDiscCfg;
    pSmpCfg = (smpCfg_t *) &ancsSmpCfg;

    /* Initialize application framework */
    AppSlaveInit();
    AppServerInit();
    AppDiscInit();

    AnccInit(handlerId, (anccCfg_t*)(&ancsAnccCfg), ANCC_DISCOVER_TIMER_IND);
}

/*************************************************************************************************/
/*!
 *  \fn     AncsHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AncsHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
    if (pMsg != NULL)
    {
        APP_TRACE_INFO2("ANCS got evt %d on handle 0x%04x", pMsg->event, ((attEvt_t *)pMsg)->handle);
        if ( pMsg->event <= ATT_CBACK_END )
        {   //process discovery-related ATT messages
            AppDiscProcAttMsg((attEvt_t *) pMsg);   //process ATT messages
        }
        else if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END)
        {
            /* process advertising and connection-related messages */
            AppSlaveProcDmMsg((dmEvt_t *) pMsg);

            /* process security-related messages */
            AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);

            /* process discovery-related messages */
            AppDiscProcDmMsg((dmEvt_t *) pMsg);
        }

        /* perform profile and user interface-related operations */
        ancsProcMsg((ancsMsg_t *) pMsg);
    }
}

void ancsAnccAttrCback(active_notif_t* pAttr)
{
    // this is an application demo, print the notification info
    if ( pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER )
    {
        APP_TRACE_INFO0("************************************************************");
        APP_TRACE_INFO0("* Notification Received ");
        APP_TRACE_INFO1("* UID             = %d", anccCb.anccList[pAttr->handle].notification_uid);
        switch(anccCb.anccList[pAttr->handle].category_id)
        {
            case BLE_ANCS_CATEGORY_ID_OTHER:
                APP_TRACE_INFO0("* Category        = Other");
            break;
            case BLE_ANCS_CATEGORY_ID_INCOMING_CALL:
                APP_TRACE_INFO0("* Category        = Incoming Call");
                ph_incoming = true;
                ph_notiuid = anccCb.anccList[pAttr->handle].notification_uid;
            break;
            case BLE_ANCS_CATEGORY_ID_MISSED_CALL:
                APP_TRACE_INFO0("* Category        = Missed Call");
                ph_incoming = false;
            break;
            case BLE_ANCS_CATEGORY_ID_VOICE_MAIL:
                APP_TRACE_INFO0("* Category        = Voice Mail");
            break;
            case BLE_ANCS_CATEGORY_ID_SOCIAL:
                APP_TRACE_INFO0("* Category        = Social");
            break;
            case BLE_ANCS_CATEGORY_ID_SCHEDULE:
                APP_TRACE_INFO0("* Category        = Schedule");
            break;
            case BLE_ANCS_CATEGORY_ID_EMAIL:
                APP_TRACE_INFO0("* Category        = Email");
            break;
            case BLE_ANCS_CATEGORY_ID_NEWS:
                APP_TRACE_INFO0("* Category        = News");
            break;
            case BLE_ANCS_CATEGORY_ID_HEALTH_AND_FITNESS:
                APP_TRACE_INFO0("* Category        = Health and Fitness");
            break;
            case BLE_ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE:
                APP_TRACE_INFO0("* Category        = Business and Finance");
            break;
            case BLE_ANCS_CATEGORY_ID_LOCATION:
                APP_TRACE_INFO0("* Category        = Location");
            break;
            case BLE_ANCS_CATEGORY_ID_ENTERTAINMENT:
                APP_TRACE_INFO0("* Category        = Entertainment");
            break;
            default:
            break;
        }

        switch(anccCb.anccList[pAttr->handle].event_id)
        {
            case BLE_ANCS_EVENT_ID_NOTIFICATION_ADDED:
                APP_TRACE_INFO0("* Event ID        = Added");
            break;
            case BLE_ANCS_EVENT_ID_NOTIFICATION_MODIFIED:
                APP_TRACE_INFO0("* Event ID        = Modified");
            break;
            case BLE_ANCS_EVENT_ID_NOTIFICATION_REMOVED:
                APP_TRACE_INFO0("* Event ID        = Removed");
            break;
            default:
            break;
        }

        APP_TRACE_INFO0("* EventFlags      = ");
        for ( uint16_t i = 0; i < 5; i++ )
        {
            if ( anccCb.anccList[pAttr->handle].event_flags & (0x00000001 << i) )
            {
                switch(i)
                {
                    case 0:
                        APP_TRACE_INFO0("Silent ");
                    break;
                    case 1:
                        APP_TRACE_INFO0("Important ");
                    break;
                    case 2:
                        APP_TRACE_INFO0("PreExisting ");
                    break;
                    case 3:
                        APP_TRACE_INFO0("PositiveAction ");
                    break;
                    case 4:
                        APP_TRACE_INFO0("NegativeAction ");
                    break;
                    default:
                    break;
                }
            }
        }
        APP_TRACE_INFO1("* Category Count  = %d", anccCb.anccList[pAttr->handle].category_count);
    }
    else if (pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_TITLE)
    {
        if ( pAttr->attrLength != 0 )
        {
            APP_TRACE_INFO0("* Title           = ");
            APP_TRACE_INFO1("%s", &(pAttr->attrDataBuf[pAttr->parseIndex]));
        }
    }
    else if (pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE)
    {
        if ( pAttr->attrLength != 0 )
        {
            APP_TRACE_INFO0("* Subtitle        = ");
            APP_TRACE_INFO1("%s", &(pAttr->attrDataBuf[pAttr->parseIndex]));
        }
    }
    else if (pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_MESSAGE)
    {
        if ( pAttr->attrLength != 0 )
        {
            APP_TRACE_INFO0("* Message         = ");
            APP_TRACE_INFO1("%s", &(pAttr->attrDataBuf[pAttr->parseIndex]));
        }
    }
    else if (pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_DATE)
    {
        if ( pAttr->attrLength != 0 )
        {
            APP_TRACE_INFO0("* Date & Time     = ");
            APP_TRACE_INFO1("%s", &(pAttr->attrDataBuf[pAttr->parseIndex]));
        }
    }
    else if (pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL)
    {
        if ( pAttr->attrLength != 0 )
        {
            APP_TRACE_INFO0("* Positive Action = ");
            APP_TRACE_INFO1("%s", &(pAttr->attrDataBuf[pAttr->parseIndex]));
        }
    }
    else if (pAttr->attrId == BLE_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL)
    {
        if (pAttr->attrLength != 0)
        {
            APP_TRACE_INFO0("* Negative Action = ");
            APP_TRACE_INFO1("%s", &(pAttr->attrDataBuf[pAttr->parseIndex]));
        }
    }
}

bool ancsRejectCall(void)
{
    if (true == ph_incoming)
    {
        AncsPerformNotiAction(anccCb.hdlList, ph_notiuid, BLE_ANCS_NOTIF_ACTION_ID_NEGATIVE);
        ph_incoming = false;
        return true;
    }
    else
    {
        return false;
    }
}
void ancsAnccNotifCback(active_notif_t* pAttr, uint32_t notiUid)
{
    //
    // removes notifications received
    //
    // AncsPerformNotiAction(pNotiAnccHdlList, notiUid, BLE_ANCS_NOTIF_ACTION_ID_NEGATIVE);
    APP_TRACE_INFO0("************************************************************");
}

void anccNotiRemoveCback(ancc_notif_t* pAttr)
{
    APP_TRACE_INFO1("Nofity removed, category_id = %d", pAttr->category_id);
}

/*************************************************************************************************/
/*!
 *  \fn     AncsStart
 *
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AncsStart(void)
{
    /* Register for stack callbacks */
    DmRegister(ancsDmCback);
    DmConnRegister(DM_CLIENT_ID_APP, ancsDmCback);
    AttRegister(ancsAttCback);
    AttConnRegister(AppServerConnCback);
    AttsCccRegister(ANCS_NUM_CCC_IDX, (attsCccSet_t *) ancsCccSet, ancsCccCback);

    /* Register for app framework callbacks */
    AppUiBtnRegister(ancsBtnCback);

    /* Register for app framework discovery callbacks */
    AppDiscRegister(ancsDiscCback);

    //
    // Register for ancc callbacks
    //
    AnccCbackRegister(ancsAnccAttrCback, ancsAnccNotifCback, anccNotiRemoveCback);

    /* Initialize attribute server database */
    SvcCoreGattCbackRegister(GattReadCback, GattWriteCback);
    SvcCoreAddGroup();
    SvcDisAddGroup();

    /* Set Service Changed CCCD index. */
    GattSetSvcChangedIdx(ANCS_GATT_SC_CCC_IDX);

    /* Reset the device */
    DmDevReset();
}
