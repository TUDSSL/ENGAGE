//*****************************************************************************
//
//  ancc_main.c
//! @file
//!
//! @brief  apple notification center service client.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "bstream.h"
#include "app_api.h"
#include "ancc_api.h"

#include "am_util.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Apple Notification Center Service Client (ANCC)
 */

/* UUIDs */
static const uint8_t anccAncsSvcUuid[] = {ATT_UUID_ANCS_SERVICE};       /*! ANCS Service UUID */
static const uint8_t anccNSChUuid[] = {ATT_UUID_NOTIFICATION_SOURCE};    /*! Notification Source UUID */
static const uint8_t anccCPChUuid[] = {ATT_UUID_CTRL_POINT};             /*! control point UUID*/
static const uint8_t anccDSChUuid[] = {ATT_UUID_DATA_SOURCE};            /*! data source UUID*/
/* Characteristics for discovery */

#define DISCOVER_TIMER_DELAY      (3000)  // ms

/*! Proprietary data */
static const attcDiscChar_t anccNSDat =
{
    anccNSChUuid,
    ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

/*! Proprietary data descriptor */  //notification source
static const attcDiscChar_t anccNSdatCcc =
{
    attCliChCfgUuid,
    ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};
// control point
static const attcDiscChar_t anccCtrlPoint =
{
    anccCPChUuid,
    ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

// data source
static const attcDiscChar_t anccDataSrc =
{
    anccDSChUuid,
    ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};
/*! Proprietary data descriptor */  //data source
static const attcDiscChar_t anccDataSrcCcc =
{
    attCliChCfgUuid,
    ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *anccSvcDiscCharList[] =
{
    &anccNSDat,                    /*! Proprietary data */
    &anccNSdatCcc,                 /*! Proprietary data descriptor */
    &anccCtrlPoint,                /*! Control point */
    &anccDataSrc,                  /*! data source */
    &anccDataSrcCcc                /*! data source descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(ANCC_HDL_LIST_LEN == ((sizeof(anccSvcDiscCharList) / sizeof(attcDiscChar_t *))));

/* Global variable */
anccCb_t    anccCb;

/*************************************************************************************************/
/*!
 *  \fn     AnccSvcDiscover
 *
 *  \brief  Perform service and characteristic discovery for ancs service.
 *          Parameter pHdlList must point to an array of length ANCC_HDL_LIST_LEN.
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccSvcDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
    AppDiscFindService(connId, ATT_128_UUID_LEN, (uint8_t *) anccAncsSvcUuid,
                     ANCC_HDL_LIST_LEN, (attcDiscChar_t **) anccSvcDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     AnccInit
 *
 *  \brief  Initialize the control variables of ancs client
 *
 *  \param  handlerId   App handler id for timer operation.
 *  \param  cfg         config variable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccInit(wsfHandlerId_t handlerId, anccCfg_t* cfg, uint8_t disctimer_event)
{
    memset(anccCb.anccList, 0, ANCC_LIST_ELEMENTS * sizeof(ancc_notif_t));
    anccCb.cfg.period = cfg->period;
    anccCb.actionTimer.handlerId = handlerId;
    anccCb.discoverTimer.handlerId = handlerId;
    anccCb.discoverTimer.msg.event = disctimer_event;
}

/*************************************************************************************************/
/*!
 *  \fn     AnccConnOpen
 *
 *  \brief  Update the key parameters for control variables when connection open.
 *
 *  \param  connId      Connection identifier.
 *  \param  hdlList     Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccConnOpen(dmConnId_t connId, uint16_t* hdlList)
{
    anccCb.connId = connId;
    anccCb.hdlList = hdlList;

    WsfTimerStop(&anccCb.discoverTimer);
    WsfTimerStartMs(&anccCb.discoverTimer, DISCOVER_TIMER_DELAY);
}

/*************************************************************************************************/
/*!
 *  \fn     AnccConnClose
 *
 *  \brief  Clear the key parameters for control variables when connection close.
 *
 *  \param  None.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccConnClose(void)
{
    WsfTimerStop(&anccCb.discoverTimer);
    anccCb.connId = DM_CONN_ID_NONE;
}

/*************************************************************************************************/
/*!
 *  \fn     anccNoConnActive
 *
 *  \brief  Return TRUE if no connections with active measurements.
 *
 *  \return TRUE if no connections active.
 */
/*************************************************************************************************/
static bool_t anccNoConnActive(void)
{
    if (anccCb.connId != DM_CONN_ID_NONE)
    {
        return FALSE;
    }
    return TRUE;
}

/*************************************************************************************************/
/*!
 *  \fn     anccActionListPush
 *
 *  \brief  Return TRUE if element is added/updated to the list.
 *
 *  \return TRUE if element is added/updated to the list.
 *          FALSE if list is full.
 */
/*************************************************************************************************/
static bool_t anccActionListPush(ancc_notif_t* pElement)
{
    uint16_t i;
    for ( i = 0; i < ANCC_LIST_ELEMENTS; i++ )
    {
        if ( (anccCb.anccList[i].notification_uid == pElement->notification_uid ) && (anccCb.anccList[i].noti_valid == true) )
        {
            // same notification uid received, update the element
            anccCb.anccList[i].notification_uid = pElement->notification_uid;
            anccCb.anccList[i].event_id = pElement->event_id;
            anccCb.anccList[i].event_flags = pElement->event_flags;
            anccCb.anccList[i].category_id = pElement->category_id;
            anccCb.anccList[i].category_count = pElement->category_count;
            return true;
        }
    }

    for ( i = 0; i < ANCC_LIST_ELEMENTS; i++ )
    {
        if ( anccCb.anccList[i].noti_valid == false )
        {
            // found an empty slot
            anccCb.anccList[i].notification_uid = pElement->notification_uid;
            anccCb.anccList[i].event_id = pElement->event_id;
            anccCb.anccList[i].event_flags = pElement->event_flags;
            anccCb.anccList[i].category_id = pElement->category_id;
            anccCb.anccList[i].category_count = pElement->category_count;
            anccCb.anccList[i].noti_valid = true;
            return true;
        }
    }
    return false;   //no empty slot left
}

/*************************************************************************************************/
/*!
 *  \fn     anccActionListPop
 *
 *  \brief  Return TRUE if element is popped out and removed from the list.
 *
 *  \return TRUE if element is popped out and removed from the list.
 *          FALSE if list is already empty.
 */
/*************************************************************************************************/
static bool_t anccActionListPop(void)
{
    uint16_t i;
    for ( i = 0; i < ANCC_LIST_ELEMENTS; i++ )
    {
        if ( anccCb.anccList[ANCC_LIST_ELEMENTS - i - 1].noti_valid == true )
        {
            // found a valid element in the list
            anccCb.anccList[ANCC_LIST_ELEMENTS - i - 1].noti_valid = false;
            anccCb.active.handle = ANCC_LIST_ELEMENTS - i - 1;
            return true;
        }
    }
    return false;   //no element left in the list
}

/*************************************************************************************************/
/*!
 *  \fn     AnccActionStart
 *
 *  \brief  Start periodic ancc operation.  This function starts a timer to perform
 *          periodic actions.
 *
 *  \param  timerEvt    WSF event designated by the application for the timer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccActionStart(uint8_t timerEvt)
{
    /* if this is first connection */
    if (anccNoConnActive() == FALSE)
    {
        /* initialize control block */
        anccCb.actionTimer.msg.event = timerEvt;

        /* (re-)start timer */
        WsfTimerStop(&anccCb.actionTimer);
        WsfTimerStartMs(&anccCb.actionTimer, anccCb.cfg.period);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AnccActionStop
 *
 *  \brief  Stop periodic ancc action.
 *
 *  \param  connId      DM connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccActionStop(void)
{
    /* stop timer */
    WsfTimerStop(&anccCb.actionTimer);
}

/*************************************************************************************************/
/*!
 *  \fn     AnccGetNotificationAttribute
 *
 *  \brief  Send a command to the apple notification center service control point.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  notiUid   NotificationUid.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccGetNotificationAttribute(uint16_t *pHdlList, uint32_t notiUid)
{
    // An example to get notification attributes
    uint16_t max_len = 256;
    uint8_t buf[19];   // retrieve the complete attribute list
    if (pHdlList[ANCC_CONTROL_POINT_HDL_IDX] != ATT_HANDLE_NONE)
    {
        buf[0] = BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES;  // put command
        uint8_t * p = &buf[1];
        UINT32_TO_BSTREAM(p, notiUid);    // encode notification uid

        // encode attribute IDs
        buf[5] = BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER;
        buf[6] = BLE_ANCS_NOTIF_ATTR_ID_TITLE;
        // 2 byte length
        buf[7] = max_len;
        buf[8] = max_len >> 8;
        buf[9] = BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE;
        buf[10] = max_len;
        buf[11] = max_len >> 8;
        buf[12] = BLE_ANCS_NOTIF_ATTR_ID_MESSAGE;
        buf[13] = max_len;
        buf[14] = max_len >> 8;
        buf[15] = BLE_ANCS_NOTIF_ATTR_ID_MESSAGE_SIZE;
        buf[16] = BLE_ANCS_NOTIF_ATTR_ID_DATE;
        buf[17] = BLE_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL;
        buf[18] = BLE_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL;
        AttcWriteReq(anccCb.connId, pHdlList[ANCC_CONTROL_POINT_HDL_IDX], sizeof(buf), buf);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AncsGetAppAttribute
 *
 *  \brief  Send a command to the apple notification center service control point.
 *
 *  \param  pHdlList  Connection identifier.
 *  \param  pAppId    Attribute handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AncsGetAppAttribute(uint16_t *pHdlList, uint8_t *pAppId)
{
    // An example to get app attributes
    uint8_t buf[64];   // to hold the command, size of app identifier is unknown
    uint8_t count = 0;
    if (pHdlList[ANCC_CONTROL_POINT_HDL_IDX] != ATT_HANDLE_NONE)
    {
        buf[0] = BLE_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES;    // put command

        while (pAppId[count++] != 0);   // NULL terminated string
        if ( count > (64 - 2) )
        {
            // app identifier is too long
        }
        else
        {
            memcpy(&buf[1], pAppId, count);
        }

        buf[count + 1] = 0; // app attribute id
        AttcWriteReq(anccCb.connId, pHdlList[ANCC_CONTROL_POINT_HDL_IDX], (count + 2), buf);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AncsPerformNotiAction
 *
 *  \brief  Send a command to the apple notification center service control point.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  actionId  Notification action identifier.
 *  \param  notiUid   NotificationUid.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AncsPerformNotiAction(uint16_t *pHdlList, uint32_t notiUid, ancc_notif_action_id_values_t actionId)
{
    // An example to performs notification action
    uint8_t buf[6];   //to hold the command, size of app identifier is unknown
    if (pHdlList[ANCC_CONTROL_POINT_HDL_IDX] != ATT_HANDLE_NONE)
    {
        buf[0] = BLE_ANCS_COMMAND_ID_GET_PERFORM_NOTIF_ACTION;    // put command
        uint8_t * p = &buf[1];
        UINT32_TO_BSTREAM(p, notiUid);      // encode notification uid
        buf[5] = actionId;                  //action id
        AttcWriteReq(anccCb.connId, pHdlList[ANCC_CONTROL_POINT_HDL_IDX], sizeof(buf), buf);
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AnccActionHandler
 *
 *  \brief  Routine to handle ancc related actions.
 *
 *  \param  actionTimerEvt  WsfTimer event indication.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccActionHandler(uint8_t actionTimerEvt)
{
    // perform action
    if ( anccActionListPop() )
    {
        AnccGetNotificationAttribute(anccCb.hdlList, anccCb.anccList[anccCb.active.handle].notification_uid);
        AnccActionStart(actionTimerEvt);
    }
    else
    {
        //list empty
        AnccActionStop();
    }
}

/*************************************************************************************************/
/*!
 *  \fn     anccAttrHandler
 *
 *  \brief  Static routine to handle attribute receiving and processing.
 *
 *  \param  pMsg      pointer to ATT message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static bool anccAttrHandler(attEvt_t * pMsg)
{
    uint8_t count = 0;
    uint16_t bytesRemaining = 0;

    switch(anccCb.active.attrState)
    {
        case NOTI_ATTR_NEW_NOTIFICATION:
            // new notification
            anccCb.active.commandId = pMsg->pValue[0];
            if ( anccCb.active.commandId == BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES )
            {
                BYTES_TO_UINT32(anccCb.active.notiUid, &(pMsg->pValue[1]));
                count = 4;
            }
            else if ( anccCb.active.commandId == BLE_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES )
            {
                while(pMsg->pValue[count + 1] != 0) // NULL terminated string
                {
                    anccCb.active.appId[count] = pMsg->pValue[count + 1];
                    count++;
                }
                anccCb.active.appId[count + 1] = 0; // NULL terminated string
            }
            else
            {
                // BLE_ANCS_COMMAND_ID_GET_PERFORM_NOTIF_ACTION
                return false;
            }
            anccCb.active.parseIndex += count + 1;
            anccCb.active.attrState = NOTI_ATTR_NEW_ATTRIBUTE;
            anccCb.active.attrId = 0;
            anccCb.active.attrCount = 0;

            if ( pMsg->valueLen > ANCC_ATTRI_BUFFER_SIZE_BYTES )
            {
                // notification size overflow
            }
            else
            {
                bytesRemaining = pMsg->valueLen;
            }
            // copy data
            memset(anccCb.active.attrDataBuf, 0, ANCC_ATTRI_BUFFER_SIZE_BYTES);
            anccCb.active.bufIndex = 0;
            for (uint16_t i = 0; i < bytesRemaining; i++)
            {
                anccCb.active.attrDataBuf[anccCb.active.bufIndex++] = pMsg->pValue[i];
            }
        // no break here by intention
        case NOTI_ATTR_NEW_ATTRIBUTE:
            // new attribute
            // check consistency of the attribute
            if ( anccCb.active.bufIndex - anccCb.active.parseIndex < 3 ) // 1 byte attribute id + 2 bytes attribute length
            {
                // attribute header not received completely
                anccCb.active.attrState = NOTI_ATTR_RECEIVING_ATTRIBUTE;
                return false;
            }

            anccCb.active.attrId = anccCb.active.attrDataBuf[anccCb.active.parseIndex];
            BYTES_TO_UINT16(anccCb.active.attrLength, &(anccCb.active.attrDataBuf[anccCb.active.parseIndex + 1]));

            if ( anccCb.active.attrLength > (anccCb.active.bufIndex - anccCb.active.parseIndex - 3) ) // 1 byte attribute id + 2 bytes attribute length
            {
                // attribute body not received completely
                anccCb.active.attrState = NOTI_ATTR_RECEIVING_ATTRIBUTE;
                return false;
            }

            // parse attribute
            anccCb.active.attrCount++;
            anccCb.active.parseIndex += 3; // 1 byte attribute id + 2 bytes attribute length

            if ( anccCb.active.attrId == BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER )
            {
                uint8_t temp = 0;
                while(anccCb.active.attrDataBuf[anccCb.active.parseIndex + temp] != 0)  // NULL terminated string
                {
                    anccCb.active.appId[temp] = anccCb.active.attrDataBuf[anccCb.active.parseIndex + temp];
                    temp++;
                }
                anccCb.active.appId[temp] = 0;  // NULL terminated string
            }

            //
            // attribute received
            // execute callback function
            //
            (*anccCb.attrCback)(&anccCb.active);

            anccCb.active.parseIndex += anccCb.active.attrLength;

            if ( anccCb.active.attrCount >= 8 ) //custom criteria
            {
                //notification reception done
                anccCb.active.attrState = NOTI_ATTR_NEW_NOTIFICATION;
                anccCb.active.parseIndex = 0;
                anccCb.active.bufIndex = 0;

                //
                // notification received
                // execute callback function
                //
                (*anccCb.notiCback)(&anccCb.active, anccCb.active.notiUid);

                return false;
            }

            return true; // continue parsing
        // no need to break;
        case NOTI_ATTR_RECEIVING_ATTRIBUTE:
            // notification continuing
            bytesRemaining = 0;
            if ( anccCb.active.bufIndex + pMsg->valueLen > ANCC_ATTRI_BUFFER_SIZE_BYTES )
            {
                // notification size overflow
                anccCb.active.attrState = NOTI_ATTR_NEW_NOTIFICATION;
                anccCb.active.parseIndex = 0;
                anccCb.active.bufIndex = 0;
                return false;
            }
            else
            {
                bytesRemaining = pMsg->valueLen;
            }
            // copy data
            for (uint16_t i = 0; i < bytesRemaining; i++)
            {
                anccCb.active.attrDataBuf[anccCb.active.bufIndex++] = pMsg->pValue[i];
            }
            anccCb.active.attrState = NOTI_ATTR_NEW_ATTRIBUTE;
            return true;
        // no need to break;
    }
    return false;
}
/*************************************************************************************************/
/*!
 *  \fn     AnccNtfValueUpdate
 *
 *  \brief  Routine to handle any ancc related notify.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  actionTimerEvt  WsfTimer event indication.
 *  \param  pMsg      pointer to ATT message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccNtfValueUpdate(uint16_t *pHdlList, attEvt_t * pMsg, uint8_t actionTimerEvt)
{
    ancc_notif_t ancs_notif;
    uint8_t *p;

    //notification received
    if (pMsg->handle == pHdlList[ANCC_NOTIFICATION_SOURCE_HDL_IDX])
    {
        // process notificiation source (brief)
        p = pMsg->pValue;
        ancs_notif.event_id = p[0];
        ancs_notif.event_flags = p[1];
        ancs_notif.category_id = p[2];
        ancs_notif.category_count = p[3];
        BYTES_TO_UINT32(ancs_notif.notification_uid, &p[4]);

        if (BLE_ANCS_EVENT_ID_NOTIFICATION_REMOVED == ancs_notif.event_id)
        {
            if (NULL != anccCb.rmvCback)
            {
                anccCb.rmvCback(&ancs_notif);
            }
        }
        else if ( !anccActionListPush(&ancs_notif) )
        {
            // list full
            // APP_TRACE_INFO0("action list full...");
        }
        else
        {
            //
            // actions to be done with timer delays to avoid generating heavy traffic
            //
            AnccActionStart(actionTimerEvt);
            // APP_TRACE_INFO0("added to action list");
        }
    }
    else if ( pMsg->handle == pHdlList[ANCC_DATA_SOURCE_HDL_IDX] )
    {
        // process notificiation/app attributes
        while(anccAttrHandler(pMsg));
    }
}

/*************************************************************************************************/
/*!
 *  \fn     AnccCbackRegister
 *
 *  \brief  Register the attribute received callback and notification completed callback.
 *
 *  \param  attrCback  Pointer to attribute received callback function.
 *  \param  notiCback  Pointer to notification completed callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnccCbackRegister(anccAttrRecvCback_t attrCback, anccNotiCmplCback_t notiCback, anccNotiRemoveCback_t rmvCback)
{
    anccCb.attrCback = attrCback;
    anccCb.notiCback = notiCback;
    anccCb.rmvCback = rmvCback;
}


/*************************************************************************************************/
/*!
 *  \fn     AnccStartServiceDiscovery
 *
 *  \brief  Do service discovery on connected connection.
 *
 *  \return Return TRUE if service discovery is initiated successfully.
 *
 */
/*************************************************************************************************/
 bool_t AnccStartServiceDiscovery(void)
{
    if (anccCb.connId != DM_CONN_ID_NONE)
    {
        appDiscStart(anccCb.connId);
        return TRUE;
    }
    return FALSE;
}
