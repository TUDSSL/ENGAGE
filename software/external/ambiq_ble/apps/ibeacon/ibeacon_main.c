// ****************************************************************************
//
//  ibeacon_main.c
//! @file
//!
//! @brief Ambiq Micro's demonstration of iBeacon.
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
#include "ibeacon_api.h"
#include "gatt_api.h"
/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! WSF message event starting value */
#define IBEACON_MSG_START               0xA0

/*! WSF message event enumeration */
enum
{
    IBEACON_TIMER_IND = IBEACON_MSG_START,  /*! iBeacon reset timer expired */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Application message type */
typedef union
{
    wsfMsgHdr_t     hdr;
    dmEvt_t         dm;
} ibeaconMsg_t;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t ibeaconAdvCfg =
{
    {60000,     0,     0},                  /*! Advertising durations in ms */
    {  800,   800,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t ibeaconSlaveCfg =
{
    1,                           /*! Maximum connections */
};

/*! configurable parameters for security */
static const appSecCfg_t ibeaconSecCfg =
{
    DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
    0,                                      /*! Initiator key distribution flags */
    DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
    FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
    FALSE                                   /*! TRUE to initiate security upon connection */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t ibeaconAdvDataDisc[] =
{
    /*! flags */
    2,                                      /*! length */
    DM_ADV_TYPE_FLAGS,                      /*! AD type */
    DM_FLAG_LE_GENERAL_DISC |               /*! flags */
    DM_FLAG_LE_BREDR_NOT_SUP,

    /*! Manufacturer specific data AD type */
    0x1A,                                   /*! length */
    DM_ADV_TYPE_MANUFACTURER,               /*! AD type */

    0x4C,                                   /*! Company ID[0] */
    0x00,                                   /*! Company ID[1] */

    0x02,                                   /*! Device type, this has to be 0x02*/
    0x15,                                   /*! Length of vendor advertising data. */

    /*! Proximity UUID 16 bytes */
    0xe2, 0xc5, 0x6d, 0xb5,
    0xdf, 0xfb, 0x48, 0xd2,
    0xb0, 0x60, 0xd0, 0xf5,
    0xa7, 0x10, 0x96, 0xe0,

    /*! Major 0xnnnn */
    0x00, 0x00,
    /*! Minor 0xnnnn */
    0x00, 0x00,

    /*! Measured Power */
    0xc5
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! WSF handler ID */
wsfHandlerId_t iBeaconHandlerId;

/*************************************************************************************************/
/*!
 *  \fn     iBeaconDmCback
 *
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void iBeaconDmCback(dmEvt_t *pDmEvt)
{
    dmEvt_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
    {
        memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
        WsfMsgSend(iBeaconHandlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     iBeacon Setup
 *
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void iBeaconSetup(ibeaconMsg_t *pMsg)
{

    /* set advertising and scan response data for discoverable mode */
    AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(ibeaconAdvDataDisc), (uint8_t *) ibeaconAdvDataDisc);

    /* set advertising and scan response data for connectable mode */
    AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);

    // non-connectable advertising
    AppSetAdvType(DM_ADV_NONCONN_UNDIRECT);

    /* start advertising; automatically set connectable/discoverable mode and bondable mode */
    AppAdvStart(APP_MODE_DISCOVERABLE);
}

/*************************************************************************************************/
/*!
 *  \fn     iBeaconProcMsg
 *
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void iBeaconProcMsg(ibeaconMsg_t *pMsg)
{
    uint8_t uiEvent = APP_UI_NONE;

    switch(pMsg->hdr.event)
    {
        case DM_RESET_CMPL_IND:
            AttsCalculateDbHash();
            DmSecGenerateEccKeyReq();
            iBeaconSetup(pMsg);
            uiEvent = APP_UI_RESET_CMPL;
        break;

        case DM_ADV_START_IND:
            uiEvent = APP_UI_ADV_START;
        break;

        case DM_ADV_STOP_IND:
            uiEvent = APP_UI_ADV_STOP;
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
 *  \fn     iBeaconHandlerInit
 *
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void iBeaconHandlerInit(wsfHandlerId_t handlerId)
{
    APP_TRACE_INFO0("iBeaconHandlerInit");

    /* store handler ID */
    iBeaconHandlerId = handlerId;

    /* Set configuration pointers */
    pAppAdvCfg = (appAdvCfg_t *) &ibeaconAdvCfg;
    pAppSlaveCfg = (appSlaveCfg_t *) &ibeaconSlaveCfg;
    pAppSecCfg = (appSecCfg_t *) &ibeaconSecCfg;

    /* Initialize application framework */
    AppSlaveInit();
}

/*************************************************************************************************/
/*!
 *  \fn     iBeaconHandler
 *
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void iBeaconHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
    if (pMsg != NULL)
    {
        APP_TRACE_INFO1("iBeacon got evt %d", pMsg->event);

        if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END)
        {
            /* process advertising and connection-related messages */
            AppSlaveProcDmMsg((dmEvt_t *) pMsg);

            /* process security-related messages */
            AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);
        }

        /* perform profile and user interface-related operations */
        iBeaconProcMsg((ibeaconMsg_t *) pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     iBeaconStart
 *
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void iBeaconStart(void)
{
    /* Register for stack callbacks */
    DmRegister(iBeaconDmCback);
    DmConnRegister(DM_CLIENT_ID_APP, iBeaconDmCback);

    /* Reset the device */
    DmDevReset();
}
