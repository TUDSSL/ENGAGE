//*****************************************************************************
//
//! amotas_main.c
//! @file
//!
//! @brief This file provides the main application for the AMOTA service.
//!
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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "att_api.h"
#include "svc_ch.h"
#include "svc_amotas.h"
#include "app_api.h"
#include "app_hw.h"
#include "amotas_api.h"
#include "am_util_debug.h"
#include "crc32.h"

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "am_bootloader.h"

#include "amota_profile_config.h"
#include "am_multi_boot.h"

#undef  APP_TRACE_INFO0
#undef  APP_TRACE_INFO1
#undef  APP_TRACE_INFO2
#undef  APP_TRACE_INFO3

#define APP_TRACE_INFO0(msg)
#define APP_TRACE_INFO1(msg, var1)
#define APP_TRACE_INFO2(msg, var1, var2)
#define APP_TRACE_INFO3(msg, var1, var2, var3)

static am_multiboot_flash_info_t *g_pFlash = &g_intFlash;

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
#if (AMOTAS_SUPPORT_EXT_FLASH == 1)
// OTA using external flash requires secondary bootloader
// Customer specific Magic#'s can be used to communicate information from
// AMOTA to the secondary bootloader, in a vendor specific way
#error "External flash based OTA not supported in Apollo3"
#endif
#endif

// Need temporary buf equal to one flash page size (larger of int or ext, if ext flash is supported)
// We accumulate data in this buffer and perform Writes only on page boundaries in
// multiple of page lentghs
#if (AMOTAS_SUPPORT_EXT_FLASH == 1) && (AM_DEVICES_SPIFLASH_PAGE_SIZE > AM_HAL_FLASH_PAGE_SIZE)
#define AMOTAS_TEMP_BUFSIZE     AM_DEVICES_SPIFLASH_PAGE_SIZE
#else
#define AMOTAS_TEMP_BUFSIZE     AM_HAL_FLASH_PAGE_SIZE
#endif


// Protection against NULL pointer
#define FLASH_OPERATE(pFlash, func) ((pFlash)->func ? (pFlash)->func() : 0)


//
// Data structure for flash operation
//
typedef struct
{
    uint8_t     writeBuffer[AMOTAS_TEMP_BUFSIZE]   __attribute__((aligned(4)));   // needs to be 32-bit word aligned.
    uint16_t    bufferIndex;
}amotasFlashOp_t;

amotasFlashOp_t amotasFlash = {
    .bufferIndex = 0,
};

// Temporary scratch buffer used to read from flash
uint32_t amotasTmpBuf[AMOTA_PACKET_SIZE / 4];

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

//
// amota states
//
typedef enum
{
    AMOTA_STATE_INIT,
    AMOTA_STATE_GETTING_FW,
    AMOTA_STATE_MAX
}eAmotaState;

//
// amota commands
//
typedef enum
{
    AMOTA_CMD_UNKNOWN,
    AMOTA_CMD_FW_HEADER,
    AMOTA_CMD_FW_DATA,
    AMOTA_CMD_FW_VERIFY,
    AMOTA_CMD_FW_RESET,
    AMOTA_CMD_MAX
}eAmotaCommand;

//
// amota status
//
typedef enum
{
    AMOTA_STATUS_SUCCESS,
    AMOTA_STATUS_CRC_ERROR,
    AMOTA_STATUS_INVALID_HEADER_INFO,
    AMOTA_STATUS_INVALID_PKT_LENGTH,
    AMOTA_STATUS_INSUFFICIENT_BUFFER,
    AMOTA_STATUS_INSUFFICIENT_FLASH,
    AMOTA_STATUS_UNKNOWN_ERROR,
    AMOTA_STATUS_FLASH_WRITE_ERROR,
    AMOTA_STATUS_MAX
}eAmotaStatus;


//
// FW header information
//
typedef struct
{
    uint32_t    encrypted;
    uint32_t    fwStartAddr;            // Address to install the image
    uint32_t    fwLength;
    uint32_t    fwCrc;
    uint32_t    secInfoLen;
    uint32_t    resvd1;
    uint32_t    resvd2;
    uint32_t    resvd3;
    uint32_t    version;
    uint32_t    fwDataType;             //binary type
    uint32_t    storageType;
    uint32_t    resvd4;

}
amotaHeaderInfo_t;

//
// FW header information
//
typedef struct
{
    uint16_t    offset;
    uint16_t    len;                        // data plus checksum
    eAmotaCommand type;
    uint8_t     data[AMOTA_PACKET_SIZE]  __attribute__((aligned(4)));   // needs to be 32-bit word aligned.
}
amotaPacket_t;

//
// Connection control block
//
typedef struct
{
    dmConnId_t    connId;               // Connection ID
    bool_t        amotaToSend;          // AMOTA notify ready to be sent on this channel
}
amotasConn_t;

//
// Firmware Address
//
typedef struct
{
    uint32_t    addr;
    uint32_t    offset;
}
amotasNewFwFlashInfo_t;


/* Control block */
static struct
{
    amotasConn_t            conn[DM_CONN_MAX];    // connection control block
    bool_t                  txReady;              // TRUE if ready to send notifications
    wsfHandlerId_t          appHandlerId;
    AmotasCfg_t             cfg;                  // configurable parameters
    eAmotaState             state;
    amotaHeaderInfo_t       fwHeader;
    amotaPacket_t           pkt;
    amotasNewFwFlashInfo_t  newFwFlashInfo;
    wsfTimer_t              resetTimer;           // reset timer after OTA update done
    wsfTimer_t              disconnectTimer;      // Disconnect timer after OTA update done
}
amotasCb;


// Erases the flash based on ui32Addr & ui32NumBytes
void
erase_flash(uint32_t ui32Addr, uint32_t ui32NumBytes)
{
    // Erase the image
    while ( ui32NumBytes )
    {
        g_pFlash->flash_erase_sector(ui32Addr);
        if ( ui32NumBytes > g_pFlash->flashSectorSize )
        {
            ui32NumBytes -= g_pFlash->flashSectorSize;
            ui32Addr += g_pFlash->flashSectorSize;
        }
        else
        {
            break;
        }
    }
}

//*****************************************************************************
//
// Connection Open event
//
//*****************************************************************************
static void
amotas_conn_open(dmEvt_t *pMsg)
{
    hciLeConnCmplEvt_t *evt = (hciLeConnCmplEvt_t*) pMsg;
    amotasCb.txReady = TRUE;

    (void)evt;

    APP_TRACE_INFO0("connection opened");
    APP_TRACE_INFO1("handle = 0x%x", evt->handle);
    APP_TRACE_INFO1("role = 0x%x", evt->role);
    APP_TRACE_INFO3("addrMSB = %02x%02x%02x%02x%02x%02x", evt->peerAddr[0], evt->peerAddr[1], evt->peerAddr[2]);
    APP_TRACE_INFO3("addrLSB = %02x%02x%02x%02x%02x%02x", evt->peerAddr[3], evt->peerAddr[4], evt->peerAddr[5]);
    APP_TRACE_INFO1("connInterval = 0x%x", evt->connInterval);
    APP_TRACE_INFO1("connLatency = 0x%x", evt->connLatency);
    APP_TRACE_INFO1("supTimeout = 0x%x", evt->supTimeout);
}

//*****************************************************************************
//
// Connection Update event
//
//*****************************************************************************
static void
amotas_conn_update(dmEvt_t *pMsg)
{
    hciLeConnUpdateCmplEvt_t *evt = (hciLeConnUpdateCmplEvt_t*) pMsg;

    (void)evt;

    APP_TRACE_INFO1("connection update status = 0x%x", evt->status);
    APP_TRACE_INFO1("handle = 0x%x", evt->handle);
    APP_TRACE_INFO1("connInterval = 0x%x", evt->connInterval);
    APP_TRACE_INFO1("connLatency = 0x%x", evt->connLatency);
    APP_TRACE_INFO1("supTimeout = 0x%x", evt->supTimeout);
}

//*****************************************************************************
//
// Find Next Connection to Send on
//
//*****************************************************************************
static amotasConn_t*
amotas_find_next2send(void)
{
    amotasConn_t *pConn = amotasCb.conn;
    uint8_t i;

    for (i = 0; i < DM_CONN_MAX; i++, pConn++)
    {
        if (pConn->connId != DM_CONN_ID_NONE && pConn->amotaToSend)
        {
            //if (AttsCccEnabled(pConn->connId, cccIdx))
            return pConn;
        }
    }
    return NULL;
}


//*****************************************************************************
//
// Timer Expiration handler
//
//*****************************************************************************

void
amotas_disconnect_timer_expired(wsfMsgHdr_t *pMsg)
{
    amotasConn_t *pConn = amotas_find_next2send();
    if ( pConn )
    {
        AppConnClose(pConn->connId);
    }

    APP_TRACE_INFO0("disconnect BLE connection");

    //
    // Delay here to let disconnect req go through
    // the RF before we reboot.
    //
    WsfTimerStartMs(&amotasCb.resetTimer, 200);
}

void
amotas_reset_timer_expired(wsfMsgHdr_t *pMsg)
{
    APP_TRACE_INFO0("amotas_reset_board");
    am_util_delay_ms(10);
#if AM_APOLLO3_RESET
    am_hal_reset_control(AM_HAL_RESET_CONTROL_SWPOI, 0);
#else // AM_APOLLO3_RESET
    am_hal_reset_poi();
#endif // AM_APOLLO3_RESET
}

//*****************************************************************************
//
// Send Notification to Client
//
//*****************************************************************************
static void
amotas_send_data(uint8_t *buf, uint16_t len)
{
    amotasConn_t *pConn = amotas_find_next2send();
    /* send notification */
    if ( pConn )
    {
        APP_TRACE_INFO1("Send to connId = %d", pConn->connId);
        AttsHandleValueNtf(pConn->connId, AMOTAS_TX_HDL, len, buf);
    }
    else
    {
        APP_TRACE_INFO0("Invalid Conn");
    }
}

//*****************************************************************************
//
// Send Reply to Client
//
//*****************************************************************************
static void
amotas_reply_to_client(eAmotaCommand cmd, eAmotaStatus status,
                       uint8_t *data, uint16_t len)
{
    uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN] = {0};
    buf[0] = (len + 1) & 0xff;
    buf[1] = ((len + 1) >> 8) & 0xff;
    buf[2] = cmd;
    buf[3] = status;
    if ( len > 0 )
    {
        memcpy(buf + 4, data, len);
    }
    amotas_send_data(buf, len + 4);
}

//*****************************************************************************
//
// Set Firmware Address
//
//return true if success, otherwise false
//*****************************************************************************
static bool
amotas_set_fw_addr(void)
{
    bool bResult = false;

    amotasCb.newFwFlashInfo.addr = 0;
    amotasCb.newFwFlashInfo.offset = 0;

    //
    // Check storage type
    //
    if ( amotasCb.fwHeader.storageType == AMOTA_FW_STORAGE_INTERNAL )
    {
        // storage in internal flash
        uint32_t storeAddr = (AMOTA_INT_FLASH_OTA_ADDRESS + AM_HAL_FLASH_PAGE_SIZE - 1) & ~(AM_HAL_FLASH_PAGE_SIZE - 1);
        uint32_t maxSize = AMOTA_INT_FLASH_OTA_MAX_SIZE & ~(AM_HAL_FLASH_PAGE_SIZE - 1);

#if !defined(AM_PART_APOLLO3) && !defined(AM_PART_APOLLO3P) // There is no easy way to get the information about the main image in Apollo3
        uint32_t ui32CurLinkAddr;
        uint32_t ui32CurLen;
        // Get information about current main image
        if (am_multiboot_get_main_image_info(&ui32CurLinkAddr, &ui32CurLen) == false)
        {
            return false;
        }
        // Length in flash page size multiples
        ui32CurLen = (ui32CurLen + (AM_HAL_FLASH_PAGE_SIZE - 1)) & ~(AM_HAL_FLASH_PAGE_SIZE - 1) ;
        // Make sure the OTA area does not overwrite the main image
        if (!((storeAddr + maxSize) <= ui32CurLinkAddr) &&
            !(storeAddr >= (ui32CurLinkAddr + ui32CurLen)))
        {
            APP_TRACE_INFO0("OTA memory overlaps with main firmware");
            return false;
        }
#endif
        // Check to make sure the incoming image will fit in the space allocated for OTA
        if (amotasCb.fwHeader.fwLength > maxSize)
        {
            APP_TRACE_INFO2("not enough OTA space allocated = %d bytes, Desired = %d bytes",
                maxSize, amotasCb.fwHeader.fwLength);
            return false;
        }

        g_pFlash = &g_intFlash;
        amotasCb.newFwFlashInfo.addr = storeAddr;
        bResult = true;
    }
    else if ( amotasCb.fwHeader.storageType == AMOTA_FW_STORAGE_EXTERNAL )
    {
        //storage in external flash

#if (AMOTAS_SUPPORT_EXT_FLASH == 1)
        //
        // update target address information
        //
        g_pFlash = &g_extFlash;

        if (g_pFlash->flash_read_page &&
            g_pFlash->flash_write_page &&
            g_pFlash->flash_erase_sector &&
            (amotasCb.fwHeader.fwLength <= AMOTA_EXT_FLASH_OTA_MAX_SIZE))
        {
            amotasCb.newFwFlashInfo.addr = AMOTA_EXT_FLASH_OTA_ADDRESS;
            bResult = true;
        }
        else
#endif
        {
            bResult = false;
        }
    }
    else
    {
        // reserved state
        bResult = false;
    }
    if (bResult == true)
    {
        //
        // Initialize the flash device.
        //
        if (FLASH_OPERATE(g_pFlash, flash_init) == 0)
        {
            if (FLASH_OPERATE(g_pFlash, flash_enable) != 0)
            {
                FLASH_OPERATE(g_pFlash, flash_deinit);
                bResult = false;
            }
            //
            // Erase necessary sectors in the flash according to length of the image.
            //
            erase_flash(amotasCb.newFwFlashInfo.addr, amotasCb.fwHeader.fwLength);

            FLASH_OPERATE(g_pFlash, flash_disable);
        }
        else
        {
            bResult = false;
        }
    }
    return bResult;
}

static int
verify_flash_content(uint32_t flashAddr, uint32_t *pSram, uint32_t len, am_multiboot_flash_info_t *pFlash)
{
    // read back and check
    uint32_t  offset = 0;
    uint32_t  remaining = len;
    int       ret = 0;
    while (remaining)
    {
        uint32_t tmpSize =
            (remaining > AMOTA_PACKET_SIZE) ? AMOTA_PACKET_SIZE : remaining;
        pFlash->flash_read_page((uint32_t)amotasTmpBuf, (uint32_t *)(flashAddr + offset), tmpSize);

        ret = memcmp(amotasTmpBuf, (uint8_t*)((uint32_t)pSram + offset), tmpSize);

        if ( ret != 0 )
        {
            // there is write failure happened.
            APP_TRACE_INFO2("flash write verify failed. address 0x%x. length %d", flashAddr, len);
            break;
        }
        offset += tmpSize;
        remaining -= tmpSize;
    }
    return ret;
}

//*****************************************************************************
//
// Write to Flash
//
//return true if success, otherwise false
//*****************************************************************************
#if 1
static bool amotas_write2flash(uint16_t len, uint8_t *buf, uint32_t addr, bool lastPktFlag)
{
    uint16_t ui16BytesRemaining = len;
    uint32_t ui32TargetAddress = 0;
    uint8_t ui8PageCount = 0;
    bool bResult = true;
    uint16_t i;

    addr -= amotasFlash.bufferIndex;
    //
    // Check the target flash address to ensure we do not operation the wrong address
    // make sure to write to page boundary
    //
    if (((uint32_t)amotasCb.newFwFlashInfo.addr > addr) ||
        (addr & (g_pFlash->flashPageSize - 1)))
    {
        //
        // application is trying to write to wrong address
        //
        return false;
    }

    FLASH_OPERATE(g_pFlash, flash_enable);
    while (ui16BytesRemaining)
    {
        uint16_t ui16Bytes2write = g_pFlash->flashPageSize - amotasFlash.bufferIndex;
        if (ui16Bytes2write > ui16BytesRemaining)
        {
            ui16Bytes2write = ui16BytesRemaining;
        }
        // move data into buffer
        for ( i = 0; i < ui16Bytes2write; i++ )
        {
            // avoid using memcpy
            amotasFlash.writeBuffer[amotasFlash.bufferIndex++] = buf[i];
        }
        ui16BytesRemaining -= ui16Bytes2write;
        buf += ui16Bytes2write;

        //
        // Write to flash when there is data more than 1 page size
        // For last fragment write even if it is less than one page
        //
        if (lastPktFlag || (amotasFlash.bufferIndex == g_pFlash->flashPageSize))
        {
            ui32TargetAddress = (addr + ui8PageCount*g_pFlash->flashPageSize);

            // Always write whole pages
            if ((g_pFlash->flash_write_page(ui32TargetAddress, (uint32_t *)amotasFlash.writeBuffer, g_pFlash->flashPageSize) != 0)
                || (verify_flash_content(ui32TargetAddress, (uint32_t *)amotasFlash.writeBuffer, amotasFlash.bufferIndex, g_pFlash) != 0))
            {
                bResult = false;
                break;
            }
            APP_TRACE_INFO2("Flash write succeeded to address 0x%x. length %d", ui32TargetAddress, amotasFlash.bufferIndex);

            ui8PageCount++;
            amotasFlash.bufferIndex = 0;
            bResult = true;
        }
    }
    FLASH_OPERATE(g_pFlash, flash_disable);

    //
    // If we get here, operations are done correctly
    //
    return bResult;
}
#else
static bool amotas_write2flash(uint16_t len, uint8_t *buf, uint32_t addr, bool lastPktFlag)
{
    //
    // Program the flash page with the data.
    // Start a critical section.
    //
    if (am_hal_flash_program_main(AM_HAL_FLASH_PROGRAM_KEY, (uint32_t *)buf, (uint32_t *)addr, len / 4) != 0)
    {
        // flash helpers return non-zero for false, zero for success
        return false;
    }
    else
    {
        return true;
    }
}
#endif


//*****************************************************************************
//
// Verify Firmware Image CRC
//
//return true if success, otherwise false
//*****************************************************************************
static bool_t
amotas_verify_firmware_crc(void)
{
    // read back the whole firmware image from flash and calculate CRC
    uint32_t ui32CRC = 0;

    //
    // Check crc in external flash
    //
    FLASH_OPERATE(g_pFlash, flash_enable);

    // read from spi flash and calculate CRC32
    for ( uint16_t i = 0; i < (amotasCb.fwHeader.fwLength / AMOTA_PACKET_SIZE); i++ )
    {
        g_pFlash->flash_read_page((uint32_t)amotasTmpBuf,
            (uint32_t *)(amotasCb.newFwFlashInfo.addr + i*AMOTA_PACKET_SIZE),
            AMOTA_PACKET_SIZE);

        am_bootloader_partial_crc32(amotasTmpBuf, AMOTA_PACKET_SIZE, &ui32CRC);
    }

    uint32_t ui32Remainder = amotasCb.fwHeader.fwLength % AMOTA_PACKET_SIZE;
    if ( ui32Remainder )
    {
        g_pFlash->flash_read_page((uint32_t)amotasTmpBuf,
            (uint32_t *)(amotasCb.newFwFlashInfo.addr + amotasCb.fwHeader.fwLength - ui32Remainder),
            ui32Remainder);

        am_bootloader_partial_crc32(amotasTmpBuf, ui32Remainder, &ui32CRC);
    }


    FLASH_OPERATE(g_pFlash, flash_disable);

    return (ui32CRC == amotasCb.fwHeader.fwCrc);
}

//*****************************************************************************
//
// Update OTA information with Firmware Information.
//
//*****************************************************************************
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
static void
amotas_update_ota(void)
{
    uint8_t  magic = *((uint8_t *)(amotasCb.newFwFlashInfo.addr + 3));

    // Set OTAPOINTER
    am_hal_ota_add(AM_HAL_FLASH_PROGRAM_KEY, magic, (uint32_t *)amotasCb.newFwFlashInfo.addr);
}

static void
amotas_init_ota(void)
{
    uint32_t *pOtaDesc = (uint32_t *)(OTA_POINTER_LOCATION & ~(AM_HAL_FLASH_PAGE_SIZE - 1));
    // Initialize OTA descriptor - This should ideally be initiated through a separate command
    // to facilitate multiple image upgrade in a single reboot
    // Will need change in the AMOTA app to do so
    am_hal_ota_init(AM_HAL_FLASH_PROGRAM_KEY, pOtaDesc);
}
#else
static void
amotas_update_ota(void)
{
    am_multiboot_ota_t otaInfo;
    am_multiboot_ota_t *pOtaInfo = &otaInfo;
    uint32_t otaDescAddr;

    // We do not want to disturb the content of the flash page where OTA descriptor
    // resides, other than the OTA descriptor itself
    // This is to avoid needing a separate flash page just for this purpose
    // It allows the flash page to be shared with some other information if needed
    uint32_t otaPtrPageAddr = (OTA_POINTER_LOCATION & ~(AM_HAL_FLASH_PAGE_SIZE - 1));
    uint32_t otaPtrOffset = OTA_POINTER_LOCATION - otaPtrPageAddr;

    // Use the temporary accumulation buffer as scrach space
    // Take a backup of image info
    memcpy(amotasFlash.writeBuffer, (uint8_t *)otaPtrPageAddr, AM_HAL_FLASH_PAGE_SIZE);

    pOtaInfo->pui32LinkAddress = (uint32_t*)amotasCb.fwHeader.fwStartAddr;
    // When security info is present, it is prepended to the image in the blob
    // Actual image starts at an offset in the blob, and the actual image size
    // needs to be adjusted accordingly
    pOtaInfo->secInfoLen = amotasCb.fwHeader.secInfoLen;
    pOtaInfo->ui32NumBytes = amotasCb.fwHeader.fwLength - amotasCb.fwHeader.secInfoLen;

    pOtaInfo->ui32ImageCrc = amotasCb.fwHeader.fwCrc;

    pOtaInfo->pui32ImageAddr = (uint32_t*)(amotasCb.newFwFlashInfo.addr + amotasCb.fwHeader.secInfoLen);

    pOtaInfo->magicNum = OTA_INFO_MAGIC_NUM;

#if (AMOTAS_SUPPORT_EXT_FLASH == 1)
    if (amotasCb.fwHeader.storageType == AMOTA_FW_STORAGE_EXTERNAL)
    {
        pOtaInfo->ui32Options = OTA_INFO_OPTIONS_EXT_FLASH;
        g_pFlash->flash_read_page((uint32_t)amotasTmpBuf,
            (uint32_t *)amotasCb.newFwFlashInfo.addr, pOtaInfo->secInfoLen);

        // Store secInfo immediately following the OTA descriptor
        pOtaInfo->pui32SecInfoPtr = (uint32_t*)(OTA_POINTER_LOCATION + 4 + sizeof(otaInfo));

        // Copy the secInfo
        am_bootloader_write_flash_within_page((uint32_t)pOtaInfo->pui32SecInfoPtr,
                                               amotasTmpBuf, pOtaInfo->secInfoLen / 4);
    }
    else
#endif
    {
        pOtaInfo->ui32Options = 0;
        pOtaInfo->pui32SecInfoPtr = (uint32_t*)(amotasCb.newFwFlashInfo.addr);
    }
    if (amotasCb.fwHeader.fwDataType != 0)
    {
        pOtaInfo->ui32Options |= OTA_INFO_OPTIONS_DATA;
    }

    // Compute CRC of the OTA Descriptor
    pOtaInfo->ui32Crc = 0;
    am_bootloader_partial_crc32((uint32_t *)pOtaInfo, sizeof(*pOtaInfo) - 4, &pOtaInfo->ui32Crc);

    otaDescAddr = OTA_POINTER_LOCATION + 4;
    // Copy the OTA Pointer
    memcpy(&amotasFlash.writeBuffer[otaPtrOffset], (uint8_t *)&otaDescAddr, 4);
    // Copy the OTA descriptor
    memcpy(&amotasFlash.writeBuffer[otaPtrOffset + 4], (uint8_t *)pOtaInfo, sizeof(otaInfo));

    // Write the flash Page
    am_bootloader_program_flash_page(otaPtrPageAddr, (uint32_t *)amotasFlash.writeBuffer, AM_HAL_FLASH_PAGE_SIZE);
}
static void
amotas_init_ota(void)
{
}
#endif

//*****************************************************************************
//
// Handle the various packet types from the Client
//
//*****************************************************************************
void
amotas_packet_handler(eAmotaCommand cmd, uint16_t len, uint8_t *buf)
{
    eAmotaStatus status = AMOTA_STATUS_SUCCESS;
    uint8_t data[4] = {0};
    bool bResult = false;
    uint32_t ver, fwCrc;
    ver = fwCrc = 0;
    bool_t resumeTransfer = FALSE;

    APP_TRACE_INFO2("received packet cmd = 0x%x, len = 0x%x", cmd, len);

    switch(cmd)
    {
        case AMOTA_CMD_FW_HEADER:
            if (len < AMOTA_FW_HEADER_SIZE)
            {
                status = AMOTA_STATUS_INVALID_HEADER_INFO;
                amotas_reply_to_client(cmd, status, NULL, 0);
                break;
            }

            if (amotasCb.state == AMOTA_STATE_GETTING_FW)
            {
                BYTES_TO_UINT32(ver, buf + 32);
                BYTES_TO_UINT32(fwCrc, buf + 12);

                if ( ver == amotasCb.fwHeader.version && fwCrc == amotasCb.fwHeader.fwCrc )
                {
                    resumeTransfer = TRUE;
                }
            }

            BYTES_TO_UINT32(amotasCb.fwHeader.encrypted, buf);
            BYTES_TO_UINT32(amotasCb.fwHeader.fwStartAddr, buf + 4);
            BYTES_TO_UINT32(amotasCb.fwHeader.fwLength, buf + 8);
            BYTES_TO_UINT32(amotasCb.fwHeader.fwCrc, buf + 12);
            BYTES_TO_UINT32(amotasCb.fwHeader.secInfoLen, buf + 16);
            BYTES_TO_UINT32(amotasCb.fwHeader.version, buf + 32);
            BYTES_TO_UINT32(amotasCb.fwHeader.fwDataType, buf + 36);
            BYTES_TO_UINT32(amotasCb.fwHeader.storageType, buf + 40);

            if (resumeTransfer)
            {
                APP_TRACE_INFO1("OTA process start from offset = 0x%x", amotasCb.newFwFlashInfo.offset);
                APP_TRACE_INFO1("beginning of flash addr = 0x%x", amotasCb.newFwFlashInfo.addr);
            }
            else
            {
                APP_TRACE_INFO0("OTA process start from beginning");
                amotasFlash.bufferIndex = 0;
                bResult = amotas_set_fw_addr();

                if ( bResult == false )
                {
                    amotas_reply_to_client(cmd, AMOTA_STATUS_INSUFFICIENT_FLASH, NULL, 0);
                    amotasCb.state = AMOTA_STATE_INIT;
                    return;
                }

                amotasCb.state = AMOTA_STATE_GETTING_FW;
            }
#ifdef AMOTA_DEBUG_ON
            APP_TRACE_INFO0("============= fw header start ===============");
            APP_TRACE_INFO1("encrypted = 0x%x", amotasCb.fwHeader.encrypted);
            APP_TRACE_INFO1("version = 0x%x", amotasCb.fwHeader.version);
            APP_TRACE_INFO1("fwLength = 0x%x", amotasCb.fwHeader.fwLength);
            APP_TRACE_INFO1("fwCrc = 0x%x", amotasCb.fwHeader.fwCrc);
            APP_TRACE_INFO1("fwStartAddr = 0x%x", amotasCb.fwHeader.fwStartAddr);
            APP_TRACE_INFO1("fwDataType = 0x%x", amotasCb.fwHeader.fwDataType);
            APP_TRACE_INFO1("storageType = 0x%x", amotasCb.fwHeader.storageType);
            APP_TRACE_INFO0("============= fw header end ===============");
#endif // AMOTA_DEBUG_ON
            data[0] = ((amotasCb.newFwFlashInfo.offset) & 0xff);
            data[1] = ((amotasCb.newFwFlashInfo.offset >> 8) & 0xff);
            data[2] = ((amotasCb.newFwFlashInfo.offset >> 16) & 0xff);
            data[3] = ((amotasCb.newFwFlashInfo.offset >> 24) & 0xff);
            amotas_reply_to_client(cmd, AMOTA_STATUS_SUCCESS, data, sizeof(data));
        break;

        case AMOTA_CMD_FW_DATA:
            bResult = amotas_write2flash(len, buf, amotasCb.newFwFlashInfo.addr + amotasCb.newFwFlashInfo.offset,
                        ((amotasCb.newFwFlashInfo.offset + len) == amotasCb.fwHeader.fwLength));

            if ( bResult == false )
            {
                data[0] = ((amotasCb.newFwFlashInfo.offset) & 0xff);
                data[1] = ((amotasCb.newFwFlashInfo.offset >> 8) & 0xff);
                data[2] = ((amotasCb.newFwFlashInfo.offset >> 16) & 0xff);
                data[3] = ((amotasCb.newFwFlashInfo.offset >> 24) & 0xff);
                amotas_reply_to_client(cmd, AMOTA_STATUS_FLASH_WRITE_ERROR, data, sizeof(data));
            }
            else
            {
                amotasCb.newFwFlashInfo.offset += len;

                data[0] = ((amotasCb.newFwFlashInfo.offset) & 0xff);
                data[1] = ((amotasCb.newFwFlashInfo.offset >> 8) & 0xff);
                data[2] = ((amotasCb.newFwFlashInfo.offset >> 16) & 0xff);
                data[3] = ((amotasCb.newFwFlashInfo.offset >> 24) & 0xff);
                amotas_reply_to_client(cmd, AMOTA_STATUS_SUCCESS, data, sizeof(data));
            }
        break;

        case AMOTA_CMD_FW_VERIFY:
            if (amotas_verify_firmware_crc())
            {
                APP_TRACE_INFO0("crc verify success");

                amotas_reply_to_client(cmd, AMOTA_STATUS_SUCCESS, NULL, 0);

                //
                // Update flash flag page here
                //
                amotas_update_ota();
            }
            else
            {
                APP_TRACE_INFO0("crc verify failed");
                amotas_reply_to_client(cmd, AMOTA_STATUS_CRC_ERROR, NULL, 0);
            }
            FLASH_OPERATE(g_pFlash, flash_deinit);
            amotasCb.state = AMOTA_STATE_INIT;
            g_pFlash = &g_intFlash;
        break;

        case AMOTA_CMD_FW_RESET:
            APP_TRACE_INFO0("Apollo will disconnect BLE link in 500ms.");
            amotas_reply_to_client(cmd, AMOTA_STATUS_SUCCESS, NULL, 0);

            //
            // Delay here to let packet go through the RF before we disconnect
            //
            WsfTimerStartMs(&amotasCb.disconnectTimer, 500);
        break;

        default:
        break;
    }
}


//*****************************************************************************
//
//! @brief initialize amota service
//!
//! @param handlerId - connection handle
//! @param pCfg - configuration parameters
//!
//! @return None
//
//*****************************************************************************
void
amotas_init(wsfHandlerId_t handlerId, AmotasCfg_t *pCfg)
{
    memset(&amotasCb, 0, sizeof(amotasCb));
    amotasCb.appHandlerId = handlerId;
    amotasCb.txReady = FALSE;
    amotasCb.state = AMOTA_STATE_INIT;
    amotasCb.resetTimer.handlerId = handlerId;
    amotasCb.disconnectTimer.handlerId = handlerId;
    for (int i = 0; i < DM_CONN_MAX; i++)
    {
        amotasCb.conn[i].connId = DM_CONN_ID_NONE;
    }
    amotas_init_ota();
}

void
amotas_conn_close(dmConnId_t connId)
{
    /* clear connection */
    amotasCb.conn[connId - 1].connId = DM_CONN_ID_NONE;
    amotasCb.conn[connId - 1].amotaToSend = FALSE;

    amotasCb.pkt.offset = 0;
    amotasCb.pkt.len = 0;
    amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
}

uint8_t
amotas_write_cback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                   uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    uint8_t dataIdx = 0;
    uint32_t calDataCrc = 0;
#if 0
    uint16_t i = 0;
    APP_TRACE_INFO0("============= data arrived start ===============");
    for (i = 0; i < len; i++)
    {
        APP_TRACE_INFO1("%x\t", pValue[i]);
    }
    APP_TRACE_INFO0("");
    APP_TRACE_INFO0("============= data arrived end ===============");
#endif
    if (amotasCb.pkt.offset == 0 && len < AMOTA_HEADER_SIZE_IN_PKT)
    {
        APP_TRACE_INFO0("Invalid packet!!!");
        amotas_reply_to_client(AMOTA_CMD_FW_HEADER, AMOTA_STATUS_INVALID_PKT_LENGTH, NULL, 0);
        return ATT_SUCCESS;
    }

    // new packet
    if (amotasCb.pkt.offset == 0)
    {
        BYTES_TO_UINT16(amotasCb.pkt.len, pValue);
        amotasCb.pkt.type = (eAmotaCommand) pValue[2];
        dataIdx = 3;
#ifdef AMOTA_DEBUG_ON
        APP_TRACE_INFO1("pkt.len = 0x%x", amotasCb.pkt.len);
        APP_TRACE_INFO1("pkt.type = 0x%x", amotasCb.pkt.type);
#endif
        if (dataIdx > amotasCb.pkt.len)
        {
            APP_TRACE_INFO0("packet length is wrong since it's smaller than 3!");
            return ATT_SUCCESS;
        }
    }

    // make sure we have enough space for new data
    if (amotasCb.pkt.offset + len - dataIdx > AMOTA_PACKET_SIZE)
    {
        APP_TRACE_INFO0("not enough buffer size!!!");
        amotas_reply_to_client(amotasCb.pkt.type, AMOTA_STATUS_INSUFFICIENT_BUFFER, NULL, 0);
        return ATT_SUCCESS;
    }

    // copy new data into buffer and also save crc into it if it's the last frame in a packet
    // 4 bytes crc is included in pkt length
    memcpy(amotasCb.pkt.data + amotasCb.pkt.offset, pValue + dataIdx, len - dataIdx);
    amotasCb.pkt.offset += (len - dataIdx);

    // whole packet received
    if (amotasCb.pkt.offset >= amotasCb.pkt.len)
    {
        uint32_t peerCrc = 0;
        // check CRC
        BYTES_TO_UINT32(peerCrc, amotasCb.pkt.data + amotasCb.pkt.len - AMOTA_CRC_SIZE_IN_PKT);
        calDataCrc = CalcCrc32(0xFFFFFFFFU, amotasCb.pkt.len - AMOTA_CRC_SIZE_IN_PKT, amotasCb.pkt.data);
#ifdef AMOTA_DEBUG_ON
        APP_TRACE_INFO1("calDataCrc = 0x%x", calDataCrc);
        APP_TRACE_INFO1("peerCrc = 0x%x", peerCrc);
#endif

        if (peerCrc != calDataCrc)
        {
            amotas_reply_to_client(amotasCb.pkt.type, AMOTA_STATUS_CRC_ERROR, NULL, 0);

            // clear pkt
            amotasCb.pkt.offset = 0;
            amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
            amotasCb.pkt.len = 0;

            return ATT_SUCCESS;
        }

        amotas_packet_handler(amotasCb.pkt.type, amotasCb.pkt.len - AMOTA_CRC_SIZE_IN_PKT, amotasCb.pkt.data);
        // clear pkt after handled
        amotasCb.pkt.offset = 0;
        amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
        amotasCb.pkt.len = 0;
    }

    return ATT_SUCCESS;
}

void
amotas_start(dmConnId_t connId, uint8_t resetTimerEvt,
             uint8_t disconnectTimerEvt, uint8_t amotaCccIdx)
{
    //
    // set conn id
    //
    amotasCb.conn[connId - 1].connId = connId;
    amotasCb.conn[connId - 1].amotaToSend = TRUE;

    amotasCb.resetTimer.msg.event = resetTimerEvt;
    amotasCb.disconnectTimer.msg.event = disconnectTimerEvt;
}

void
amotas_stop(dmConnId_t connId)
{
    //
    // clear connection
    //
    amotasCb.conn[connId - 1].connId = DM_CONN_ID_NONE;
    amotasCb.conn[connId - 1].amotaToSend = FALSE;
}


//*****************************************************************************
//
//! @brief initialize amota service
//!
//! @param pMsg - WSF message
//!
//! @return None
//
//*****************************************************************************
void amotas_proc_msg(wsfMsgHdr_t *pMsg)
{
    if (pMsg->event == DM_CONN_OPEN_IND)
    {
        amotas_conn_open((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == DM_CONN_UPDATE_IND)
    {
        amotas_conn_update((dmEvt_t *) pMsg);
    }
    else if (pMsg->event == amotasCb.resetTimer.msg.event)
    {
        amotas_reset_timer_expired(pMsg);
    }
    else if (pMsg->event == amotasCb.disconnectTimer.msg.event)
    {
        amotas_disconnect_timer_expired(pMsg);
    }
}
