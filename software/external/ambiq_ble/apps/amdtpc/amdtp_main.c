// ****************************************************************************
//
//  amdtp_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of AMDTP client.
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
#include "wsf_buf.h"
#include "hci_api.h"
#include "dm_api.h"
#include "gap_api.h"
#include "att_api.h"
#include "smp_api.h"
#include "app_cfg.h"
#include "app_api.h"
#include "app_db.h"
#include "app_ui.h"
#include "svc_core.h"
#include "svc_ch.h"
#include "gatt_api.h"
#include "amdtp_api.h"
#include "amdtpc_api.h"
#include "calc128.h"
#include "ble_menu.h"
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
  AMDTP_TIMER_IND = AMDTP_MSG_START,    /*! AMDTP tx timeout timer expired */
#ifdef MEASURE_THROUGHPUT
  AMDTP_MEAS_TP_TIMER_IND,
#endif
};

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
struct
{
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];   /*! Cached handle list */
  wsfHandlerId_t    handlerId;                      /*! WSF hander ID */
  bool_t            scanning;                       /*! TRUE if scanning */
  bool_t            autoConnect;                    /*! TRUE if auto-connecting */
  uint8_t           discState;                      /*! Service discovery state */
  uint8_t           hdlListLen;                     /*! Cached handle list length */
} amdtpcCb;

/*! connection control block */
typedef struct
{
  appDbHdl_t          dbHdl;                        /*! Device database record handle type */
  uint8_t             addrType;                     /*! Type of address of device to connect to */
  bdAddr_t            addr;                         /*! Address of device to connect to */
  bool_t              doConnect;                    /*! TRUE to issue connect on scan complete */
} amdtpcConnInfo_t;

amdtpcConnInfo_t amdtpcConnInfo;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for master */
static const appMasterCfg_t amdtpcMasterCfg =
{
  96,                                      /*! The scan interval, in 0.625 ms units */
  48,                                      /*! The scan window, in 0.625 ms units  */
  4000,                                    /*! The scan duration in ms */
  DM_DISC_MODE_NONE,                       /*! The GAP discovery mode */
  DM_SCAN_TYPE_ACTIVE                      /*! The scan type (active or passive) */
};

/*! configurable parameters for security */
static const appSecCfg_t amdtpcSecCfg =
{
  DM_AUTH_BOND_FLAG | DM_AUTH_SC_FLAG,    /*! Authentication and bonding flags */
  DM_KEY_DIST_IRK,                        /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK | DM_KEY_DIST_IRK,      /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! SMP security parameter configuration */
static const smpCfg_t amdtpcSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,                    /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0,                                      /*! Device authentication requirements */
};

/*! Connection parameters */
static const hciConnSpec_t amdtpcConnCfg =
{
  40,                                     /*! Minimum connection interval in 1.25ms units */
  40,                                     /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  0,                                      /*! Unused */
  0                                       /*! Unused */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t amdtpcDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

static const appCfg_t amdtpcAppCfg =
{
  TRUE,                                   /*! TRUE to abort service discovery if service not found */
  TRUE                                    /*! TRUE to disconnect if ATT transaction times out */
};

/*! local IRK */
static uint8_t localIrk[] =
{
  0xA6, 0xD9, 0xFF, 0x70, 0xD6, 0x1E, 0xF0, 0xA4, 0x46, 0x5F, 0x8D, 0x68, 0x19, 0xF3, 0xB4, 0x96
};

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  AMDTPC_DISC_GATT_SVC,      /*! GATT service */
  AMDTPC_DISC_GAP_SVC,       /*! GAP service */
  AMDTPC_DISC_AMDTP_SVC,     /*! AMDTP service */
  AMDTPC_DISC_SVC_MAX        /*! Discovery complete */
};

/*! the Client handle list, amdtpcCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- AMDTPC_DISC_GATT_START
 *  | GATT svc changed handle     |
 *  -------------------------------
 *  | GATT svc changed ccc handle |
 *  ------------------------------- <- AMDTPC_DISC_GAP_START
 *  | GAP central addr res handle |
 *  -------------------------------
 *  | GAP RPA Only handle         |
 *  ------------------------------- <- AMDTPC_DISC_AMDTP_START
 *  | WP handles                  |
 *  | ...                         |
 *  -------------------------------
 */

/*! Start of each service's handles in the the handle list */
#define AMDTPC_DISC_GATT_START     0
#define AMDTPC_DISC_GAP_START      (AMDTPC_DISC_GATT_START + GATT_HDL_LIST_LEN)
#define AMDTPC_DISC_AMDTP_START    (AMDTPC_DISC_GAP_START + GAP_HDL_LIST_LEN)
#define AMDTPC_DISC_HDL_LIST_LEN   (AMDTPC_DISC_AMDTP_START + AMDTP_HDL_LIST_LEN)

/*! Pointers into handle list for each service's handles */
static uint16_t *pAmdtpcGattHdlList = &amdtpcCb.hdlList[AMDTPC_DISC_GATT_START];
static uint16_t *pAmdtpcGapHdlList = &amdtpcCb.hdlList[AMDTPC_DISC_GAP_START];
static uint16_t *pAmdtpcHdlList = &amdtpcCb.hdlList[AMDTPC_DISC_AMDTP_START];


/* LESC OOB configuration */
static dmSecLescOobCfg_t *amdtpcOobCfg;

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/*
 * Data for configuration after service discovery
 */

/* Default value for CCC indications */
const uint8_t amdtpcCccIndVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* Default value for CCC notifications */
const uint8_t amdtpcTxCccNtfVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* Default value for CCC notifications */
const uint8_t amdtpcAckCccNtfVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t amdtpcDiscCfgList[] =
{
  /* Write:  GATT service changed ccc descriptor */
  {amdtpcCccIndVal, sizeof(amdtpcCccIndVal), (GATT_SC_CCC_HDL_IDX + AMDTPC_DISC_GATT_START)},

  /* Write:  AMDTP service TX ccc descriptor */
  {amdtpcTxCccNtfVal, sizeof(amdtpcTxCccNtfVal), (AMDTP_TX_DATA_CCC_HDL_IDX + AMDTPC_DISC_AMDTP_START)},

  /* Write:  AMDTP service TX ccc descriptor */
  {amdtpcAckCccNtfVal, sizeof(amdtpcAckCccNtfVal), (AMDTP_ACK_CCC_HDL_IDX + AMDTPC_DISC_AMDTP_START)},
};

/* Characteristic configuration list length */
#define AMDTPC_DISC_CFG_LIST_LEN   (sizeof(amdtpcDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
//WSF_ASSERT(AMDTPC_DISC_CFG_LIST_LEN <= AMDTPC_DISC_HDL_LIST_LEN);

#ifdef MEASURE_THROUGHPUT
wsfTimer_t measTpTimer;
int gTotalDataBytesRecev = 0;
#endif

/*************************************************************************************************/
/*!
 *  \fn     amdtpcDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t   *pMsg;
  uint16_t  len;
  uint16_t  reportLen;

  if (pDmEvt->hdr.event == DM_SEC_ECC_KEY_IND)
  {
    DmSecSetEccKey(&pDmEvt->eccMsg.data.key);

    if (amdtpcSecCfg.oob)
    {
      uint8_t oobLocalRandom[SMP_RAND_LEN];
      SecRand(oobLocalRandom, SMP_RAND_LEN);
      DmSecCalcOobReq(oobLocalRandom, pDmEvt->eccMsg.data.key.pubKey_x);
    }
  }
  else if (pDmEvt->hdr.event == DM_SEC_CALC_OOB_IND)
  {
    if (amdtpcOobCfg == NULL)
    {
      amdtpcOobCfg = WsfBufAlloc(sizeof(dmSecLescOobCfg_t));
    }

    if (amdtpcOobCfg)
    {
      Calc128Cpy(amdtpcOobCfg->localConfirm, pDmEvt->oobCalcInd.confirm);
      Calc128Cpy(amdtpcOobCfg->localRandom, pDmEvt->oobCalcInd.random);
    }
  }
  else
  {
    len = DmSizeOfEvt(pDmEvt);

    if (pDmEvt->hdr.event == DM_SCAN_REPORT_IND)
    {
      reportLen = pDmEvt->scanReport.len;
    }
    else
    {
      reportLen = 0;
    }

    if ((pMsg = WsfMsgAlloc(len + reportLen)) != NULL)
    {
      memcpy(pMsg, pDmEvt, len);
      if (pDmEvt->hdr.event == DM_SCAN_REPORT_IND)
      {
        pMsg->scanReport.pData = (uint8_t *) ((uint8_t *) pMsg + len);
        memcpy(pMsg->scanReport.pData, pDmEvt->scanReport.pData, reportLen);
      }
      WsfMsgSend(amdtpcCb.handlerId, pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcAttCback
 *
 *  \brief  Application  ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(amdtpcCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcScanStart
 *
 *  \brief  Perform actions on scan start.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcScanStart(dmEvt_t *pMsg)
{
  if (pMsg->hdr.status == HCI_SUCCESS)
  {
    amdtpcCb.scanning = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcScanStop
 *
 *  \brief  Perform actions on scan stop.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcScanStop(dmEvt_t *pMsg)
{
  if (pMsg->hdr.status == HCI_SUCCESS)
  {
    amdtpcCb.scanning = FALSE;
    amdtpcCb.autoConnect = FALSE;

    /* Open connection */
    if (amdtpcConnInfo.doConnect)
    {
      AppConnOpen(amdtpcConnInfo.addrType, amdtpcConnInfo.addr, amdtpcConnInfo.dbHdl);
      amdtpcConnInfo.doConnect = FALSE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcScanReport
 *
 *  \brief  Handle a scan report.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcScanReport(dmEvt_t *pMsg)
{
  uint8_t *pData;
  appDbHdl_t dbHdl;
  bool_t  connect = FALSE;

  /* disregard if not scanning or autoconnecting */
  if (!amdtpcCb.scanning || !amdtpcCb.autoConnect)
  {
    return;
  }

  /* if we already have a bond with this device then connect to it */
  if ((dbHdl = AppDbFindByAddr(pMsg->scanReport.addrType, pMsg->scanReport.addr)) != APP_DB_HDL_NONE)
  {
    /* if this is a directed advertisement where the initiator address is an RPA */
    if (DM_RAND_ADDR_RPA(pMsg->scanReport.directAddr, pMsg->scanReport.directAddrType))
    {
      /* resolve direct address to see if it's addressed to us */
      AppMasterResolveAddr(pMsg, dbHdl, APP_RESOLVE_DIRECT_RPA);
    }
    else
    {
      connect = TRUE;
    }
  }
  /* if the peer device uses an RPA */
  else if (DM_RAND_ADDR_RPA(pMsg->scanReport.addr, pMsg->scanReport.addrType))
  {
    /* reslove advertiser's RPA to see if we already have a bond with this device */
    AppMasterResolveAddr(pMsg, APP_DB_HDL_NONE, APP_RESOLVE_ADV_RPA);
  }
  /* find vendor-specific advertising data */
  else if ((pData = DmFindAdType(DM_ADV_TYPE_MANUFACTURER, pMsg->scanReport.len,
                                 pMsg->scanReport.pData)) != NULL)
  {
    /* check length and vendor ID */
    if (pData[DM_AD_LEN_IDX] >= 3 && BYTES_UINT16_CMP(&pData[DM_AD_DATA_IDX], HCI_ID_ARM))
    {
      connect = TRUE;
    }
  }

  if (connect)
  {
    /* stop scanning and connect */
    amdtpcCb.autoConnect = FALSE;
    AppScanStop();

    /* Store peer information for connect on scan stop */
    amdtpcConnInfo.addrType = pMsg->scanReport.addrType;
    memcpy(amdtpcConnInfo.addr, pMsg->scanReport.addr, sizeof(bdAddr_t));
    amdtpcConnInfo.dbHdl = dbHdl;
    amdtpcConnInfo.doConnect = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcOpen
 *
 *  \brief  Perform UI actions on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcOpen(dmEvt_t *pMsg)
{
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcSetup
 *
 *  \brief  Set up procedures that need to be performed after device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcSetup(dmEvt_t *pMsg)
{
  amdtpcCb.scanning = FALSE;
  amdtpcCb.autoConnect = FALSE;
  amdtpcConnInfo.doConnect = FALSE;

  DmConnSetConnSpec((hciConnSpec_t *) &amdtpcConnCfg);
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcDiscGapCmpl
 *
 *  \brief  GAP service discovery has completed.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcDiscGapCmpl(dmConnId_t connId)
{
  appDbHdl_t dbHdl;

  /* if RPA Only attribute found on peer device */
  if ((pAmdtpcGapHdlList[GAP_RPAO_HDL_IDX] != ATT_HANDLE_NONE) &&
      ((dbHdl = AppDbGetHdl(connId)) != APP_DB_HDL_NONE))
  {
    /* update DB */
    AppDbSetPeerRpao(dbHdl, TRUE);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcBtnCback(uint8_t btn)
{
  dmConnId_t      connId;

  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        break;

      case APP_UI_BTN_1_LONG:
        /* disconnect */
        AppConnClose(connId);
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
        /* if scanning cancel scanning */
        if (amdtpcCb.scanning)
        {
          AppScanStop();
        }
        /* else auto connect */
        else if (!amdtpcCb.autoConnect)
        {
          amdtpcCb.autoConnect = TRUE;
          amdtpcConnInfo.doConnect = FALSE;
          AppScanStart(amdtpcMasterCfg.discMode, amdtpcMasterCfg.scanType,
                       amdtpcMasterCfg.scanDuration);
        }
        break;

      case APP_UI_BTN_1_LONG:
        /* clear bonded device info */
        AppDbDeleteAllRecords();
        break;

      case APP_UI_BTN_1_EX_LONG:
        /* add RPAO characteristic to GAP service -- needed only when DM Privacy enabled */
        SvcCoreGapAddRpaoCh();
        break;

      case APP_UI_BTN_2_EX_LONG:
        /* enable device privacy -- start generating local RPAs every 15 minutes */
        DmDevPrivStart(15 * 60);

        /* set Scanning filter policy to accept directed advertisements with RPAs */
        DmDevSetFilterPolicy(DM_FILT_POLICY_MODE_SCAN, HCI_FILT_RES_INIT);
        break;

      default:
        break;
    }
  }
}

void AmdtpcScanStart(void)
{
    AppScanStart(amdtpcMasterCfg.discMode, amdtpcMasterCfg.scanType,
                       amdtpcMasterCfg.scanDuration);
}

void AmdtpcScanStop(void)
{
    AppScanStop();
}

void AmdtpcConnOpen(uint8_t idx)
{
    appDevInfo_t *devInfo;
    devInfo = AppScanGetResult(idx);
    if (devInfo)
    {
        AppConnOpen(devInfo->addrType, devInfo->addr, NULL);
    }
    else
    {
        APP_TRACE_INFO0("AmdtpcConnOpen() devInfo = NULL\n");
    }
}

bool sendDataContinuously = false;
void AmdtpcSendTestData(void)
{
    static uint8_t counter = 0;
    uint8_t data[236] = {0};
    eAmdtpStatus_t status;

    sendDataContinuously = true;
    data[1] = counter;
    status = AmdtpcSendPacket(AMDTP_PKT_TYPE_DATA, false, true, data, sizeof(data));
    if (status != AMDTP_STATUS_SUCCESS)
    {
        APP_TRACE_INFO1("AmdtpcSendTestData() failed, status = %d\n", status);
    }
    else
    {
        counter++;
    }
}

void AmdtpcSendTestDataStop(void)
{
    sendDataContinuously = false;
}

void AmdtpcRequestServerSend(void)
{
    uint8_t data[4] = {0};
    eAmdtpStatus_t status;

    data[0] = 1;
    status = AmdtpcSendPacket(AMDTP_PKT_TYPE_DATA, false, true, data, sizeof(data));
    if (status != AMDTP_STATUS_SUCCESS)
    {
        APP_TRACE_INFO1("AmdtpcRequestServerSend() failed, status = %d\n", AmdtpcSendTestData);
    }
}

void AmdtpcRequestServerSendStop(void)
{
    uint8_t data[4] = {0};
    eAmdtpStatus_t status;

    data[0] = 2;
    status = AmdtpcSendPacket(AMDTP_PKT_TYPE_DATA, false, true, data, sizeof(data));
    if (status != AMDTP_STATUS_SUCCESS)
    {
        APP_TRACE_INFO1("AmdtpcRequestServerSend() failed, status = %d\n", AmdtpcSendTestData);
    }
}

void amdtpDtpRecvCback(uint8_t * buf, uint16_t len)
{
#ifdef MEASURE_THROUGHPUT
    static bool measTpStarted = false;
#endif
    // reception callback
    // print the received data
#if 0
    APP_TRACE_INFO0("-----------AMDTP Received data--------------\n");
    APP_TRACE_INFO3("len = %d, buf[0] = %d, buf[1] = %d\n", len, buf[0], buf[1]);
#endif
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
    APP_TRACE_INFO1("amdtpDtpTransCback status = %d\n", status);
    if (status == AMDTP_STATUS_SUCCESS && sendDataContinuously)
    {
        AmdtpcSendTestData();
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amdtpcDiscCback
 *
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, amdtpcCb.hdlListLen, amdtpcCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* initiate security */
      AppMasterSecurityReq(connId);
      break;

    case APP_DISC_READ_DATABASE_HASH:
      /* Read peer's database hash */
      AppDiscReadDatabaseHash(connId);
      break;

    case APP_DISC_START:
      /* initialize discovery state */
      amdtpcCb.discState = AMDTPC_DISC_GATT_SVC;

      /* discover GATT service */
      GattDiscover(connId, pAmdtpcGattHdlList);
      break;

    case APP_DISC_FAILED:
      if (pAppCfg->abortDisc)
      {
        /* if discovery failed for proprietary data service then disconnect */
        if (amdtpcCb.discState == AMDTPC_DISC_AMDTP_SVC)
        {
          AppConnClose(connId);
          break;
        }
      }
      /* else fall through and continue discovery */

    case APP_DISC_CMPL:
      /* next discovery state */
      amdtpcCb.discState++;

      if (amdtpcCb.discState == AMDTPC_DISC_GAP_SVC)
      {
        /* discover GAP service */
        GapDiscover(connId, pAmdtpcGapHdlList);
      }
      else if (amdtpcCb.discState == AMDTPC_DISC_AMDTP_SVC)
      {
        /* discover proprietary data service */
        AmdtpcDiscover(connId, pAmdtpcHdlList);
      }
      else
      {
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* GAP service discovery completed */
        amdtpcDiscGapCmpl(connId);

        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, AMDTPC_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) amdtpcDiscCfgList, AMDTPC_DISC_HDL_LIST_LEN, amdtpcCb.hdlList);
      }
      break;

    case APP_DISC_CFG_START:
        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, AMDTPC_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) amdtpcDiscCfgList, AMDTPC_DISC_HDL_LIST_LEN, amdtpcCb.hdlList);
      break;

    case APP_DISC_CFG_CMPL:
      AppDiscComplete(connId, status);
      amdtpc_start(pAmdtpcHdlList[AMDTP_RX_HDL_IDX], pAmdtpcHdlList[AMDTP_ACK_HDL_IDX], AMDTP_TIMER_IND);
      break;

    case APP_DISC_CFG_CONN_START:
      /* no connection setup configuration */
      break;

    default:
      break;
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
 *  \fn     amdtpcProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amdtpcProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;

  switch(pMsg->hdr.event)
  {
    case AMDTP_TIMER_IND:
      amdtpc_proc_msg(&pMsg->hdr);
      break;

#ifdef MEASURE_THROUGHPUT
    case AMDTP_MEAS_TP_TIMER_IND:
      showThroughput();
      break;
#endif

    case ATTC_HANDLE_VALUE_NTF:
      amdtpc_proc_msg(&pMsg->hdr);
      break;

    case ATTC_WRITE_CMD_RSP:
      amdtpc_proc_msg(&pMsg->hdr);
      break;

    case DM_RESET_CMPL_IND:
      AttsCalculateDbHash();
      DmSecGenerateEccKeyReq();
      amdtpcSetup(pMsg);
#ifdef BLE_MENU
      BleMenuInit();
#endif
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_SCAN_START_IND:
      amdtpcScanStart(pMsg);
      uiEvent = APP_UI_SCAN_START;
      break;

    case DM_SCAN_STOP_IND:
      amdtpcScanStop(pMsg);
#ifdef BLE_MENU
      am_menu_printf("scan stop\r\n");
#endif
      uiEvent = APP_UI_SCAN_STOP;
      break;

    case DM_SCAN_REPORT_IND:
      amdtpcScanReport(pMsg);
      break;

    case DM_CONN_OPEN_IND:
      amdtpcOpen(pMsg);
#ifdef BLE_MENU
      am_menu_printf(" Connection opened\r\n");
#endif
      uiEvent = APP_UI_CONN_OPEN;
      break;

    case DM_CONN_CLOSE_IND:
      amdtpc_proc_msg(&pMsg->hdr);
      uiEvent = APP_UI_CONN_CLOSE;
      break;

    case DM_SEC_PAIR_CMPL_IND:
      uiEvent = APP_UI_SEC_PAIR_CMPL;
      break;

    case DM_SEC_PAIR_FAIL_IND:
      uiEvent = APP_UI_SEC_PAIR_FAIL;
      break;

    case DM_SEC_ENCRYPT_IND:
      uiEvent = APP_UI_SEC_ENCRYPT;
      break;

    case DM_SEC_ENCRYPT_FAIL_IND:
      uiEvent = APP_UI_SEC_ENCRYPT_FAIL;
      break;

    case DM_SEC_AUTH_REQ_IND:
      if (pMsg->authReq.oob)
      {
        dmConnId_t connId = (dmConnId_t) pMsg->hdr.param;

        /* TODO: Perform OOB Exchange with the peer. */
        /* TODO: Fill datsOobCfg peerConfirm and peerRandom with value passed out of band */

        if (amdtpcOobCfg != NULL)
        {
          DmSecSetOob(connId, amdtpcOobCfg);
        }

        DmSecAuthRsp(connId, 0, NULL);
      }
      else
      {
        AppHandlePasskey(&pMsg->authReq);
      }
      break;

    case DM_SEC_COMPARE_IND:
      AppHandleNumericComparison(&pMsg->cnfInd);
      break;

    case DM_ADV_NEW_ADDR_IND:
      break;

    case DM_VENDOR_SPEC_CMD_CMPL_IND:
      {
        #if defined(AM_PART_APOLLO) || defined(AM_PART_APOLLO2)
       
          uint8_t *param_ptr = &pMsg->vendorSpecCmdCmpl.param[0];
        
          switch (pMsg->vendorSpecCmdCmpl.opcode)
          {
            case 0xFC20: //read at address
            {
              uint32_t read_value;

              BSTREAM_TO_UINT32(read_value, param_ptr);

              APP_TRACE_INFO3("VSC 0x%0x complete status %x param %x", 
                pMsg->vendorSpecCmdCmpl.opcode, 
                pMsg->hdr.status,
                read_value);
            }
            break;
            default:
                APP_TRACE_INFO2("VSC 0x%0x complete status %x",
                    pMsg->vendorSpecCmdCmpl.opcode,
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
 *  \fn     AmdtpcHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpcHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("AmdtpcHandlerInit");

  /* store handler ID */
  amdtpcCb.handlerId = handlerId;

  /* set handle list length */
  amdtpcCb.hdlListLen = AMDTPC_DISC_HDL_LIST_LEN;

  /* Set configuration pointers */
  pAppMasterCfg = (appMasterCfg_t *) &amdtpcMasterCfg;
  pAppSecCfg = (appSecCfg_t *) &amdtpcSecCfg;
  pAppDiscCfg = (appDiscCfg_t *) &amdtpcDiscCfg;
  pAppCfg = (appCfg_t *)&amdtpcAppCfg;
  pSmpCfg = (smpCfg_t *) &amdtpcSmpCfg;

  /* Initialize application framework */
  AppMasterInit();
  AppDiscInit();

  /* Set IRK for the local device */
  DmSecSetLocalIrk(localIrk);
  amdtpc_init(handlerId, amdtpDtpRecvCback, amdtpDtpTransCback);

#ifdef MEASURE_THROUGHPUT
  measTpTimer.handlerId = handlerId;
  measTpTimer.msg.event = AMDTP_MEAS_TP_TIMER_IND;
#endif
}

/*************************************************************************************************/
/*!
 *  \fn     AmdtpcHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpcHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Amdtpc got evt %d", pMsg->event);

    /* process ATT messages */
    if (pMsg->event <= ATT_CBACK_END)
    {
      /* process discovery-related ATT messages */
      AppDiscProcAttMsg((attEvt_t *) pMsg);
    }
    /* process DM messages */
    else if (pMsg->event <= DM_CBACK_END)
    {
      /* process advertising and connection-related messages */
      AppMasterProcDmMsg((dmEvt_t *) pMsg);

      /* process security-related messages */
      AppMasterSecProcDmMsg((dmEvt_t *) pMsg);

      /* process discovery-related messages */
      AppDiscProcDmMsg((dmEvt_t *) pMsg);
    }

    /* perform profile and user interface-related operations */
    amdtpcProcMsg((dmEvt_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     DatcStart
 *
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AmdtpcStart(void)
{
  /* Register for stack callbacks */
  DmRegister(amdtpcDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, amdtpcDmCback);
  AttRegister(amdtpcAttCback);

  /* Register for app framework button callbacks */
  AppUiBtnRegister(amdtpcBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(amdtpcDiscCback);

  /* Initialize attribute server database */
  SvcCoreAddGroup();

  /* Reset the device */
  DmDevReset();
}
