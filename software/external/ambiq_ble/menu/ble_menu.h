// ****************************************************************************
//
//  ble_menu.h
//! @file
//!
//! @brief Functions for BLE control menu.
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

#ifndef BLE_MENU_H
#define BLE_MENU_H

#include "am_util.h"
#include "am_util_stdio.h"
#include "am_bsp.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    BLE_MENU_ID_MAIN = 0,
    BLE_MENU_ID_GAP,
    BLE_MENU_ID_GATT,
    BLE_MENU_ID_AMDTP,
    BLE_MENU_ID_MAX
}eBleMenuId;

typedef enum
{
    GAP_MENU_ID_NONE = 0,
    GAP_MENU_ID_SCAN_START,
    GAP_MENU_ID_SCAN_STOP,
    GAP_MENU_ID_SCAN_RESULTS,
    GAP_MENU_ID_CONNECT,
    GAP_MENU_ID_MAX
}eGapMenuId;

typedef enum
{
    GATT_MENU_ID_NONE = 0,
    GATT_MENU_ID_TBD,
    GATT_MENU_ID_MAX
}eGattMenuId;

typedef enum
{
    AMDTP_MENU_ID_NONE = 0,
    AMDTP_MENU_ID_SEND,
    AMDTP_MENU_ID_SEND_STOP,
    AMDTP_MENU_ID_SERVER_SEND,
    AMDTP_MENU_ID_SERVER_SEND_STOP,
    AMDTP_MENU_ID_MAX
}eAmdtpMenuId;

typedef struct
{
    eBleMenuId prevMenuId;
    eBleMenuId menuId;
    eGapMenuId gapMenuSelected;
    eGattMenuId gattMenuSelected;
}sBleMenuCb;

extern char menuRxData[20];
extern uint32_t menuRxDataLen;

extern uint32_t am_menu_printf(const char *pcFmt, ...);

void
BleMenuRx(void);

void
BleMenuInit(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_MENU_H
