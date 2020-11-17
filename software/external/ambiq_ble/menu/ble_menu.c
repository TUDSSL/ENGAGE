// ****************************************************************************
//
//  ble_menu.c
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

#include "ble_menu.h"
#include "wsf_types.h"
#include "amdtp_api.h"
#include "app_api.h"

char menuRxData[20];
uint32_t menuRxDataLen = 0;

sBleMenuCb bleMenuCb;

char mainMenuContent[BLE_MENU_ID_MAX][32] = {
    "1. BLE_MENU_ID_GAP",
    "2. BLE_MENU_ID_GATT",
    "3. BLE_MENU_ID_AMDTP",
};

char gapMenuContent[GAP_MENU_ID_MAX][32] = {
    "1. Start Scan",
    "2. Stop Scan",
    "3. Show Scan Results",
    "4. Create Connection"
};

char gattMenuContent[GATT_MENU_ID_MAX][32] = {
    "1. TBD",
};

char amdtpMenuContent[AMDTP_MENU_ID_MAX][64] = {
    "1. Send test data continuously",
    "2. Stop sending test data",
    "3. Request Server to send",
    "4. Request Server to stop sending"
};

static void BleMenuShowMenu(void);

//*****************************************************************************
//
// Transmit delay waits for busy bit to clear to allow
// for a transmission to fully complete before proceeding.
//
//*****************************************************************************
void
uart_transmit_delay(uint32_t ui32Module)
{
  //
  // Wait until busy bit clears to make sure UART fully transmitted last byte
  //
  //while ( am_hal_uart_flags_get(ui32Module) & AM_HAL_UART_FR_BUSY );
}


static bool isSelectionHome(void)
{
    if (menuRxData[0] == 'h')
    {
        bleMenuCb.menuId = BLE_MENU_ID_MAIN;
        bleMenuCb.prevMenuId = BLE_MENU_ID_MAIN;
        bleMenuCb.gapMenuSelected = GAP_MENU_ID_NONE;
        return true;
    }
    return false;
}

static void showScanResults(void)
{
    appDevInfo_t *devInfo;
    uint8_t num = AppScanGetNumResults();

    am_menu_printf("--------------------Scan Results--------------------\r\n");
    for (int i = 0; i < num; i++)
    {
        devInfo = AppScanGetResult(i);
        if (devInfo)
        {
            am_menu_printf("%d : %d %02x%02x%02x%02x%02x%02x \r\n", i, devInfo->addrType,
            devInfo->addr[0], devInfo->addr[1], devInfo->addr[2], devInfo->addr[3], devInfo->addr[4], devInfo->addr[5]);
        }
    }
    am_menu_printf("-----------------------------------------------------\r\n");
}

static void handleGAPSlection(void)
{
    eGapMenuId id;
    if (bleMenuCb.gapMenuSelected == GAP_MENU_ID_NONE)
    {
        id = (eGapMenuId)(menuRxData[0] - '0');
    }
    else
    {
        id = bleMenuCb.gapMenuSelected;
    }

    switch (id)
    {
        case GAP_MENU_ID_SCAN_START:
            am_menu_printf("scan start\r\n");
            AmdtpcScanStart();
            break;
        case GAP_MENU_ID_SCAN_STOP:
            AmdtpcScanStop();
            break;
        case GAP_MENU_ID_SCAN_RESULTS:
            showScanResults();
            break;
        case GAP_MENU_ID_CONNECT:
            if (bleMenuCb.gapMenuSelected == GAP_MENU_ID_NONE)
            {
                am_menu_printf("choose an idx from scan results to connect:\r\n");
                showScanResults();
                bleMenuCb.gapMenuSelected = GAP_MENU_ID_CONNECT;
            }
            else
            {
                uint8_t idx = menuRxData[0] - '0';
                bleMenuCb.gapMenuSelected = GAP_MENU_ID_NONE;
                AmdtpcConnOpen(idx);
            }
            break;
        default:
            break;
    }
}

static void handleAMDTPSlection(void)
{
    eAmdtpMenuId id;
    id = (eAmdtpMenuId)(menuRxData[0] - '0');

    switch (id)
    {
        case AMDTP_MENU_ID_SEND:
            am_menu_printf("send data to server\r\n");
            AmdtpcSendTestData();
            break;
        case AMDTP_MENU_ID_SEND_STOP:
            am_menu_printf("send data to server stop\r\n");
            AmdtpcSendTestDataStop();
            break;
        case AMDTP_MENU_ID_SERVER_SEND:
            am_menu_printf("request server to send\r\n");
            AmdtpcRequestServerSend();
            break;
        case AMDTP_MENU_ID_SERVER_SEND_STOP:
            am_menu_printf("request server to stop\r\n");
            AmdtpcRequestServerSendStop();
            break;
        default:
            break;
    }
}

static void handleSelection(void)
{
    if (isSelectionHome())
    {
        BleMenuShowMenu();
        return;
    }

    switch (bleMenuCb.menuId)
    {
        case BLE_MENU_ID_MAIN:
            bleMenuCb.menuId = (eBleMenuId)(menuRxData[0] - '0');
            BleMenuShowMenu();
            break;
        case BLE_MENU_ID_GAP:
            handleGAPSlection();
            break;
        case BLE_MENU_ID_GATT:
        break;
        case BLE_MENU_ID_AMDTP:
            handleAMDTPSlection();
            break;
        default:
            am_util_debug_printf("handleSelection() unknown input\n");
            break;
    }
}

void
BleMenuRx(void)
{
    if (menuRxDataLen == 0)
    {
        return;
    }

    menuRxData[menuRxDataLen] = '\0';
    am_util_debug_printf("BleMenuRx data = %s\n", menuRxData);

    handleSelection();
    // clear UART rx buffer
    memset(menuRxData, 0, sizeof(menuRxData));
    menuRxDataLen = 0;
}

static void
BLEMenuShowMainMenu(void)
{
    am_menu_printf("--------------------BLE main menu--------------------\r\n");
    for (int i = 0; i < BLE_MENU_ID_MAX; i++)
    {
        am_menu_printf("%s\r\n", mainMenuContent[i]);
        uart_transmit_delay(AM_BSP_UART_PRINT_INST);
    }
    am_menu_printf("hint: use 'h' to do main menu\r\n");
    am_menu_printf("-----------------------------------------------------\r\n");
}

static void
BLEMenuShowGAPMenu(void)
{
    for (int i = 0; i < GAP_MENU_ID_MAX; i++)
    {
        am_menu_printf("%s\r\n", gapMenuContent[i]);
        uart_transmit_delay(AM_BSP_UART_PRINT_INST);
    }
}

static void
BLEMenuShowGATTMenu(void)
{
    for (int i = 0; i < GATT_MENU_ID_MAX; i++)
    {
        am_menu_printf("%s\r\n", gattMenuContent[i]);
        uart_transmit_delay(AM_BSP_UART_PRINT_INST);
    }
}

static void
BLEMenuShowAMDTPMenu(void)
{
    for (int i = 0; i < AMDTP_MENU_ID_MAX; i++)
    {
        am_menu_printf("%s\r\n", amdtpMenuContent[i]);
        uart_transmit_delay(AM_BSP_UART_PRINT_INST);
    }
}

static void
BleMenuShowMenu(void)
{
    switch (bleMenuCb.menuId)
    {
        case BLE_MENU_ID_MAIN:
            BLEMenuShowMainMenu();
            break;
        case BLE_MENU_ID_GAP:
            BLEMenuShowGAPMenu();
            break;
        case BLE_MENU_ID_GATT:
            BLEMenuShowGATTMenu();
            break;
        case BLE_MENU_ID_AMDTP:
            BLEMenuShowAMDTPMenu();
            break;
        default:
            break;
    }
}

void
BleMenuInit(void)
{
    bleMenuCb.prevMenuId = BLE_MENU_ID_MAIN;
    bleMenuCb.menuId = BLE_MENU_ID_MAIN;
    bleMenuCb.gapMenuSelected = GAP_MENU_ID_NONE;
    bleMenuCb.gattMenuSelected = GATT_MENU_ID_NONE;
    BleMenuShowMenu();
}
