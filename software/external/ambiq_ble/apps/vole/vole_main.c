// ****************************************************************************
//
//  vole_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of Voice Over LE service.
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

#include "usr_include.h"

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

#include "vole_api.h"
#include "svc_amvole.h"
#include "vole_common.h"
#include "vole_board_config.h"

#include "am_util.h"
#include "crc32.h"
#include "am_app_KWD_AMA.h"
#include "FreeRTOS.h"
#include "task.h"
#include "voles_api.h"
#include "gatt_api.h"


/**************************************************************************************************
  Macros
**************************************************************************************************/

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
} amvoleMsg_t;


/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t amvoleAdvCfg =
{
  {60000,     0,     0},                  /*! Advertising durations in ms */
  {  800,   800,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t amvoleSlaveCfg =
{
  VOLE_CONN_MAX,                           /*! Maximum connections */
};

/*! configurable parameters for security */
static const appSecCfg_t amvoleSecCfg =
{
  DM_AUTH_BOND_FLAG | DM_AUTH_SC_FLAG,    /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};


static const appUpdateCfg_t amvoleUpdateCfg =
{
  1000, //3000,                           /*! Connection idle period in ms before attempting
                                          //connection parameter update; set to zero to disable */
  6,    //6,                              /*! 7.5ms */
  12,   //15,                             /*! 30ms */
  0,                                      /*! Connection latency */
  400, //2000, //600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};

/*! SMP security parameter configuration */
static const smpCfg_t amvoleSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_NO_IN_NO_OUT,                    /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0,                                      /*! Device authentication requirements */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

// RAM buffers to be used
uint8_t amvoleAdvDataDisc[31];
uint8_t amvoleScanDataDisc[31];

/*! advertising data, discoverable mode */
static const uint8_t amvoleAdvDataDiscDefault[] =
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
  17,                                      /*! length */
  DM_ADV_TYPE_128_UUID,                    /*! AD type */
  ATT_UUID_VOLES_SERVICE
};

/*! scan data, discoverable mode */
static const uint8_t amvoleScanDataDiscDefault[] =
{
  /*! device name */
  5,                                      /*! length */
  DM_ADV_TYPE_LOCAL_NAME,                 /*! AD type */
  'V',
  'o',
  'L',
  'E'
};

dmConnId_t g_AmaConnId = DM_CONN_ID_NONE;

bool_t g_start_voice_send = FALSE;

extern bool am_app_KWD_AMA_tx_ver_exchange_send(void);

// index is the starting point of the local name, local name only in advData
static bool amvoleSetLocalName(uint8_t* pAdvData, uint8_t* pLocalName, uint8_t len, uint8_t index)
{
    if (index + len + 1 > 31)
    {
        // max adv data is 31 byte long
        return false;
    }

    // set parameter length
    pAdvData[index] = len + 1;
    // set parameter type
    pAdvData[index + 1] = DM_ADV_TYPE_LOCAL_NAME;

    // set local name
    for ( uint8_t i = 0; i < len; i++ )
    {
        pAdvData[i + 2 + index] = pLocalName[i];
    }

    return true;
}

void amvoleKwdSetDemoName(void)
{
    uint8_t test_bdaddress[6];
    uint8_t ble_device_name[20] = "VOLES-";    //local name = device name
    uint8_t *pBda;
    uint8_t index = 6;

    //fixme: read bd address and print out
    pBda = HciGetBdAddr();
    BdaCpy(test_bdaddress, pBda);

    pBda = (uint8_t*)Bda2Str(test_bdaddress);
    APP_TRACE_INFO0("Local Device BD Address: ");
    APP_TRACE_INFO0(Bda2Str(test_bdaddress));
    APP_TRACE_INFO0("\n");

    // build demo name here
    // 1st letter is board variant
#if USE_MAYA
    ble_device_name[index++] = 'M';
#elif USE_APOLLO2_QT
    ble_device_name[index++] = 'Q';
#else
    ble_device_name[index++] = 'E';
#endif


    // 3rd letter is wake-on-sound variant
#if USE_WAKE_ON_SOUND
    ble_device_name[index++] = 'W';
#else
    ble_device_name[index++] = 'A';   // A for always listening...
#endif


    ble_device_name[index++] = '-';

    ble_device_name[index++] = 'A';
    ble_device_name[index++] = 'M';
    ble_device_name[index++] = 'A';

    // a hyphen...
    ble_device_name[index++] = '-';

    // take the last 4 hex digit
    ble_device_name[index++] = pBda[8];
    ble_device_name[index++] = pBda[9];
    ble_device_name[index++] = pBda[10];
    ble_device_name[index++] = pBda[11];

    // set local name here:
    amvoleSetLocalName(amvoleScanDataDisc, ble_device_name, index, 0);
    //SvcCoreSetDevName(ble_device_name, index);
}

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
  VOLES_GATT_SC_CCC_IDX,                  /*! GATT service, service changed characteristic */
  VOLES_TX_CCC_IDX,                /*! AMDTP service, tx characteristic */
  VOLES_NUM_CCC_IDX
};

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t amvoleCccSet[VOLES_NUM_CCC_IDX] =
{
  /* cccd handle                value range               security level */
  {GATT_SC_CH_CCC_HDL,          ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_NONE},   /* VOLES_GATT_SC_CCC_IDX */
  {VOLES_TX_CH_CCC_HDL,        ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_NONE},   /* VOLES_TX_CCC_IDX */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! WSF handler ID */
wsfHandlerId_t amvoleHandlerId;

/*************************************************************************************************/
/*!
 *  \fn     amvoleDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  uint16_t len;

  len = DmSizeOfEvt(pDmEvt);

  if ((pMsg = WsfMsgAlloc(len)) != NULL)
  {
    memcpy(pMsg, pDmEvt, len);
    WsfMsgSend(amvoleHandlerId, pMsg);
  }
}

bool amvoleTxChannelIsAvailable(void)
{
  return(DM_CONN_ID_NONE != AppConnIsOpen());
}

void VoleBleSend(uint8_t * buf, uint32_t len)
{
    if (amvoleTxChannelIsAvailable())
    {
        // simply tries to send notification
        AttsHandleValueNtf(g_AmaConnId, VOLES_TX_HDL, len, buf);   // connId always group 0 since support only 1 connection.
    }
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleAttCback
 *
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;

  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(amvoleHandlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleCccCback
 *
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleCccCback(attsCccEvt_t *pEvt)
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
    WsfMsgSend(amvoleHandlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleProcCccState
 *
 *  \brief  Process CCC state change.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleProcCccState(amvoleMsg_t *pMsg)
{
  APP_TRACE_INFO3("ccc state ind value:%d handle:%d idx:%d\n", pMsg->ccc.value, pMsg->ccc.handle, pMsg->ccc.idx);

  /* VOLES TX CCC */
  if (pMsg->ccc.idx == VOLES_TX_CCC_IDX)
  {
    if (pMsg->ccc.value == ATT_CLIENT_CFG_NOTIFY)
    {
      // notify enabled
      g_AmaConnId = (dmConnId_t) pMsg->ccc.hdr.param;
      APP_TRACE_INFO1("connId : %d\r\n", pMsg->ccc.hdr.param);
    }
    else
    {
      g_AmaConnId = DM_CONN_ID_NONE;
    }
    return;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleClose
 *
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleClose(amvoleMsg_t *pMsg)
{
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleSetup
 *
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleSetup(amvoleMsg_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(amvoleAdvDataDisc), (uint8_t *) amvoleAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(amvoleScanDataDisc), (uint8_t *) amvoleScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, sizeof(amvoleAdvDataDisc), (uint8_t *) amvoleAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, sizeof(amvoleScanDataDisc), (uint8_t *) amvoleScanDataDisc);

  AppSetBondable(TRUE);
  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}


//*****************************************************************************
//
// LED task to indicate external events, such as heart beat and key word detected.
//
//*****************************************************************************

void am_app_led_on(void)
{
#if defined(AM_PART_APOLLO2)
        am_hal_gpio_out_bit_toggle(LED_D6);
        am_hal_gpio_out_bit_toggle(LED_D7);
        am_hal_gpio_out_bit_toggle(LED_D8);
#endif // #if defined(AM_PART_APOLLO2)

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
#if USE_APOLLO3_BLUE_EVB
        am_hal_gpio_state_write(LED_D5, AM_HAL_GPIO_OUTPUT_TOGGLE);
        am_hal_gpio_state_write(LED_D6, AM_HAL_GPIO_OUTPUT_TOGGLE);
        am_hal_gpio_state_write(LED_D7, AM_HAL_GPIO_OUTPUT_TOGGLE);
        am_hal_gpio_state_write(LED_D8, AM_HAL_GPIO_OUTPUT_TOGGLE);

#endif // #if USE_APOLLO3_BLUE_EVB
#endif //#if defined(AM_PART_APOLLO3)

}

void am_app_led_off(void)
{
#if defined(AM_PART_APOLLO2)
    am_hal_gpio_out_bit_clear(LED_D6);
    am_hal_gpio_out_bit_clear(LED_D7);
    am_hal_gpio_out_bit_clear(LED_D8);
#endif

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
#if USE_APOLLO3_BLUE_EVB
    am_hal_gpio_state_write(LED_D5, AM_HAL_GPIO_OUTPUT_CLEAR);
    am_hal_gpio_state_write(LED_D6, AM_HAL_GPIO_OUTPUT_CLEAR);
    am_hal_gpio_state_write(LED_D7, AM_HAL_GPIO_OUTPUT_CLEAR);
    am_hal_gpio_state_write(LED_D8, AM_HAL_GPIO_OUTPUT_CLEAR);
#endif
#endif
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleBtnCback
 *
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleBtnCback(uint8_t btn)
{
  dmConnId_t      connId = AppConnIsOpen();

  APP_TRACE_INFO2("button %d pressed, connection open:%d", btn, connId);

  /* button actions when connected */
  if (connId != DM_CONN_ID_NONE)
  {
    APP_TRACE_INFO1("btn:%d", btn);
    switch (btn)
    {
      // transmit voice data using mSBC encode
      case APP_UI_BTN_1_SHORT:
        APP_TRACE_INFO0("start speech data send...");
        voles_set_codec_type(MSBC_CODEC_IN_USE);
        voles_init(amvoleHandlerId, MSBC_CODEC_IN_USE);
        am_app_KWD_AMA_start_speech_send();
        break;

      case APP_UI_BTN_2_SHORT:
       voles_set_codec_type(OPUS_CODEC_IN_USE);
        voles_init(amvoleHandlerId, OPUS_CODEC_IN_USE);
       am_app_KWD_AMA_start_speech_send();
        break;

      default:
        break;
    }
  }
}


uint8_t
amvole_write_cback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                   uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    if (handle == VOLES_RX_HDL)
    {
        am_app_KWD_AMA_rx_handler(pValue, len);
    }
    return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \fn     amvoleProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void amvoleProcMsg(amvoleMsg_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;

  static uint8_t retry_cnt = 0;

  switch(pMsg->hdr.event)
  {
    case ATTS_HANDLE_VALUE_CNF:

      voles_proc_msg(&pMsg->hdr);

      break;

    case ATTS_CCC_STATE_IND:
      amvoleProcCccState(pMsg);
      if (pMsg->ccc.handle == VOLES_TX_CH_CCC_HDL)
      {
          am_app_KWD_AMA_tx_ver_exchange_send();
      }
      break;

    case DM_RESET_CMPL_IND:
        AttsCalculateDbHash();
        DmSecGenerateEccKeyReq();
        amvoleSetup(pMsg);

      #if USE_BLE_TX_POWER_SET
        HciVsEM_SetRfPowerLevelEx(TX_POWER_LEVEL_PLUS_6P2_dBm);
      #endif

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
      voles_proc_msg(&pMsg->hdr);
      DmConnSetDataLen(1, 251, 0x848);

      uiEvent = APP_UI_CONN_OPEN;

      retry_cnt = 0;

      break;

    case ATT_MTU_UPDATE_IND:
      if ( AttGetMtu(1) < BLE_MSBC_DATA_BUFFER_SIZE )
      {
        if (retry_cnt < 5)
        {
          retry_cnt++;
          AttcMtuReq(1, 247);
        }
      }

      APP_TRACE_INFO2("ATT_MTU_UPDATE_IND AttGetMtu(), return = %d pMsg->att.mtu = %d\n", AttGetMtu(1), pMsg->att.mtu);
      break;

    case DM_CONN_DATA_LEN_CHANGE_IND:
      am_util_debug_printf("DM_CONN_DATA_LEN_CHANGE_IND: status = %d, max RX len = %d, max TX len = %d \n", pMsg->dm.dataLenChange.hdr.status, pMsg->dm.dataLenChange.maxRxOctets, pMsg->dm.dataLenChange.maxTxOctets);
      break;

    case DM_CONN_CLOSE_IND:
      APP_TRACE_INFO1("DM_CONN_CLOSE_IND reason = 0x%02x\n", pMsg->dm.connClose.reason);

      amvoleClose(pMsg);
      uiEvent = APP_UI_CONN_CLOSE;

//      AppAdvStart(APP_MODE_DISCOVERABLE);

      g_AmaConnId = DM_CONN_ID_NONE;
      //g_eAmaStatus = VOS_AMA_INIT;
      break;

    case DM_CONN_UPDATE_IND:
      voles_proc_msg(&pMsg->hdr);
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

void amvoleStartSendVoiceData()
{
    g_start_voice_send = TRUE;
    am_app_led_on();
    voles_transmit_voice_data();
}
/*************************************************************************************************/
/*!
 *  \fn     VoleHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void VoleHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("VoleHandlerInit");

  /* store handler ID */
  amvoleHandlerId = handlerId;

  /* Set configuration pointers */
  pAppAdvCfg = (appAdvCfg_t *) &amvoleAdvCfg;
  pAppSlaveCfg = (appSlaveCfg_t *) &amvoleSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &amvoleSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &amvoleUpdateCfg;

  /* Initialize application framework */
  AppSlaveInit();
  AppServerInit();

  /* Set stack configuration pointers */
  pSmpCfg = (smpCfg_t *) &amvoleSmpCfg;
  //voles_init(handlerId);
}

/*************************************************************************************************/
/*!
 *  \fn     VoleHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void VoleHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("vole got evt 0x%x", pMsg->event);

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
    amvoleProcMsg((amvoleMsg_t *) pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     VoleStart
 *
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void VoleStart(void)
{
  /* Register for stack callbacks */
  DmRegister(amvoleDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, amvoleDmCback);
  AttRegister(amvoleAttCback);
  AttConnRegister(AppServerConnCback);
  AttsCccRegister(VOLES_NUM_CCC_IDX, (attsCccSet_t *) amvoleCccSet, amvoleCccCback);

  /* Register for app framework callbacks */
  AppUiBtnRegister(amvoleBtnCback);

  // set up adv data
  memcpy(amvoleAdvDataDisc, amvoleAdvDataDiscDefault, sizeof(amvoleAdvDataDiscDefault));
  memcpy(amvoleScanDataDisc, amvoleScanDataDiscDefault, sizeof(amvoleScanDataDiscDefault));

  /* Initialize attribute server database */
  SvcCoreGattCbackRegister(GattReadCback, GattWriteCback);
  SvcCoreAddGroup();
  SvcDisAddGroup();
  SvcVolesCbackRegister(NULL, amvole_write_cback);
  SvcVolesAddGroup();

  /* Set Service Changed CCCD index. */
  GattSetSvcChangedIdx(VOLES_GATT_SC_CCC_IDX);
  /* Reset the device */
  DmDevReset();
}