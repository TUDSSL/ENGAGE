// ****************************************************************************
//
//  amdtp_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of AMDTP service.
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
#include "smp_api.h"
#include "app_api.h"
#include "app_db.h"
#include "app_ui.h"
#include "app_hw.h"
#include "svc_ch.h"
#include "svc_core.h"
#include "svc_dis.h"
#include "amdtp_api.h"
#include "amdtps_api.h"
#include "svc_amdtp.h"

#include "am_util.h"
#include "gatt_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/
#define MEASURE_THROUGHPUT

/*! WSF message event starting value */
#define AMDTP_MSG_START               0xA0

/*! WSF message event enumeration */
enum
{
  AMDTP_TIMER_IND = AMDTP_MSG_START,  /*! AMDTP tx timeout timer expired */
#ifdef MEASURE_THROUGHPUT
  AMDTP_MEAS_TP_TIMER_IND,
#endif
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
} amdtpMsg_t;

static void amdtpClose(amdtpMsg_t *pMsg);
/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t amdtpAdvCfg =
{
  {60000,     0,     0},                  /*! Advertising durations in ms */
  {  800,   800,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t amdtpSlaveCfg =
{
  AMDTP_CONN_MAX,                           /*! Maximum connections */
};

/*! configurable parameters for security */
static const appSecCfg_t amdtpSecCfg =
{
  DM_AUTH_BOND_FLAG | DM_AUTH_SC_FLAG,    /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for AMDTP connection parameter update */
static const appUpdateCfg_t amdtpUpdateCfg =
{
  3000,                                   /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
 /* W/A: Apollo2-Blue has issues with interval 7.5ms */
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
  6,                                      /*! 7.5ms */
  15,                                     /*! 18.75ms */
#else
   8,
  18,
#endif
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};

/*! SMP security parameter configuration */
static const smpCfg_t amdtpSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,                    /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0,                                      /*! Device authentication requirements */
};

/*! AMDTPS configuration */
static const AmdtpsCfg_t amdtpAmdtpsCfg =
{
    0
};
/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t amdtpAdvDataDisc[] =
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

  17,                                      /*! length */
  DM_ADV_TYPE_128_UUID,                    /*! AD type */
  ATT_UUID_AMDTP_SERVICE
};

/*! scan data, discoverable mode */
static const uint8_t amdtpScanDataDisc[] =
{
  /*! device name */
  6,                                      /*! length */
  DM_ADV_TYPE_LOCAL_NAME,                 /*! AD type */
  'A',
  'm',
  'd',
  't',
  'p'
};

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
  AMDTP_GATT_SC_CCC_IDX,                  /*! GATT service, service changed characteristic */
  AMDTP_AMDTPS_TX_CCC_IDX,                /*! AMDTP service, tx characteristic */
  AMDTP_AMDTPS_RX_ACK_CCC_IDX,            /*! AMDTP service, rx ack characteristic */
  AMDTP_NUM_CCC_IDX
};

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t amdtpCccSet[AMDTP_NUM_CCC_IDX] =
{
  /* cccd handle                value range               security level */
  {GATT_SC_CH_CCC_HDL,          ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_NONE},   /* AMDTP_GATT_SC_CCC_IDX */
  {AMDTPS_TX_CH_CCC_HDL,        ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_NONE},   /* AMDTP_AMDTPS_TX_CCC_IDX */
  {AMDTPS_ACK_CH_CCC_HDL,       ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_NONE}    /* AMDTP_AMDTPS_RX_ACK_CCC_IDX */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! WSF handler ID */
wsfHandlerId_t amdtpHandlerId;

#ifdef MEASURE_THROUGHPUT
wsfTimer_t measTpTimer;
int gTotalDataBytesRecev = 0;
#endif

/*************************************************************************************************/
/*!
 *  \fn     amdtpDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  uint16_t len;

  len = DmSizeOfEvt(pDmEvt);

  if ((pMsg = WsfMsgAlloc(len)) != NULL)
  {
    memcpy(pMsg, pDmEvt, len);
    WsfMsgSend(amdtpHandlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpAttCback
 *
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(amdtpHandlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpCccCback
 *
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpCccCback(attsCccEvt_t *pEvt)
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
    WsfMsgSend(amdtpHandlerId, pMsg);
  }
}

static bool sendDataContinuously = false;
static uint32_t counter = 0;

static void AmdtpsSendTestData(void)
{
    uint8_t data[1024] = {0};
    eAmdtpStatus_t status;

    sendDataContinuously = true;
    *((uint32_t*)&(data[0])) = counter;

//    status = AmdtpsSendPacket(AMDTP_PKT_TYPE_DATA, false, true, data, sizeof(data));
    status = AmdtpsSendPacket(AMDTP_PKT_TYPE_DATA, false, false, data, AttGetMtu(1) - 11);    //fixme
    if (status != AMDTP_STATUS_SUCCESS)
    {
        APP_TRACE_INFO1("AmdtpsSendTestData() failed, status = %d\n", status);
    }
    else
    {
        counter++;
    }
}
/*************************************************************************************************/
/*!
 *  \fn     amdtpProcCccState
 *
 *  \brief  Process CCC state change.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
bool bPairingCompleted = false;
static void amdtpProcCccState(amdtpMsg_t *pMsg)
{
  APP_TRACE_INFO3("ccc state ind value:%d handle:%d idx:%d", pMsg->ccc.value, pMsg->ccc.handle, pMsg->ccc.idx);

  /* AMDTPS TX CCC */
  if (pMsg->ccc.idx == AMDTP_AMDTPS_TX_CCC_IDX)
  {
    if (pMsg->ccc.value == ATT_CLIENT_CFG_NOTIFY)
    {
      // notify enabled
      amdtps_start((dmConnId_t)pMsg->ccc.hdr.param, AMDTP_TIMER_IND, AMDTP_AMDTPS_TX_CCC_IDX);
      // AppSlaveSecurityeq(1);
      // if(bPairingCompleted == true)
      {
#if defined(AMDTPS_TXTEST)
        counter = 0;
        AmdtpsSendTestData(); //fixme
#endif
      }
    }
    else
    {
      // notify disabled
      amdtpClose(pMsg);
      amdtps_stop((dmConnId_t)pMsg->ccc.hdr.param);
    }
    return;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpClose
 *
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpClose(amdtpMsg_t *pMsg)
{
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpSetup
 *
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpSetup(amdtpMsg_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(amdtpAdvDataDisc), (uint8_t *) amdtpAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(amdtpScanDataDisc), (uint8_t *) amdtpScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, sizeof(amdtpAdvDataDisc), (uint8_t *) amdtpAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, sizeof(amdtpScanDataDisc), (uint8_t *) amdtpScanDataDisc);

  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpBtnCback(uint8_t btn)
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

#ifdef MEASURE_THROUGHPUT
static void showThroughput(void)
{
    APP_TRACE_INFO1("throughput : %d Bytes/s\n", gTotalDataBytesRecev);
    gTotalDataBytesRecev = 0;
    WsfTimerStartSec(&measTpTimer, 1);
}
#endif

/*************************************************************************************************/
/*!
 *  \fn     amdtpProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpProcMsg(amdtpMsg_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;

  switch(pMsg->hdr.event)
  {
    case AMDTP_TIMER_IND:
      amdtps_proc_msg(&pMsg->hdr);
      break;

#ifdef MEASURE_THROUGHPUT
    case AMDTP_MEAS_TP_TIMER_IND:
      showThroughput();
      break;
#endif

    case ATTS_HANDLE_VALUE_CNF:
      //APP_TRACE_INFO1("ATTS_HANDLE_VALUE_CNF, status = %d", pMsg->att.hdr.status);

      amdtps_proc_msg(&pMsg->hdr);
      break;

    case ATTS_CCC_STATE_IND:
      amdtpProcCccState(pMsg);
      break;

    case ATT_MTU_UPDATE_IND:
      APP_TRACE_INFO1("Negotiated MTU %d", ((attEvt_t *)pMsg)->mtu);
      break;

    case DM_CONN_DATA_LEN_CHANGE_IND:
      APP_TRACE_INFO2("DM_CONN_DATA_LEN_CHANGE_IND, Tx=%d, Rx=%d", ((hciLeDataLenChangeEvt_t*)pMsg)->maxTxOctets, ((hciLeDataLenChangeEvt_t*)pMsg)->maxRxOctets);
      //AttcMtuReq((dmConnId_t)(pMsg->hdr.param), 480);//ATT_MAX_MTU);//83); //fixme
      break;
    case DM_RESET_CMPL_IND:
      AttsCalculateDbHash();
      DmSecGenerateEccKeyReq();
      amdtpSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_ADV_START_IND:
      uiEvent = APP_UI_ADV_START;
      break;

    case DM_ADV_STOP_IND:
      uiEvent = APP_UI_ADV_STOP;
      break;

    case DM_CONN_OPEN_IND:
      amdtps_proc_msg(&pMsg->hdr);
//      AttcMtuReq((dmConnId_t)(pMsg->hdr.param), ATT_MAX_MTU);//83); //fixme

//        hciConnSpec_t connSpec;
//        connSpec.connIntervalMin = (7.5/1.25);
//        connSpec.connIntervalMax = (15/1.25);
//        connSpec.connLatency = 0;
//        connSpec.supTimeout = 200;
//        connSpec.minCeLen = 4;//0;
//        connSpec.maxCeLen = 8;//0xffff; //fixme
//        DmConnUpdate(1, &connSpec);

      // PDU length, TX interval (0x148 ~ 0x848)
      //DmConnSetDataLen(1, 27, 0x148);
      DmConnSetDataLen(1, 251, 0x848);

//      AppSlaveSecurityReq(1);
      uiEvent = APP_UI_CONN_OPEN;
      break;

    case DM_CONN_CLOSE_IND:
      amdtpClose(pMsg);
      amdtps_proc_msg(&pMsg->hdr);

      APP_TRACE_INFO1("DM_CONN_CLOSE_IND, reason = 0x%x", pMsg->dm.connClose.reason);

      uiEvent = APP_UI_CONN_CLOSE;
      break;

    case DM_CONN_UPDATE_IND:
      amdtps_proc_msg(&pMsg->hdr);
      break;

    case DM_SEC_PAIR_CMPL_IND:
      DmSecGenerateEccKeyReq();

      uiEvent = APP_UI_SEC_PAIR_CMPL;

      bPairingCompleted = true;

      break;

    case DM_SEC_PAIR_FAIL_IND:
      DmSecGenerateEccKeyReq();

      APP_TRACE_INFO1("DM_SEC_PAIR_FAIL_IND, status = 0x%x", pMsg->dm.pairCmpl.hdr.status);
      bPairingCompleted = false;

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
//
//static void AmdtpsSendTestData(void)
//{
//    static uint8_t counter = 0;
//    uint8_t data[236] = {0};
//    eAmdtpStatus_t status;
//
//    sendDataContinuously = true;
//    data[1] = counter;
//    status = AmdtpsSendPacket(AMDTP_PKT_TYPE_DATA, false, true, data, sizeof(data));
//    if (status != AMDTP_STATUS_SUCCESS)
//    {
//        APP_TRACE_INFO1("AmdtpsSendTestData() failed, status = %d\n", status);
//    }
//    else
//    {
//        counter++;
//    }
//}

void amdtpDtpRecvCback(uint8_t * buf, uint16_t len)
{
#ifdef MEASURE_THROUGHPUT
    static bool measTpStarted = false;
#endif
#if 0
    // reception callback
    // print the received data
    APP_TRACE_INFO0("-----------AMDTP Received data--------------\n");
    APP_TRACE_INFO3("len = %d, buf[0] = %d, buf[1] = %d\n", len, buf[0], buf[1]);
#endif
    if (buf[0] == 1)
    {
        APP_TRACE_INFO0("send test data\n");
        AmdtpsSendTestData();
    }
    else if (buf[0] == 2)
    {
        APP_TRACE_INFO0("send test data stop\n");
        sendDataContinuously = false;
    }
#ifdef MEASURE_THROUGHPUT
    gTotalDataBytesRecev += len;
    if (!measTpStarted)
    {
        measTpStarted = true;
        WsfTimerStartSec(&measTpTimer, 1);
    }
#endif
}

void amdtpDtpTransCback(eAmdtpStatus_t status)
{
    // APP_TRACE_INFO1("amdtpDtpTransCback status = %d\n", status);
    if (status == AMDTP_STATUS_SUCCESS && sendDataContinuously)
    {
        AmdtpsSendTestData();
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AmdtpHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("AmdtpHandlerInit");

  /* store handler ID */
  amdtpHandlerId = handlerId;

  /* Set configuration pointers */
  pAppAdvCfg = (appAdvCfg_t *) &amdtpAdvCfg;
  pAppSlaveCfg = (appSlaveCfg_t *) &amdtpSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &amdtpSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &amdtpUpdateCfg;

  /* Initialize application framework */
  AppSlaveInit();
  AppServerInit();
  /* Set stack configuration pointers */
  pSmpCfg = (smpCfg_t *) &amdtpSmpCfg;

  /* initialize amdtp service server */
  amdtps_init(handlerId, (AmdtpsCfg_t *) &amdtpAmdtpsCfg, amdtpDtpRecvCback, amdtpDtpTransCback);

#ifdef MEASURE_THROUGHPUT
  measTpTimer.handlerId = handlerId;
  measTpTimer.msg.event = AMDTP_MEAS_TP_TIMER_IND;
#endif
}

/*************************************************************************************************/
/*!
 *  \fn     AmdtpHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
    if (pMsg != NULL)
    {
        // APP_TRACE_INFO1("Amdtp got evt %d", pMsg->event);

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
        amdtpProcMsg((amdtpMsg_t *) pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AmdtpStart
 *
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpStart(void)
{
  /* Register for stack callbacks */
  DmRegister(amdtpDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, amdtpDmCback);
  AttRegister(amdtpAttCback);
  AttConnRegister(AppServerConnCback);
  AttsCccRegister(AMDTP_NUM_CCC_IDX, (attsCccSet_t *) amdtpCccSet, amdtpCccCback);

  /* Register for app framework callbacks */
  AppUiBtnRegister(amdtpBtnCback);

  /* Initialize attribute server database */
  SvcCoreGattCbackRegister(GattReadCback, GattWriteCback);
  SvcCoreAddGroup();
  SvcDisAddGroup();
  SvcAmdtpsCbackRegister(NULL, amdtps_write_cback);
  SvcAmdtpsAddGroup();

  /* Set Service Changed CCCD index. */
  GattSetSvcChangedIdx(AMDTP_GATT_SC_CCC_IDX);
  /* Reset the device */
  DmDevReset();
}
