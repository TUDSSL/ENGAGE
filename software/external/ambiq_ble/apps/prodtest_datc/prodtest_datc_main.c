// ****************************************************************************
//
//  prodtest_datc_main.c
//! @file
//!
//! @brief Proprietary data transfer client sample application for Nordic-ble.
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

/*************************************************************************************************/
/*
 *  file   prodtest_datc_main.c
 *  brief  Proprietary data transfer client sample application for Nordic-ble.
 *
 *          $Date: 2017-07-14 15:35:43 -0500 (Fri, 14 Jul 2017) $
 *          $Revision: 12352 $
 *
 *  Copyright (c) 2012-2017 ARM Ltd., all rights reserved.
 *  ARM Ltd. confidential and proprietary.
 *
 *  IMPORTANT.  Your use of this file is governed by a Software License Agreement
 *  ("Agreement") that must be accepted in order to download or otherwise receive a
 *  copy of this file.  You may not use or copy this file for any purpose other than
 *  as described in the Agreement.  If you do not agree to all of the terms of the
 *  Agreement do not use this file and delete all copies in your possession or control;
 *  if you do not have a copy of the Agreement, you must contact ARM Ltd. prior
 *  to any use, copying or further distribution of this software.
 */
/*************************************************************************************************/

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
#include "wpc_api.h"
#include "prodtest_datc_api.h"
#include "calc128.h"
#if WDXC_INCLUDED == TRUE
#include "wsf_efs.h"
#include "wdxc_api.h"
#include "wdxs_defs.h"
#endif /* WDXC_INCLUDED */

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "hci_apollo_config.h"
#include "hci_drv_em9304.h"
#include "gatt_api.h"

/**************************************************************************************************
Macros
**************************************************************************************************/
#if CS50_INCLUDED == TRUE
/* PHY Test Modes */
#define DATC_PHY_1M                       1
#define DATC_PHY_2M                       2
#define DATC_PHY_CODED                    3
#endif /* CS50_INCLUDED */

#if WDXC_INCLUDED == TRUE
/* Size of WDXC file discovery dataset */
#define DATC_WDXC_MAX_FILES               4

/*! WSF message event starting value */
#define DATC_MSG_START                    0xA0

/*! Data rate timer period in seconds */
#define DATC_WDXS_DATA_RATE_TIMEOUT       4

/*! WSF message event enumeration */
enum
{
  DATC_WDXS_DATA_RATE_TIMER_IND = DATC_MSG_START, /*! Data rate timer expired */
};
#endif /* WDXC_INCLUDED */

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
struct
{
  uint16_t          hdlList[DM_CONN_MAX][APP_DB_HDL_LIST_LEN];   /*! Cached handle list */
  wsfHandlerId_t    handlerId;                      /*! WSF hander ID */
  bool_t            scanning;                       /*! TRUE if scanning */
  bool_t            autoConnect;                    /*! TRUE if auto-connecting */
  uint8_t           discState[DM_CONN_MAX];         /*! Service discovery state */
  uint8_t           hdlListLen;                     /*! Cached handle list length */
  uint8_t           btnConnId;                      /*! The index of the connection ID for button presses */
#if CS50_INCLUDED == TRUE
  uint8_t           phyMode[DM_CONN_MAX];           /*! PHY Test Mode */
#endif /* CS50_INCLUDED */
#if WDXC_INCLUDED == TRUE
  bool_t            streaming[DM_CONN_MAX];         /*! TRUE if WDXC Stream running */
  wsfEfsHandle_t    streamHandle[DM_CONN_MAX];      /*! WDXC Stream file handle */
  wsfTimer_t        dataRateTimer[DM_CONN_MAX];     /* Timer for data rate calculation */
  uint32_t          rxCount[DM_CONN_MAX];           /* Streaming data count for data rate calculation */
  uint32_t          lastCount[DM_CONN_MAX];         /* Streaming data count for data rate calculation */
  wsfEfsFileInfo_t  fileList[DM_CONN_MAX][DATC_WDXC_MAX_FILES]; /* Buffer to hold WDXC file list */
#endif /* WDXC_INCLUDED */
} datcCb;

/*! connection control block */
typedef struct
{
  appDbHdl_t          dbHdl;                        /*! Device database record handle type */
  uint8_t             addrType;                     /*! Type of address of device to connect to */
  bdAddr_t            addr;                         /*! Address of device to connect to */
  bool_t              doConnect;                    /*! TRUE to issue connect on scan complete */
} datcConnInfo_t;

datcConnInfo_t datcConnInfo;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for master */
static const appMasterCfg_t datcMasterCfg =
{
  64,                                      /*! The scan interval, in 0.625 ms units */
  48,                                      /*! The scan window, in 0.625 ms units  */
  4000,                                    /*! The scan duration in ms */
  DM_DISC_MODE_NONE,                       /*! The GAP discovery mode */
  DM_SCAN_TYPE_ACTIVE                      /*! The scan type (active or passive) */
};

/*! configurable parameters for security */
static const appSecCfg_t datcSecCfg =
{
  0,    /*! Authentication and bonding flags */
  DM_KEY_DIST_IRK,                        /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK | DM_KEY_DIST_IRK,      /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                    /*! TRUE to initiate security upon connection */
};

/*! SMP security parameter configuration */
static const smpCfg_t datcSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,                    /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0,                                      /*! Device authentication requirements */
};

/*! Connection parameters */
static const hciConnSpec_t datcConnCfg =
{
  10,                                     /*! Minimum connection interval in 1.25ms units */
  10,                                     /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  100,                                    /*! Supervision timeout in 10ms units */
  0,                                      /*! Unused */
  0                                       /*! Unused */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t datcDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

static const appCfg_t datcAppCfg =
{
  FALSE,                                  /*! TRUE to abort service discovery if service not found */
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
  DATC_DISC_GATT_SVC,      /*! GATT service */
  DATC_DISC_GAP_SVC,       /*! GAP service */
  DATC_DISC_WP_SVC,        /*! ARM Ltd. proprietary service */
#if WDXC_INCLUDED == TRUE
  DATC_DISC_WDXC_SCV,      /*! ARM Ltd. Wireless Data Exchange service */
#endif /* WDXC_INCLUDED */
  DATC_DISC_SVC_MAX        /*! Discovery complete */
};

/*! the Client handle list, datcCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- DATC_DISC_GATT_START
 *  | GATT svc changed handle     |
 *  -------------------------------
 *  | GATT svc changed ccc handle |
 *  ------------------------------- <- DATC_DISC_GAP_START
 *  | GAP central addr res handle |
 *  -------------------------------
 *  | GAP RPA Only handle         |
 *  ------------------------------- <- DATC_DISC_WP_START
 *  | WP handles                  |
 *  | ...                         |
 *  -------------------------------
 */

/*! Start of each service's handles in the the handle list */
#define DATC_DISC_GATT_START       0
#define DATC_DISC_GAP_START        (DATC_DISC_GATT_START + GATT_HDL_LIST_LEN)
#define DATC_DISC_WP_START         (DATC_DISC_GAP_START + GAP_HDL_LIST_LEN)
#if WDXC_INCLUDED == TRUE
#define DATC_DISC_WDXC_START       (DATC_DISC_WP_START + WPC_P1_HDL_LIST_LEN)
#define DATC_DISC_HDL_LIST_LEN     (DATC_DISC_WDXC_START + WDXC_HDL_LIST_LEN)
#else
#define DATC_DISC_HDL_LIST_LEN     (DATC_DISC_WP_START + WPC_P1_HDL_LIST_LEN)
#endif /* WDXC_INCLUDED */

/*! Pointers into handle list for each service's handles */
static uint16_t *pDatcGattHdlList[DM_CONN_MAX];
static uint16_t *pDatcGapHdlList[DM_CONN_MAX];
static uint16_t *pDatcWpHdlList[DM_CONN_MAX];
#if WDXC_INCLUDED == TRUE
static uint16_t *pDatcWdxHdlList[DM_CONN_MAX];
#endif /* WDXC_INCLUDED */

/* LESC OOB configuration */
static dmSecLescOobCfg_t *datcOobCfg;

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/*
 * Data for configuration after service discovery
 */

/* Default value for CCC indications */
const uint8_t datcCccIndVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* Default value for CCC notifications */
const uint8_t datcCccNtfVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t datcDiscCfgList[] =
{
  /* Write:  GATT service changed ccc descriptor */
  {datcCccIndVal, sizeof(datcCccIndVal), (GATT_SC_CCC_HDL_IDX + DATC_DISC_GATT_START)},

  /* Write:  Proprietary data service changed ccc descriptor */
  {datcCccNtfVal, sizeof(datcCccNtfVal), (WPC_P1_NA_CCC_HDL_IDX + DATC_DISC_WP_START)},

#if WDXC_INCLUDED == TRUE
  /* Write:  WDXC ccc descriptors */
  {datcCccNtfVal, sizeof(datcCccNtfVal), (WDXC_DC_CCC_HDL_IDX + DATC_DISC_WDXC_START)},
  {datcCccNtfVal, sizeof(datcCccNtfVal), (WDXC_FTC_CCC_HDL_IDX + DATC_DISC_WDXC_START)},
  {datcCccNtfVal, sizeof(datcCccNtfVal), (WDXC_FTD_CCC_HDL_IDX + DATC_DISC_WDXC_START)},
  {datcCccNtfVal, sizeof(datcCccNtfVal), (WDXC_AU_CCC_HDL_IDX + DATC_DISC_WDXC_START)},
#endif /* WDXC_INCLUDED */
};

/* Characteristic configuration list length */
#define DATC_DISC_CFG_LIST_LEN   (sizeof(datcDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(DATC_DISC_CFG_LIST_LEN <= DATC_DISC_HDL_LIST_LEN);

/*************************************************************************************************/
/*!
 *  \fn     datcDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t   *pMsg;
  uint16_t  len;
  uint16_t  reportLen;

  if (pDmEvt->hdr.event == DM_SEC_ECC_KEY_IND)
  {
    DmSecSetEccKey(&pDmEvt->eccMsg.data.key);

    if (datcSecCfg.oob)
    {
      uint8_t oobLocalRandom[SMP_RAND_LEN];
      SecRand(oobLocalRandom, SMP_RAND_LEN);
      DmSecCalcOobReq(oobLocalRandom, pDmEvt->eccMsg.data.key.pubKey_x);
    }
  }
  else if (pDmEvt->hdr.event == DM_SEC_CALC_OOB_IND)
  {
    if (datcOobCfg == NULL)
    {
      datcOobCfg = WsfBufAlloc(sizeof(dmSecLescOobCfg_t));
    }

    if (datcOobCfg)
    {
      Calc128Cpy(datcOobCfg->localConfirm, pDmEvt->oobCalcInd.confirm);
      Calc128Cpy(datcOobCfg->localRandom, pDmEvt->oobCalcInd.random);
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
      WsfMsgSend(datcCb.handlerId, pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcAttCback
 *
 *  \brief  Application  ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(datcCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcScanStart
 *
 *  \brief  Perform actions on scan start.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcScanStart(dmEvt_t *pMsg)
{
  if (pMsg->hdr.status == HCI_SUCCESS)
  {
    datcCb.scanning = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcScanStop
 *
 *  \brief  Perform actions on scan stop.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcScanStop(dmEvt_t *pMsg)
{
  if (pMsg->hdr.status == HCI_SUCCESS)
  {
    datcCb.scanning = FALSE;
    datcCb.autoConnect = FALSE;

    /* Open connection */
    if (datcConnInfo.doConnect)
    {
      AppConnOpen(datcConnInfo.addrType, datcConnInfo.addr, datcConnInfo.dbHdl);
      datcConnInfo.doConnect = FALSE;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcScanReport
 *
 *  \brief  Handle a scan report.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcScanReport(dmEvt_t *pMsg)
{
  uint8_t *pData;
  appDbHdl_t dbHdl;
  bool_t  connect = FALSE;

  /* disregard if not scanning or autoconnecting */
  if (!datcCb.scanning || !datcCb.autoConnect)
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

  APP_TRACE_INFO1("Device report RSSI %d", pMsg->scanReport.rssi);


  if (connect)
  {
    /* stop scanning and connect */
    datcCb.autoConnect = FALSE;
    AppScanStop();

    /* Store peer information for connect on scan stop */
    datcConnInfo.addrType = DmHostAddrType(pMsg->scanReport.addrType);
    memcpy(datcConnInfo.addr, pMsg->scanReport.addr, sizeof(bdAddr_t));
    datcConnInfo.dbHdl = dbHdl;
    datcConnInfo.doConnect = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcOpen
 *
 *  \brief  Perform UI actions on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcOpen(dmEvt_t *pMsg)
{
#if CS50_INCLUDED == TRUE
  datcCb.phyMode[pMsg->hdr.param-1] = DATC_PHY_1M;
#endif /* CS50_INCLUDED */

#if WDXC_INCLUDED == TRUE
  datcCb.streamHandle[pMsg->hdr.param-1] = WSF_EFS_INVALID_HANDLE;
#endif /* WDXC_INCLUDED */
}

static void datcSendData(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \fn     datcValueNtf
 *
 *  \brief  Process a received ATT notification.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcValueNtf(attEvt_t *pMsg)
{
#if WDXC_INCLUDED == FALSE

  dmConnId_t  connId = datcCb.btnConnId;
  /* print the received data */
  APP_TRACE_INFO0((const char *) pMsg->pValue);

  // send another data
  datcSendData(connId);
#endif /* WDXC_INCLUDED */
}

/*************************************************************************************************/
/*!
 *  \fn     datcSetup
 *
 *  \brief  Set up procedures that need to be performed after device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcSetup(dmEvt_t *pMsg)
{
  datcCb.scanning = FALSE;
  datcCb.autoConnect = FALSE;
  datcConnInfo.doConnect = FALSE;

  DmConnSetConnSpec((hciConnSpec_t *) &datcConnCfg);
}

#if WDXC_INCLUDED == FALSE
/*************************************************************************************************/
/*!
 *  \fn     datcSendData
 *
 *  \brief  Send example data.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcSendData(dmConnId_t connId)
{
  uint8_t str[] = "hello world";

  if (pDatcWpHdlList[connId-1][WPC_P1_DAT_HDL_IDX] != ATT_HANDLE_NONE)
  {
    AttcWriteCmd(connId, pDatcWpHdlList[connId-1][WPC_P1_DAT_HDL_IDX], sizeof(str), str);
  }
}
#endif /* WDXC_INCLUDED */

/*************************************************************************************************/
/*!
 *  \fn     datcDiscGapCmpl
 *
 *  \brief  GAP service discovery has completed.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcDiscGapCmpl(dmConnId_t connId)
{
  appDbHdl_t dbHdl;

  /* if RPA Only attribute found on peer device */
  if ((pDatcGapHdlList[connId-1][GAP_RPAO_HDL_IDX] != ATT_HANDLE_NONE) &&
      ((dbHdl = AppDbGetHdl(connId)) != APP_DB_HDL_NONE))
  {
    /* update DB */
    AppDbSetPeerRpao(dbHdl, TRUE);
  }
}

#if WDXC_INCLUDED == TRUE
/*************************************************************************************************/
/*!
 *  \fn     datcWdxcProcessDataRateTimeout
 *
 *  \brief  Calculate the data rate.
 *
 *  \param  None.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcWdxcProcessDataRateTimeout(dmConnId_t connId)
{
  uint32_t bps;
  (void) bps; /* Suppress warning when APP_TRACE disabled */

  /* Calculate data rate */
  bps = (datcCb.rxCount[connId-1] - datcCb.lastCount[connId-1]) * 8 / DATC_WDXS_DATA_RATE_TIMEOUT;
  datcCb.lastCount[connId-1] = datcCb.rxCount[connId-1];

  APP_TRACE_INFO1("Streaming data rate %d bits per second", bps);

  /* Restart the timer */
  datcCb.dataRateTimer[connId-1].msg.event = DATC_WDXS_DATA_RATE_TIMER_IND;
  datcCb.dataRateTimer[connId-1].msg.param = connId;
  datcCb.dataRateTimer[connId-1].handlerId = datcCb.handlerId;

  WsfTimerStartSec(&datcCb.dataRateTimer[connId-1], DATC_WDXS_DATA_RATE_TIMEOUT);
}

/*************************************************************************************************/
/*!
 *  \fn     datcWdxcFtdCallback
 *
 *  \brief  WDXC File Transfer Data Callback.
 *
 *  \param  connId    Connection ID.
 *  \param  fileHdl   Handle of the file.
 *  \param  len       length of pData in bytes.
 *  \param  pData     File data.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcWdxcFtdCallback(dmConnId_t connId, uint16_t fileHdl, uint16_t len, uint8_t *pData)
{
  datcCb.rxCount[connId-1] += len;
}

/*************************************************************************************************/
/*!
 *  \fn     datcWdxcFtcCallback
 *
 *  \brief  WDXC File Transfer Control Callback.
 *
 *  \param  connId    Connection ID.
 *  \param  op        Control operation.
 *  \param  status    Status of operation.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcWdxcFtcCallback(dmConnId_t connId, uint16_t handle, uint8_t op, uint8_t status)
{
  if (op == WDXS_FTC_OP_EOF)
  {
    if (handle == WDXS_FLIST_HANDLE)
    {
      /* File Discovery complete */
      int8_t i;

      for (i = 0; i < DATC_WDXC_MAX_FILES; i++)
      {
        if (datcCb.fileList[connId-1][i].attributes.type == WSF_EFS_FILE_TYPE_STREAM)
        {
          /* Find the file handle of a data stream in the datcWdxcFileList */
          datcCb.streamHandle[connId-1] = datcCb.fileList[connId-1][i].handle;
          return;
        }
      }
    }
    else if (handle == datcCb.streamHandle[connId-1])
    {
      datcCb.streaming[connId-1] = FALSE;

      /* Stop the data rate calculation timer */
      WsfTimerStop(&datcCb.dataRateTimer[connId-1]);
    }
  }
  else if (op == WDXS_FTC_OP_GET_RSP)
  {
    if ((handle == datcCb.streamHandle[connId-1]) && (status == WDXS_FTC_ST_SUCCESS))
    {
      /* WDXC data stream started, initialize the data rate count and start the data rate timer */
      datcCb.streaming[connId-1] = TRUE;
      datcCb.rxCount[connId-1] = 0;
      datcCb.lastCount[connId-1] = 0;
      datcWdxcProcessDataRateTimeout(connId);
    }
  }
}
#endif /* WDXC_INCLUDED */

/*************************************************************************************************/
/*!
 *  \fn     datcBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcBtnCback(uint8_t btn)
{
  {
    switch (btn)
    {
        case APP_UI_BTN_1_SHORT:
          /* if scanning cancel scanning */
          if (datcCb.scanning)
          {
            AppScanStop();
          }
          /* else auto connect */
          else if (!datcCb.autoConnect)
          {
            datcCb.autoConnect = TRUE;
            datcConnInfo.doConnect = FALSE;
            AppScanStart(datcMasterCfg.discMode, datcMasterCfg.scanType,
              datcMasterCfg.scanDuration);
          }
          break;

        case APP_UI_BTN_1_MED:
          /* Increment connection ID buttons apply to */
          if (++datcCb.btnConnId > DM_CONN_MAX)
          {
            datcCb.btnConnId = 1;
          }
          APP_TRACE_INFO1("ConnID for Button Press: %d", datcCb.btnConnId);
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

/*************************************************************************************************/
/*!
 *  \fn     datcDiscCback
 *
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, datcCb.hdlListLen, datcCb.hdlList[connId-1]);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* initiate security */
      AppMasterSecurityReq(connId);
      break;

    case APP_DISC_START:
      /* initialize discovery state */
      datcCb.discState[connId-1] = DATC_DISC_GATT_SVC;

      /* discover GATT service */
      GattDiscover(connId, pDatcGattHdlList[connId-1]);
      break;

    case APP_DISC_FAILED:
      if (pAppCfg->abortDisc)
      {
        /* if discovery failed for proprietary data service then disconnect */
        if (datcCb.discState[connId-1] == DATC_DISC_WP_SVC)
        {
          AppConnClose(connId);
          break;
        }
      }
      /* else fall through and continue discovery */

    case APP_DISC_CMPL:
      /* next discovery state */
      datcCb.discState[connId-1]++;

      if (datcCb.discState[connId-1] == DATC_DISC_GAP_SVC)
      {
        /* discover GAP service */
        GapDiscover(connId, pDatcGapHdlList[connId-1]);
      }
      else if (datcCb.discState[connId-1] == DATC_DISC_WP_SVC)
      {
        /* discover proprietary data service */
        WpcP1Discover(connId, pDatcWpHdlList[connId-1]);
      }
#if WDXC_INCLUDED == TRUE
      else if (datcCb.discState[connId-1] == DATC_DISC_WDXC_SCV)
      {
        WdxcWdxsDiscover(connId, pDatcWdxHdlList[connId-1]);
      }
#endif /* WDXC_INCLUDED */
      else
      {
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* GAP service discovery completed */
        datcDiscGapCmpl(connId);

        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, DATC_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) datcDiscCfgList, DATC_DISC_HDL_LIST_LEN, datcCb.hdlList[connId-1]);
      }

      AppConnClose(datcCb.btnConnId);

      break;

    case APP_DISC_CFG_START:
        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, DATC_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) datcDiscCfgList, DATC_DISC_HDL_LIST_LEN, datcCb.hdlList[connId-1]);
      break;

    case APP_DISC_CFG_CMPL:
      AppDiscComplete(connId, status);
      break;

    case APP_DISC_CFG_CONN_START:
      /* no connection setup configuration */
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;

  switch(pMsg->hdr.event)
  {
#if WDXC_INCLUDED == TRUE
    case DATC_WDXS_DATA_RATE_TIMER_IND:
      datcWdxcProcessDataRateTimeout((dmConnId_t) pMsg->hdr.param);
      break;
#endif /* WDXC_INCLUDED */

    case ATTC_HANDLE_VALUE_NTF:
      datcValueNtf((attEvt_t *) pMsg);
      break;

    case DM_RESET_CMPL_IND:
      AttsCalculateDbHash();
      DmSecGenerateEccKeyReq();
      datcSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;

      HciVsEM_SetRfPowerLevelEx(TX_POWER_LEVEL_PLUS_0P4_dBm);

      break;

    case DM_SCAN_START_IND:
      datcScanStart(pMsg);
      uiEvent = APP_UI_SCAN_START;

      // set start flag and set status to failure
      am_hal_gpio_out_bit_set(TEST_START_GPIO);
      am_hal_gpio_out_bit_clear(TEST_STATUS_GPIO);

      break;

    case DM_SCAN_STOP_IND:
      // First check if the target is found
      if ( datcConnInfo.doConnect == FALSE )
      {
        // set end flag for start gpio
        am_hal_gpio_out_bit_clear(TEST_START_GPIO);
      }

      datcScanStop(pMsg);
      uiEvent = APP_UI_SCAN_STOP;


      break;

    case DM_SCAN_REPORT_IND:
      datcScanReport(pMsg);
      break;

    case DM_CONN_OPEN_IND:
      datcOpen(pMsg);
      uiEvent = APP_UI_CONN_OPEN;

      // set end flag for start gpio
      am_hal_gpio_out_bit_clear(TEST_START_GPIO);

      // set status to success
      am_hal_gpio_out_bit_set(TEST_STATUS_GPIO);

      break;

    case DM_CONN_CLOSE_IND:
#if WDXC_INCLUDED == TRUE
      WsfTimerStop(&datcCb.dataRateTimer[pMsg->hdr.param - 1]);
      datcCb.streaming[pMsg->hdr.param - 1] = FALSE;
#endif /* WDXC_INCLUDED */
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

        if (datcOobCfg != NULL)
        {
          DmSecSetOob(connId, datcOobCfg);
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

#if CS50_INCLUDED == TRUE
    case DM_PHY_UPDATE_IND:
      APP_TRACE_INFO2("DM_PHY_UPDATE_IND - RX: %d, TX: %d", pMsg->phyUpdate.rxPhy, pMsg->phyUpdate.txPhy);
      datcCb.phyMode[pMsg->hdr.param-1] = pMsg->phyUpdate.rxPhy;
      break;
#endif /* CS50_INCLUDED */

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

  // start testing upon reset complete.
  if ( pMsg->hdr.event == DM_RESET_CMPL_IND )
  {
      datcBtnCback(APP_UI_BTN_1_SHORT);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     DatcHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatcHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("DatcHandlerInit");

  /* store handler ID */
  datcCb.handlerId = handlerId;

  /* set handle list length */
  datcCb.hdlListLen = DATC_DISC_HDL_LIST_LEN;

  /* Initialize the connection id for button actions */
  datcCb.btnConnId = 1;

  /* Set configuration pointers */
  pAppMasterCfg = (appMasterCfg_t *) &datcMasterCfg;
  pAppSecCfg = (appSecCfg_t *) &datcSecCfg;
  pAppDiscCfg = (appDiscCfg_t *) &datcDiscCfg;
  pAppCfg = (appCfg_t *)&datcAppCfg;
  pSmpCfg = (smpCfg_t *) &datcSmpCfg;

  /* Initialize application framework */
  AppMasterInit();
  AppDiscInit();

  /* Set IRK for the local device */
  DmSecSetLocalIrk(localIrk);
}

/*************************************************************************************************/
/*!
 *  \fn     DatcHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatcHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  if (pMsg != NULL)
  {
#if WDXC_INCLUDED == TRUE
    if (datcCb.streaming[pMsg->param -1] != TRUE)
#endif /*WDXC_INCLUDED  */
    {
      // APP_TRACE_INFO1("Datc got evt %d", pMsg->event);
    }

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
    datcProcMsg((dmEvt_t *) pMsg);

#if WDXC_INCLUDED == TRUE
    /* perform wdxc operations */
    WdxcProcMsg((wsfMsgHdr_t *) pMsg);
#endif /*WDXC_INCLUDED  */
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcInitSvcHdlList
 *
 *  \brief  Initialize the pointers into the handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcInitSvcHdlList()
{
  uint8_t i;

  for ( i = 0; i < DM_CONN_MAX; i++ )
  {
    /*! Pointers into handle list for each service's handles */
    pDatcGattHdlList[i] = &datcCb.hdlList[i][DATC_DISC_GATT_START];
    pDatcGapHdlList[i] = &datcCb.hdlList[i][DATC_DISC_GAP_START];
    pDatcWpHdlList[i] = &datcCb.hdlList[i][DATC_DISC_WP_START];
#if WDXC_INCLUDED == TRUE
    pDatcWdxHdlList[i] = &datcCb.hdlList[i][DATC_DISC_WDXC_START];
#endif /* WDXC_INCLUDED */
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
void DatcStart(void)
{
  /* Initialize handle pointers */
  datcInitSvcHdlList();

  /* Register for stack callbacks */
  DmRegister(datcDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, datcDmCback);
  AttRegister(datcAttCback);

  /* Register for app framework button callbacks */
  AppUiBtnRegister(datcBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(datcDiscCback);

  /* Initialize attribute server database */
  SvcCoreAddGroup();

#if WDXC_INCLUDED == TRUE
  /* Initialize the WDXC and set the WDXC application callbacks */
  WdxcInit(datcWdxcFtdCallback, datcWdxcFtcCallback);
#endif /* WDXC_INCLUDED */

  /* Reset the device */
  DmDevReset();
}
