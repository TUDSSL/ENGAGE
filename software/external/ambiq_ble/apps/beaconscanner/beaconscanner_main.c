// ****************************************************************************
//
//  beaconscanner_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of Beacon Scanner.
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
#include "att_api.h"
#include "smp_api.h"
#include "app_cfg.h"
#include "app_api.h"
#include "app_main.h"
#include "app_db.h"
#include "app_ui.h"
#include "svc_core.h"
#include "svc_px.h"
#include "svc_ch.h"
#include "gatt_api.h"
#include "gap_api.h"
#include "fmpl_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! WSF message event starting value */
#define BEACONSCANNER_MSG_START               0xA0

/*! WSF message event enumeration */
enum
{
  BEACONSCANNER_SCAN_TIMER_IND = BEACONSCANNER_MSG_START,     /*! Scan timer expired */
};


/*! Base UUID:  B10D0000-0000-0000-BD59-D38B413F017D */
#define IBEACON_TENDEGREE_UUID128       0xB1, 0x0D, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00, \
                                        0xBD, 0x59, 0xD3, 0x8B, \
                                        0x41, 0x3F, 0x01, 0x7D

const uint8_t TenDegree_ibeacon_uuid128 [] = {IBEACON_TENDEGREE_UUID128};
// offset of 16 BYTE UUID within manufacturer data
#define IBEACON_UUID128_OFFSET            6

// offset of 2 BYTE Major within manufacturer data
#define IBEACON_UUID128_MAJOR             (IBEACON_UUID128_OFFSET + 16)
// offset of 2 BYTE Minor within manufacturer data
#define IBEACON_UUID128_MINOR             (IBEACON_UUID128_MAJOR + 2)

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! proximity reporter control block */
static struct
{
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];
  wsfHandlerId_t    handlerId;
  uint8_t           discState;
  bdAddr_t          peerAddr;                     /* Peer address */
  uint8_t           addrType;                     /* Peer address type */
  wsfTimer_t        scanTimer;                    /* timer for scan after connection is up */
  appDbHdl_t        dbHdl;                        /*! Device database record handle type */
} beaconScannerCb;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t beaconScannerAdvCfg =
{
  {5000, 0,  0},                  /*! Advertising durations in ms */
  {   56, 0,  0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t beaconScannerSlaveCfg =
{
  1,                                      /*! Maximum connections */
};

/*! configurable parameters for security */
static const appSecCfg_t beaconScannerSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  DM_KEY_DIST_IRK,                        /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK | DM_KEY_DIST_IRK,      /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t beaconScannerUpdateCfg =
{
  6000,                                   /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
  640,                                    /*! Minimum connection interval in 1.25ms units */
  800,                                    /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t beaconScannerDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

/*! SMP security parameter configuration */
static const smpCfg_t beaconScannerSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,                    /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0                                       /*! Device authentication requirements */
};

static const appCfg_t beaconScannerAppCfg =
{
  TRUE,                                   /*! TRUE to abort service discovery if service not found */
  TRUE                                    /*! TRUE to disconnect if ATT transaction times out */
};

/*! local IRK */
static uint8_t localIrk[] =
{
  0x95, 0xC8, 0xEE, 0x6F, 0xC5, 0x0D, 0xEF, 0x93, 0x35, 0x4E, 0x7C, 0x57, 0x08, 0xE2, 0xA3, 0x85
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t beaconScannerAdvDataDisc[] =
{
  /*! flags */
  2,                                      /*! length */
  DM_ADV_TYPE_FLAGS,                      /*! AD type */
  DM_FLAG_LE_LIMITED_DISC |               /*! flags */
  DM_FLAG_LE_BREDR_NOT_SUP,

  /*! tx power */
  2,                                      /*! length */
  DM_ADV_TYPE_TX_POWER,                   /*! AD type */
  0,                                      /*! tx power */

  /*! device name */
  14,                                      /*! length */
  DM_ADV_TYPE_LOCAL_NAME,                 /*! AD type */
  'B',
  'e',
  'a',
  'c',
  'o',
  'n',
  'S',
  'c',
  'a',
  'n',
  'n',
  'e',
  'r',
};

/*! scan data */
static const uint8_t beaconScannerScanData[] =
{
  /*! service UUID list */
  7,                                      /*! length */
  DM_ADV_TYPE_16_UUID,                    /*! AD type */
  UINT16_TO_BYTES(ATT_UUID_LINK_LOSS_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_IMMEDIATE_ALERT_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_TX_POWER_SERVICE)
};


/*! configurable parameters for master */
static const appMasterCfg_t beaconScannerMasterCfg =
{
  96,                                      /*! The scan interval, in 0.625 ms units */
  48,                                      /*! The scan window, in 0.625 ms units  */
  200,                                    /*! The scan duration in ms */
  DM_DISC_MODE_NONE,                       /*! The GAP discovery mode */
  DM_SCAN_TYPE_PASSIVE                      /*! The scan type (active or passive) */
};


/*! Scan interval in seconds */
#define BEACONSCANNER_SCAN_INTERVAL      1
/*! Scan delay in seconds afer connection is being setup. */
#define BEACONSCANNER_SCAN_DELAY         3


/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  BEACONSCANNER_DISC_IAS_SVC,       /* Immediate Alert service */
  BEACONSCANNER_DISC_GATT_SVC,      /* GATT service */
  BEACONSCANNER_DISC_GAP_SVC,       /* GAP service */
  BEACONSCANNER_DISC_SVC_MAX        /* Discovery complete */
};

/*! the Client handle list, beaconScannerCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- BEACONSCANNER_DISC_IAS_START
 *  | IAS alert level handle      |
 *  ------------------------------- <- BEACONSCANNER_DISC_GATT_START
 *  | GATT svc changed handle     |
 *  -------------------------------
 *  | GATT svc changed ccc handle |
 *  ------------------------------- <- BEACONSCANNER_DISC_GAP_START
 *  | GAP central addr res handle |
 *  -------------------------------
 *  | GAP RPA Only handle         |
 *  -------------------------------
 */

/*! Start of each service's handles in the the handle list */
#define BEACONSCANNER_DISC_IAS_START        0
#define BEACONSCANNER_DISC_GATT_START       (BEACONSCANNER_DISC_IAS_START + FMPL_IAS_HDL_LIST_LEN)
#define BEACONSCANNER_DISC_GAP_START        (BEACONSCANNER_DISC_GATT_START + GATT_HDL_LIST_LEN)
#define BEACONSCANNER_DISC_HDL_LIST_LEN     (BEACONSCANNER_DISC_GAP_START + GAP_HDL_LIST_LEN)

/*! Pointers into handle list for each service's handles */
static uint16_t *pBeaconScanIasHdlList  = &beaconScannerCb.hdlList[BEACONSCANNER_DISC_IAS_START];
static uint16_t *pBeaconScanGattHdlList = &beaconScannerCb.hdlList[BEACONSCANNER_DISC_GATT_START];
static uint16_t *pBeaconScanGapHdlList  = &beaconScannerCb.hdlList[BEACONSCANNER_DISC_GAP_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(BEACONSCANNER_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Data
**************************************************************************************************/

/* Default value for GATT service changed ccc descriptor */
static const uint8_t beaconScannerGattScCccVal[] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* List of characteristics to configure */
static const attcDiscCfg_t beaconScannerDiscCfgList[] =
{
  /* Write:  GATT service changed ccc descriptor */
  {beaconScannerGattScCccVal, sizeof(beaconScannerGattScCccVal), (GATT_SC_CCC_HDL_IDX + BEACONSCANNER_DISC_GATT_START)},

  /* Read: GAP central address resolution attribute */
  {NULL, 0, (GAP_CAR_HDL_IDX + BEACONSCANNER_DISC_GAP_START)},
};

/* Characteristic configuration list length */
#define BEACONSCANNER_DISC_CFG_LIST_LEN   (sizeof(beaconScannerDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(BEACONSCANNER_DISC_CFG_LIST_LEN <= BEACONSCANNER_DISC_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Server Data
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors used in local ATT server */
enum
{
  BEACONSCANNER_GATT_SC_CCC_IDX,        /*! GATT service, service changed characteristic */
  BEACONSCANNER_NUM_CCC_IDX             /*! Number of ccc's */
};

/*! client characteristic configuration descriptors settings, indexed by ccc enumeration */
static const attsCccSet_t beaconScannerCccSet[BEACONSCANNER_NUM_CCC_IDX] =
{
  /* cccd handle         value range                 security level */
  {GATT_SC_CH_CCC_HDL,   ATT_CLIENT_CFG_INDICATE,    DM_SEC_LEVEL_ENC}    /* BEACONSCANNER_GATT_SC_CCC_IDX */
};

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerAlert
 *
 *  \brief  Perform an alert based on the alert characteristic value
 *
 *  \param  alert  alert characteristic value
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerAlert(uint8_t alert)
{
  /* perform alert according to setting of alert alert */
  if (alert == CH_ALERT_LVL_NONE)
  {
    AppUiAction(APP_UI_ALERT_CANCEL);
  }
  else if (alert == CH_ALERT_LVL_MILD)
  {
    AppUiAction(APP_UI_ALERT_LOW);
  }
  else if (alert == CH_ALERT_LVL_HIGH)
  {
    AppUiAction(APP_UI_ALERT_HIGH);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  uint16_t  len;

  if (pDmEvt->hdr.event == DM_SEC_ECC_KEY_IND)
  {
    DmSecSetEccKey(&pDmEvt->eccMsg.data.key);
  }
  else
  {
    len = DmSizeOfEvt(pDmEvt);

    if ((pMsg = WsfMsgAlloc(len)) != NULL)
    {
      memcpy(pMsg, pDmEvt, len);
      WsfMsgSend(beaconScannerCb.handlerId, pMsg);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerAttCback
 *
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(beaconScannerCb.handlerId, pMsg);
  }

  return;
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerCccCback
 *
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerCccCback(attsCccEvt_t *pEvt)
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
    WsfMsgSend(beaconScannerCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerIasWriteCback
 *
 *  \brief  ATTS write callback for Immediate Alert service.
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
static uint8_t beaconScannerIasWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                                          uint16_t offset, uint16_t len, uint8_t *pValue,
                                          attsAttr_t *pAttr)
{
  ATT_TRACE_INFO3("beaconScannerIasWriteCback connId:%d handle:0x%04x op:0x%02x",
                  connId, handle, operation);
  ATT_TRACE_INFO2("                 offset:0x%04x len:0x%04x", offset, len);

  beaconScannerAlert(*pValue);

  return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
*  \fn     beaconScannerOpen
*
*  \brief  Perform actions on connection open.
*
*  \param  pMsg    Pointer to DM callback event message.
*
*  \return None.
*/
/*************************************************************************************************/
static void beaconScannerOpen(dmEvt_t *pMsg)
{
  /* Update peer address info */
  beaconScannerCb.addrType = pMsg->connOpen.addrType;
  BdaCpy(beaconScannerCb.peerAddr, pMsg->connOpen.peerAddr);
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerClose
 *
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerClose(dmEvt_t *pMsg)
{
  uint8_t   *pVal;
  uint16_t  len;

  /* perform alert according to setting of link loss alert */
  if (AttsGetAttr(LLS_AL_HDL, &len, &pVal) == ATT_SUCCESS)
  {
    if (*pVal == CH_ALERT_LVL_MILD || *pVal == CH_ALERT_LVL_HIGH)
    {
      beaconScannerAlert(*pVal);
    }
  }
}

/*************************************************************************************************/
/*!
*  \fn     beaconScannerSecPairCmpl
*
*  \brief  Handle pairing complete.
*
*  \param  pMsg    Pointer to DM callback event message.
*
*  \return None.
*/
/*************************************************************************************************/
static void beaconScannerSecPairCmpl(dmEvt_t *pMsg)
{
  appConnCb_t *pCb;
  dmSecKey_t *pPeerKey;

  /* if LL Privacy has been enabled */
  if (DmLlPrivEnabled())
  {
    /* look up app connection control block from DM connection ID */
    pCb = &appConnCb[pMsg->hdr.param - 1];

    /* if database record handle valid */
    if ((pCb->dbHdl != APP_DB_HDL_NONE) && ((pPeerKey = AppDbGetKey(pCb->dbHdl, DM_KEY_IRK, NULL)) != NULL))
    {
      /* store peer identity info */
      BdaCpy(beaconScannerCb.peerAddr, pPeerKey->irk.bdAddr);
      beaconScannerCb.addrType = pPeerKey->irk.addrType;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerSetup
 *
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerSetup(dmEvt_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(beaconScannerAdvDataDisc), (uint8_t *) beaconScannerAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(beaconScannerScanData), (uint8_t *) beaconScannerScanData);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, sizeof(beaconScannerAdvDataDisc), (uint8_t *) beaconScannerAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, sizeof(beaconScannerScanData), (uint8_t *) beaconScannerScanData);

  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerValueUpdate
 *
 *  \brief  Process a received ATT indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to */

    /* GATT */
    if (GattValueUpdate(pBeaconScanGattHdlList, pMsg) == ATT_SUCCESS)
    {
      return;
    }

    /* GAP */
    if (GapValueUpdate(pBeaconScanGapHdlList, pMsg) == ATT_SUCCESS)
    {
      return;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerDiscGapCmpl
 *
 *  \brief  GAP service discovery has completed.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerDiscGapCmpl(dmConnId_t connId)
{
  appDbHdl_t dbHdl;

  /* if RPA Only attribute found on peer device */
  if ((pBeaconScanGapHdlList[GAP_RPAO_HDL_IDX] != ATT_HANDLE_NONE) &&
      ((dbHdl = AppDbGetHdl(connId)) != APP_DB_HDL_NONE))
  {
    /* update DB */
    AppDbSetPeerRpao(dbHdl, TRUE);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerBtnCback(uint8_t btn)
{
  dmConnId_t  connId;

  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* send immediate alert, high */
        FmplSendAlert(connId, pBeaconScanIasHdlList[FMPL_IAS_AL_HDL_IDX], CH_ALERT_LVL_HIGH);
        break;

      case APP_UI_BTN_1_MED:
        /* send immediate alert, none */
        FmplSendAlert(connId, pBeaconScanIasHdlList[FMPL_IAS_AL_HDL_IDX], CH_ALERT_LVL_NONE);
        break;

      case APP_UI_BTN_1_LONG:
        /* disconnect */
        AppConnClose(connId);
        break;

      case APP_UI_BTN_2_SHORT:

        break;

      case APP_UI_BTN_2_MED:
        {
          uint8_t addrType = DmConnPeerAddrType(connId);

          /* if peer is using a public address */
          if (addrType == DM_ADDR_PUBLIC)
          {
            /* add peer to the white list */
            DmDevWhiteListAdd(addrType, DmConnPeerAddr(connId));

            /* set Advertising filter policy to All */
            DmDevSetFilterPolicy(DM_FILT_POLICY_MODE_ADV, HCI_ADV_FILT_ALL);
          }
        }
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
        /* clear bonded device info */
        AppDbDeleteAllRecords();

        /* if LL Privacy is supported */
        if (HciLlPrivacySupported())
        {
          /* if LL Privacy has been enabled */
          if (DmLlPrivEnabled())
          {
            /* make sure LL Privacy is disabled before restarting advertising */
            DmPrivSetAddrResEnable(FALSE);
          }

          /* clear resolving list */
          DmPrivClearResList();
        }

        /* restart advertising */
        AppAdvStart(APP_MODE_AUTO_INIT);
        break;

      case APP_UI_BTN_1_EX_LONG:
        /* add RPAO characteristic to GAP service -- needed only when DM Privacy enabled */
        SvcCoreGapAddRpaoCh();
        break;

      case APP_UI_BTN_2_MED:
        /* clear the white list */
        DmDevWhiteListClear();

        /* set Advertising filter policy to None */
        DmDevSetFilterPolicy(DM_FILT_POLICY_MODE_ADV, HCI_ADV_FILT_NONE);
        break;

      case APP_UI_BTN_2_SHORT:
        /* stop advertising */
        AppAdvStop();
        break;

      case APP_UI_BTN_2_LONG:
        /* start directed advertising using peer address */
        AppConnAccept(DM_ADV_CONN_DIRECT_LO_DUTY, beaconScannerCb.addrType, beaconScannerCb.peerAddr, beaconScannerCb.dbHdl);
        break;

      case APP_UI_BTN_2_EX_LONG:
        /* enable device privacy -- start generating local RPAs every 15 minutes */
        DmDevPrivStart(15 * 60);
        break;

      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerDiscCback
 *
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, BEACONSCANNER_DISC_HDL_LIST_LEN, beaconScannerCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* request security */
      AppSlaveSecurityReq(connId);
      break;

    case APP_DISC_START:
      /* initialize discovery state */
      beaconScannerCb.discState = BEACONSCANNER_DISC_IAS_SVC;

      /* discover immediate alert service */
      FmplIasDiscover(connId, pBeaconScanIasHdlList);
      break;

    case APP_DISC_FAILED:
      if (pAppCfg->abortDisc)
      {
        /* if immediate alert service not found */
        if (beaconScannerCb.discState == BEACONSCANNER_DISC_IAS_SVC)
        {
          /* discovery failed */
          AppDiscComplete(connId, APP_DISC_FAILED);
          break;
        }
      }
      /* else fall through to continue discovery */

    case APP_DISC_CMPL:
      /* next discovery state */
      beaconScannerCb.discState++;

      if (beaconScannerCb.discState == BEACONSCANNER_DISC_GATT_SVC)
      {
        /* discover GATT service */
        GattDiscover(connId, pBeaconScanGattHdlList);
      }
      else if (beaconScannerCb.discState == BEACONSCANNER_DISC_GAP_SVC)
      {
        /* discover GAP service */
        GapDiscover(connId, pBeaconScanGapHdlList);
      }
      else
      {
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* GAP service discovery completed */
        beaconScannerDiscGapCmpl(connId);

        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, BEACONSCANNER_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) beaconScannerDiscCfgList, BEACONSCANNER_DISC_HDL_LIST_LEN, beaconScannerCb.hdlList);
      }
      break;

    case APP_DISC_CFG_START:
      /* start configuration */
      AppDiscConfigure(connId, APP_DISC_CFG_START, BEACONSCANNER_DISC_CFG_LIST_LEN,
                       (attcDiscCfg_t *) beaconScannerDiscCfgList, BEACONSCANNER_DISC_HDL_LIST_LEN, beaconScannerCb.hdlList);
      break;

    case APP_DISC_CFG_CMPL:
      AppDiscComplete(connId, APP_DISC_CFG_CMPL);
      break;

    case APP_DISC_CFG_CONN_START:
      /* no connection setup configuration for this application */
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     beaconScannerProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void beaconScannerProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;

  switch(pMsg->hdr.event)
  {
    case ATTC_READ_RSP:
    case ATTC_HANDLE_VALUE_IND:
      beaconScannerValueUpdate((attEvt_t *) pMsg);
      break;

    case ATT_MTU_UPDATE_IND:
      APP_TRACE_INFO1("Negotiated MTU %d", ((attEvt_t *)pMsg)->mtu);
      break;  

    case DM_RESET_CMPL_IND:
      AttsCalculateDbHash();
      DmSecGenerateEccKeyReq();
      beaconScannerSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_ADV_START_IND:
      uiEvent = APP_UI_ADV_START;
      break;

    case DM_ADV_STOP_IND:
      uiEvent = APP_UI_ADV_STOP;
      // almost immediately start SCAN.
      WsfTimerStartMs(&beaconScannerCb.scanTimer, 10);

      break;

    case DM_SCAN_START_IND:
      break;

    case DM_SCAN_STOP_IND:
      {
        // advertising again if there's no connection.
        if (AppConnIsOpen() == DM_CONN_ID_NONE)
        {
          AppAdvStart(APP_MODE_AUTO_INIT);
        }
        else
        {
          // during connection, we'll scan every BEACONSCANNER_SCAN_INTERVAL seconds.
          WsfTimerStartSec(&beaconScannerCb.scanTimer, BEACONSCANNER_SCAN_INTERVAL);
        }
      }

      break;

    case DM_SCAN_REPORT_IND:
      {
          uint8_t *pData;
          pData = DmFindAdType(DM_ADV_TYPE_MANUFACTURER, pMsg->scanReport.len,
                           pMsg->scanReport.pData);

          if ( pData )
          {

            if ( !memcmp(pData + IBEACON_UUID128_OFFSET, TenDegree_ibeacon_uuid128, 16) )
            {

              uint16_t major, minor;

              APP_TRACE_INFO1("eventType 0x%x", pMsg->scanReport.eventType);
              APP_TRACE_INFO1("addrType 0x%x", pMsg->scanReport.addrType);
              APP_TRACE_INFO3("addr[0-2] 0x%02x %02x %02x",
                  pMsg->scanReport.addr[0],
                  pMsg->scanReport.addr[1],
                  pMsg->scanReport.addr[2]
                  );
              APP_TRACE_INFO3("addr[3-5] 0x%02x %02x %02x",
                  pMsg->scanReport.addr[3],
                  pMsg->scanReport.addr[4],
                  pMsg->scanReport.addr[5]
                  );
              APP_TRACE_INFO1("rssi %d", pMsg->scanReport.rssi);

              major = pData[IBEACON_UUID128_MAJOR] << 8;
              major += pData[IBEACON_UUID128_MAJOR + 1];

              APP_TRACE_INFO1("Major %d", major);

              minor = pData[IBEACON_UUID128_MINOR] << 8;
              minor += pData[IBEACON_UUID128_MINOR + 1];

              APP_TRACE_INFO1("Minor %d", minor);

            }
          }

      }
      break;
    case DM_CONN_OPEN_IND:
      beaconScannerOpen(pMsg);
      uiEvent = APP_UI_CONN_OPEN;

      // here we purposely delay scan until the interaction at higher layer
      // between master and slave is done.

      WsfTimerStartSec(&beaconScannerCb.scanTimer, BEACONSCANNER_SCAN_DELAY);

      break;

    case DM_CONN_CLOSE_IND:
      beaconScannerClose(pMsg);
      uiEvent = APP_UI_CONN_CLOSE;
      AppScanStop();
      break;

    case DM_SEC_PAIR_CMPL_IND:
      beaconScannerSecPairCmpl(pMsg);
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
      AppHandlePasskey(&pMsg->authReq);
      break;

    case DM_SEC_COMPARE_IND:
      AppHandleNumericComparison(&pMsg->cnfInd);
      break;

    case DM_ADV_NEW_ADDR_IND:
      break;

    case BEACONSCANNER_SCAN_TIMER_IND:
      AppScanStart(beaconScannerMasterCfg.discMode, beaconScannerMasterCfg.scanType,
          beaconScannerMasterCfg.scanDuration);

      break;
    case DM_CONN_READ_RSSI_IND:
      /* if successful */
      if (pMsg->hdr.status == HCI_SUCCESS)
      {
        /* display RSSI value */
        AppUiDisplayRssi(pMsg->readRssi.rssi);
      }
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
 *  \fn     BeaconScannerHandlerInit
 *
 *  \brief  Proximity reporter handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BeaconScannerHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("BeaconScannerHandlerInit");

  /* store handler ID */
  beaconScannerCb.handlerId = handlerId;

  /* initialize control block */
  beaconScannerCb.scanTimer.handlerId = handlerId;
  beaconScannerCb.scanTimer.msg.event = BEACONSCANNER_SCAN_TIMER_IND;

  /* Set configuration pointers */
  pAppMasterCfg = (appMasterCfg_t *) &beaconScannerMasterCfg;
  pAppSlaveCfg = (appSlaveCfg_t *) &beaconScannerSlaveCfg;
  pAppAdvCfg = (appAdvCfg_t *) &beaconScannerAdvCfg;
  pAppSecCfg = (appSecCfg_t *) &beaconScannerSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &beaconScannerUpdateCfg;
  pAppDiscCfg = (appDiscCfg_t *) &beaconScannerDiscCfg;
  pAppCfg = (appCfg_t *) &beaconScannerAppCfg;

  /* Set stack configuration pointers */
  pSmpCfg = (smpCfg_t *)&beaconScannerSmpCfg;

  /* Initialize application framework */
  AppMasterInit();
  AppSlaveInit();
  AppDiscInit();

  /* Set IRK for the local device */
  DmSecSetLocalIrk(localIrk);
}

/*************************************************************************************************/
/*!
 *  \fn     BeaconScannerHandler
 *
 *  \brief  WSF event handler for proximity reporter.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BeaconScannerHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("BeaconScanner got evt %d", pMsg->event);

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
      AppSlaveProcDmMsg((dmEvt_t *) pMsg);

      /* process security-related messages */
      AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);

      /* process discovery-related messages */
      AppDiscProcDmMsg((dmEvt_t *) pMsg);
    }

    /* perform profile and user interface-related operations */
    beaconScannerProcMsg((dmEvt_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     BeaconScannerStart
 *
 *  \brief  Start the proximity profile reporter application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BeaconScannerStart(void)
{
  /* Register for stack callbacks */
  DmRegister(beaconScannerDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, beaconScannerDmCback);
  AttRegister(beaconScannerAttCback);
  AttConnRegister(AppServerConnCback);
  AttsCccRegister(BEACONSCANNER_NUM_CCC_IDX, (attsCccSet_t *) beaconScannerCccSet, beaconScannerCccCback);

  /* Register for app framework button callbacks */
  AppUiBtnRegister(beaconScannerBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(beaconScannerDiscCback);

  /* Initialize attribute server database */
  SvcCoreAddGroup();
  SvcPxCbackRegister(NULL, beaconScannerIasWriteCback);
  SvcPxAddGroup();

  /* Reset the device */
  DmDevReset();
}
