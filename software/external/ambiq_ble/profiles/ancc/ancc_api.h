//*****************************************************************************
//
//  ancc_api.h
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

#ifndef ANCC_API_H
#define ANCC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/
/* ANCS Service */                          // 7905F431-B5CE-4E99-A40F-4B1E122D00D0
#define ATT_UUID_ANCS_SERVICE               0xd0, 0x00, 0x2d, 0x12, 0x1e, 0x4b, 0x0f, 0xa4,\
                                            0x99, 0x4e, 0xce, 0xb5, 0x31, 0xf4, 0x05, 0x79
/* ANCS characteristics */
                                            // 9FBF120D-6301-42D9-8C58-25E699A21DBD
#define ATT_UUID_NOTIFICATION_SOURCE        0xbd, 0x1d, 0xa2, 0x99, 0xe6, 0x25, 0x58, 0x8c, \
                                            0xd9, 0x42, 0x01, 0x63, 0x0d, 0x12, 0xbf, 0x9f

                                            //69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9
#define ATT_UUID_CTRL_POINT                 0xd9, 0xd9, 0xaa, 0xfd, 0xbd, 0x9b, 0x21, 0x98,\
                                            0xa8, 0x49, 0xe1, 0x45, 0xf3, 0xd8, 0xd1, 0x69

                                            // 22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB
#define ATT_UUID_DATA_SOURCE                0xfb, 0x7b, 0x7c, 0xce, 0x6a, 0xb3, 0x44, 0xbe,\
                                            0xb5, 0x4b, 0xd6, 0x24, 0xe9, 0xc6, 0xea, 0x22

#define ANCC_LIST_ELEMENTS                  64
#define ANCC_APP_IDENTIFIER_SIZE_BYTES      64
#define ANCC_ATTRI_BUFFER_SIZE_BYTES        512

/**@brief Category IDs for iOS notifications. */
typedef enum
{
    BLE_ANCS_CATEGORY_ID_OTHER,                     /**< The iOS notification belongs to the "other" category.  */
    BLE_ANCS_CATEGORY_ID_INCOMING_CALL,             /**< The iOS notification belongs to the "Incoming Call" category. */
    BLE_ANCS_CATEGORY_ID_MISSED_CALL,               /**< The iOS notification belongs to the "Missed Call" category. */
    BLE_ANCS_CATEGORY_ID_VOICE_MAIL,                /**< The iOS notification belongs to the "Voice Mail" category. */
    BLE_ANCS_CATEGORY_ID_SOCIAL,                    /**< The iOS notification belongs to the "Social" category. */
    BLE_ANCS_CATEGORY_ID_SCHEDULE,                  /**< The iOS notification belongs to the "Schedule" category. */
    BLE_ANCS_CATEGORY_ID_EMAIL,                     /**< The iOS notification belongs to the "E-mail" category. */
    BLE_ANCS_CATEGORY_ID_NEWS,                      /**< The iOS notification belongs to the "News" category. */
    BLE_ANCS_CATEGORY_ID_HEALTH_AND_FITNESS,        /**< The iOS notification belongs to the "Health and Fitness" category. */
    BLE_ANCS_CATEGORY_ID_BUSINESS_AND_FINANCE,      /**< The iOS notification belongs to the "Buisness and Finance" category. */
    BLE_ANCS_CATEGORY_ID_LOCATION,                  /**< The iOS notification belongs to the "Location" category. */
    BLE_ANCS_CATEGORY_ID_ENTERTAINMENT              /**< The iOS notification belongs to the "Entertainment" category. */
} ancc_category_id_values_t;

/**@brief Event IDs for iOS notifications. */
typedef enum
{
    BLE_ANCS_EVENT_ID_NOTIFICATION_ADDED,           /**< The iOS notification was added. */
    BLE_ANCS_EVENT_ID_NOTIFICATION_MODIFIED,        /**< The iOS notification was modified. */
    BLE_ANCS_EVENT_ID_NOTIFICATION_REMOVED          /**< The iOS notification was removed. */
} ancc_evt_id_values_t;

/**@brief Control point command IDs that the Notification Consumer can send to the Notification Provider. */
typedef enum
{
    BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES,       /**< Requests attributes to be sent from the NP to the NC for a given notification. */
    BLE_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES,         /**< Requests attributes to be sent from the NP to the NC for a given iOS App. */
    BLE_ANCS_COMMAND_ID_GET_PERFORM_NOTIF_ACTION,   /**< Requests an action to be performed on a given notification, for example dismiss an alarm. */
} ancc_command_id_values_t;

/**@brief IDs for iOS notification attributes. */
typedef enum
{
    BLE_ANCS_NOTIF_ATTR_ID_APP_IDENTIFIER,          /**< Identifies that the attribute data is of an "App Identifier" type. */
    BLE_ANCS_NOTIF_ATTR_ID_TITLE,                   /**< Identifies that the attribute data is a "Title". Needs to be followed by a 2-bytes max length parameter*/
    BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE,                /**< Identifies that the attribute data is a "Subtitle". Needs to be followed by a 2-bytes max length parameter*/
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE,                 /**< Identifies that the attribute data is a "Message". Needs to be followed by a 2-bytes max length parameter*/
    BLE_ANCS_NOTIF_ATTR_ID_MESSAGE_SIZE,            /**< Identifies that the attribute data is a "Message Size". */
    BLE_ANCS_NOTIF_ATTR_ID_DATE,                    /**< Identifies that the attribute data is a "Date". */
    BLE_ANCS_NOTIF_ATTR_ID_POSITIVE_ACTION_LABEL,   /**< The notification has a "Positive action" that can be executed associated with it. */
    BLE_ANCS_NOTIF_ATTR_ID_NEGATIVE_ACTION_LABEL,   /**< The notification has a "Negative action" that can be executed associated with it. */
} ancc_notif_attr_id_values_t;

/**@brief ActionID values. */
typedef enum
{
    BLE_ANCS_NOTIF_ACTION_ID_POSITIVE,
    BLE_ANCS_NOTIF_ACTION_ID_NEGATIVE,
} ancc_notif_action_id_values_t;


/**@brief Typedef for iOS notifications. */
typedef struct
{
    uint8_t event_id;           /**< This field informs the accessory whether the given iOS notification was added, modified, or removed. The enumerated values for this field are defined in EventID Values.. */
    uint8_t event_flags;        /**< A bitmask whose set bits inform an NC of specificities with the iOS notification. */
    uint8_t category_id;        /**< A numerical value providing a category in which the iOS notification can be classified. */
    uint8_t category_count;     /**< The current number of active iOS notifications in the given category. */
    uint32_t notification_uid;  /**< A 32-bit numerical value that is the unique identifier (UID) for the iOS notification. */
    bool_t  noti_valid;         /**< A flag to indicate whether the notification is still valid in the list. */
} ancc_notif_t;

/*! ancs client enumeration of handle indexes of characteristics to be discovered */
enum
{
  ANCC_NOTIFICATION_SOURCE_HDL_IDX,             // ANCC Notification Source Handle
  ANCC_NOTIFICATION_SOURCE_CCC_HDL_IDX,         // ANCC Notification Source CCC Handle
  ANCC_CONTROL_POINT_HDL_IDX,                   // ANCC Control Point Handle
  ANCC_DATA_SOURCE_HDL_IDX,                     // ANCC Data Source Handle
  ANCC_DATA_SOURCE_CCC_HDL_IDX,                 // ANCC Data Source CCC Handle
  ANCC_HDL_LIST_LEN                             /*! Handle list length */
};

/*! Configurable parameters */
typedef struct
{
    wsfTimerTicks_t     period;                 /*! action timer expiration period in ms */
} anccCfg_t;

typedef enum
{
    NOTI_ATTR_NEW_NOTIFICATION = 0,
    NOTI_ATTR_NEW_ATTRIBUTE,
    NOTI_ATTR_RECEIVING_ATTRIBUTE
} enum_active_state_t;

typedef struct
{
    uint16_t            handle;                 // handle to indicate the current active notification in the list
    enum_active_state_t attrState;
    uint16_t            bufIndex;
    uint16_t            parseIndex;
    uint16_t            attrLength;
    uint8_t             attrId;
    uint16_t            attrCount;
    uint8_t             commandId;
    uint32_t            notiUid;
    uint8_t             appId[ANCC_APP_IDENTIFIER_SIZE_BYTES];
    uint8_t             attrDataBuf[ANCC_ATTRI_BUFFER_SIZE_BYTES];
} active_notif_t;

/*! Application data reception callback */
typedef void (*anccAttrRecvCback_t)(active_notif_t* pAttr);
typedef void (*anccNotiCmplCback_t)(active_notif_t* pAttr, uint32_t notiUid);

/*! Application notify remove callback */
typedef void (*anccNotiRemoveCback_t)(ancc_notif_t* pAttr);
typedef struct
{
    dmConnId_t          connId;
    uint16_t*           hdlList;
    anccCfg_t           cfg;
    wsfTimer_t          actionTimer;                     // perform actions with proper delay
    wsfTimer_t          discoverTimer;                   // perform service discovery delay
    ancc_notif_t        anccList[ANCC_LIST_ELEMENTS];    // buffer size = MAX_LIST_ELEMENTS*sizeof(ancc_notif_t)
    active_notif_t      active;
    anccAttrRecvCback_t attrCback;
    anccNotiCmplCback_t notiCback;
    anccNotiRemoveCback_t rmvCback;
} anccCb_t;

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/
// operation interfaces
void AncsPerformNotiAction(uint16_t *pHdlList, uint32_t notiUid, ancc_notif_action_id_values_t actionId);
void AncsGetAppAttribute(uint16_t *pHdlList, uint8_t* appId);
void AnccGetNotificationAttribute(uint16_t *pHdlList, uint32_t notiUid);

// initialization interfaces
void AnccInit(wsfHandlerId_t handlerId, anccCfg_t* cfg, uint8_t disctimer_event);
void AnccCbackRegister(anccAttrRecvCback_t attrCback, anccNotiCmplCback_t notiCback, anccNotiRemoveCback_t rmvCback);

// app routines
void AnccNtfValueUpdate(uint16_t *pHdlList, attEvt_t * pMsg, uint8_t actionTimerEvt);
void AnccActionHandler(uint8_t actionTimerEvt);
void AnccActionStop(void);
void AnccActionStart(uint8_t timerEvt);
void AnccConnClose(void);
void AnccConnOpen(dmConnId_t connId, uint16_t* hdlList);
void AnccSvcDiscover(dmConnId_t connId, uint16_t *pHdlList);
bool_t AnccStartServiceDiscovery(void);

/* Global variable */
extern anccCb_t    anccCb;
#ifdef __cplusplus
};
#endif

#endif /* ANCC_API_H */
