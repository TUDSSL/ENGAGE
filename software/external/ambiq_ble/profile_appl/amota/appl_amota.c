//*****************************************************************************
//
//  appl_amota.c
//! @file
//!
//! @brief Appl notification center service client.
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

/**
 *  \file appl_amota.c
 *
 *  This file contains the AMOTA application.
  *  Sample applications detailed below:
 *      a. The Sensor, as defined by the Sepcification plays the GAP Peripheral
 *         role.
 *      b. The Sensor application has following sevice records:
 *           - GAP
 *           - GATT
 *           - Battery
 *           - Device Information and
 *           - AMOTA
 *         [NOTE]: Please see gatt_db.c for more details of the record.
 *      c. appl_manage_transfer routine takes care of handling peer
 *         configuration. This handling would be needed:
 *           - When Peer Configures Measurement Transfer by writting to the
 *             Characteristic Client Configuration of AMOTA Tx.
 *           - Subsequent reconnection with bonded device that had already
 *             configured the device for transfer. Please note it is mandatory
 *             for GATT Servers to remember the configurations of bonded GATT
 *             clients.
 *         In order to ensure the above mentioned configurations are correctly
 *         handled, the routine, appl_manage_transfer, is therefore called from:
 *           - gatt_db_amotas_handler and
 *           - appl_amotas_connect
 *         [NOTE]: If link does not have the needed secruity for the service,
 *         transfer will not be initiated.
 */



/* --------------------------------------------- Header File Inclusion */
#include "appl.h"

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "am_bootloader.h"
#include "amota_profile_config.h"
#include "am_multi_boot.h"
#include "amota_crc32.h"

#ifdef AMOTAS

static am_multiboot_flash_info_t *g_pFlash = &g_intFlash;

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
#if (AMOTAS_SUPPORT_EXT_FLASH == 1)
#error "External flash based OTA not supported in Apollo3"
#endif
#endif

#define BT_MODULE_BIT_MASK_AMOTA        0x00010000
#define BT_MODULE_ID_AMOTA              (BT_MODULE_PAGE_2 | BT_MODULE_BIT_MASK_AMOTA)
#define AMOTA_ERR(...)                  BT_debug_error(BT_MODULE_BIT_MASK_AMOTA, __VA_ARGS__)
#define AMOTA_TRC(...)                  BT_debug_trace(BT_MODULE_BIT_MASK_AMOTA, __VA_ARGS__)

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

#define DISCONNECT_TIMEOUT_DEFAULT              1   // 1 second
#define RESET_TIMEOUT_DEFAULT                   1   // 1 second

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
    BOOLEAN                 txReady;              // TRUE if ready to send notifications
    AmotasCfg_t             cfg;                  // configurable parameters
    eAmotaState             state;
    amotaHeaderInfo_t       fwHeader;
    amotaPacket_t           pkt;
    amotasNewFwFlashInfo_t  newFwFlashInfo;
    BT_timer_handle         resetTimer_handle;    // reset timer after OTA update done
    BT_timer_handle         disconnectTimer_handle;    // disconnect timer after OTA update done
}
amotasCb;

/* --------------------------------------------- Static Global Variables */
static GATT_DB_HANDLE   amotas_gatt_db_handle;
static ATT_ATTR_HANDLE  amotas_tx_char_handle;
static APPL_HANDLE      amotas_appl_handle;
/* --------------------------------------------- Functions */

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

void amotas_reset_timer_expired(void *data, UINT16 datalen)
{
    AMOTA_TRC("amotas_reset_board\n");

    // reset reset timer handle here
    amotasCb.resetTimer_handle = BT_TIMER_HANDLE_INIT_VAL;

    am_util_delay_ms(10);
#if AM_APOLLO3_RESET
    am_hal_reset_control(AM_HAL_RESET_CONTROL_SWPOI, 0);
#else // AM_APOLLO3_RESET
    am_hal_reset_poi();
#endif // AM_APOLLO3_RESET

}

void
amotas_disconnect_timer_expired(void *data, UINT16 datalen)
{
    API_RESULT retval;
    UINT16 appl_ble_connection_handle;

    AMOTA_TRC("Disconnect BLE connection\n");

    appl_ble_connection_handle = APPL_GET_CONNECTION_HANDLE(amotas_appl_handle);
    BT_hci_disconnect (appl_ble_connection_handle, HC_OTHER_END_TERMINATED_USER);

    // reset disconnect timer handle here
    amotasCb.disconnectTimer_handle = BT_TIMER_HANDLE_INIT_VAL;

    //
    // Delay here to let disconnect req go through
    // the RF before we reboot.
    //

    if ( BT_TIMER_HANDLE_INIT_VAL != amotasCb.resetTimer_handle )
    {
        retval = BT_stop_timer (amotasCb.resetTimer_handle);
        AMOTA_TRC (
        "[AMOTA]: Stopping Timer with result 0x%04X, timer handle %p\n",
        retval, amotasCb.resetTimer_handle);
        amotasCb.resetTimer_handle = BT_TIMER_HANDLE_INIT_VAL;
    }

    retval = BT_start_timer
             (
                 &amotasCb.resetTimer_handle,
                 RESET_TIMEOUT_DEFAULT,
                 amotas_reset_timer_expired,
                 NULL,
                 0
             );

    if (API_SUCCESS != retval)
    {
        // we have to reset from here directly.
        amotas_reset_timer_expired(NULL, 0);
    }
}


void appl_amotas_init(void)
{
    memset(&amotasCb, 0, sizeof(amotasCb));
    amotasCb.txReady = false;
    amotasCb.state = AMOTA_STATE_INIT;
    amotasCb.resetTimer_handle = BT_TIMER_HANDLE_INIT_VAL;
    amotasCb.disconnectTimer_handle = BT_TIMER_HANDLE_INIT_VAL;

    amotasCb.pkt.offset = 0;
    amotasCb.pkt.len = 0;
    amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
}

void appl_amotas_connect(APPL_HANDLE  * appl_handle)
{
    ATT_VALUE         value;
    UINT16            cli_cnfg;

    cli_cnfg = 0;

    // save application handle
    amotas_appl_handle              = *appl_handle;

    amotas_gatt_db_handle.device_id = APPL_GET_DEVICE_HANDLE((*appl_handle));

    amotas_gatt_db_handle.char_id = GATT_CHAR_AMOTA_TX;
    amotas_gatt_db_handle.service_id = GATT_SER_AMOTA_INST;

    BT_gatt_db_get_char_val_hndl(&amotas_gatt_db_handle, &amotas_tx_char_handle);
    BT_gatt_db_get_char_cli_cnfg(&amotas_gatt_db_handle, &value);
    BT_UNPACK_LE_2_BYTE (&cli_cnfg, value.val);

    AMOTA_TRC (
    "[APPL]: Fetched Client Configuration (0x%04X) for Device (0x%02X)\n",
    cli_cnfg, APPL_GET_DEVICE_HANDLE((*appl_handle)));

    appl_manage_trasnfer(amotas_gatt_db_handle, cli_cnfg);
}

void appl_manage_trasnfer(GATT_DB_HANDLE handle, UINT16 config)
{
    API_RESULT     retval;

    UCHAR          security, ekey_size;

    AMOTA_TRC("[AMOTA]: appl_manage_trasnfer+ \n");
    /* Get required security for service */
    /* Get required security level */
    BT_gatt_db_get_service_security (&handle, &security);
    /* Get required encryption key size */
    BT_gatt_db_get_service_enc_key_size (&handle, &ekey_size);

    /* Verify if security requirements are available with the link */
    retval = appl_smp_assert_security
             (
                 &handle.device_id,
                 security,
                 ekey_size
             );

    /* Security requirements satisfied? */
    if ( API_SUCCESS != retval )
    {
        /* No. Return */
        return;
    }

    if ( GATT_CLI_CNFG_NOTIFICATION == config )
    {
        amotasCb.txReady = true;
        AMOTA_TRC("[AMOTA]: notify registered \n");
    }
    else if ( GATT_CLI_CNFG_DEFAULT == config )
    {
        amotasCb.txReady = false;
        AMOTA_TRC("[AMOTA]: notify unregistered \n");
    }
    else
    {
        /* Incorrect Configuration */
    }
}


void amota_mtu_update(APPL_HANDLE    * appl_handle, UINT16 t_mtu)
{
    UINT16 mtu = 0;

    BT_att_access_mtu(&APPL_GET_ATT_INSTANCE(*appl_handle),
        &mtu);
    AMOTA_TRC("appl_handle 0x%x t_mtu = %d %d\n", *appl_handle, t_mtu, mtu);
}


//*****************************************************************************
//
// Send Notification to Client
//
//*****************************************************************************
void appl_amotas_send_data(UCHAR *data, UINT16 len)
{
    ATT_HANDLE_VALUE_PAIR hndl_val_param;
    API_RESULT retval;

    //AMOTA_TRC("Sending Tx On Handle 0x%04X\n", amotas_tx_char_handle);

    hndl_val_param.handle =  amotas_tx_char_handle;
    hndl_val_param.value.val = data;
    hndl_val_param.value.len = len;

    retval = BT_att_send_hndl_val_ntf
             (
                 &APPL_GET_ATT_INSTANCE(amotas_appl_handle),
                 &hndl_val_param
              );

    if ( API_SUCCESS != retval )
    {
        AMOTA_ERR( "[** ERR **]: Failed to send measurement, reason 0x%04X", retval);
    }
}

//*****************************************************************************
//
// Send Reply to Client
//
//*****************************************************************************
static void
amotas_reply_to_client(eAmotaCommand cmd, eAmotaStatus status, uint8_t *data, uint16_t len)
{
    uint8_t buf[20] = {0};
    buf[0] = (len + 1) & 0xff;
    buf[1] = ((len + 1) >> 8) & 0xff;
    buf[2] = cmd;
    buf[3] = status;
    if ( len > 0 )
    {
        memcpy(buf + 4, data, len);
    }
    appl_amotas_send_data(buf, len + 4);
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
        if ( am_multiboot_get_main_image_info(&ui32CurLinkAddr, &ui32CurLen) == false )
        {
            return false;
        }
        // Length in flash page size multiples
        ui32CurLen = (ui32CurLen + (AM_HAL_FLASH_PAGE_SIZE - 1)) & ~(AM_HAL_FLASH_PAGE_SIZE - 1) ;
        // Make sure the OTA area does not overwrite the main image
        if ( !((storeAddr + maxSize) <= ui32CurLinkAddr) &&
             !(storeAddr >= (ui32CurLinkAddr + ui32CurLen)) )
        {
            AMOTA_TRC("OTA memory overlaps with main firmware\n");
            return false;
        }
#endif
        // Check to make sure the incoming image will fit in the space allocated for OTA
        if ( amotasCb.fwHeader.fwLength > maxSize )
        {
            AMOTA_TRC("not enough OTA space allocated = %d bytes, Desired = %d bytes\n",
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

        if ( g_pFlash->flash_read_page &&
             g_pFlash->flash_write_page &&
             g_pFlash->flash_erase_sector &&
             (amotasCb.fwHeader.fwLength <= AMOTA_EXT_FLASH_OTA_MAX_SIZE) )
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
    if ( bResult == true )
    {
        //
        // Initialize the flash device.
        //
        if ( FLASH_OPERATE(g_pFlash, flash_init) == 0 )
        {
            if ( FLASH_OPERATE(g_pFlash, flash_enable) != 0 )
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
            AMOTA_TRC("flash write verify failed. address 0x%x. length %d\n", flashAddr, len);
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
    if ( ((uint32_t)amotasCb.newFwFlashInfo.addr > addr) ||
         (addr & (g_pFlash->flashPageSize - 1)) )
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
        if ( ui16Bytes2write > ui16BytesRemaining )
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
        if ( lastPktFlag || (amotasFlash.bufferIndex == g_pFlash->flashPageSize) )
        {
            ui32TargetAddress = (addr + ui8PageCount*g_pFlash->flashPageSize);

            // Always write whole pages
            if ( (g_pFlash->flash_write_page(ui32TargetAddress, (uint32_t *)amotasFlash.writeBuffer, g_pFlash->flashPageSize) != 0)
                || (verify_flash_content(ui32TargetAddress, (uint32_t *)amotasFlash.writeBuffer, amotasFlash.bufferIndex, g_pFlash) != 0) )
            {
                bResult = false;
                break;
            }
            AMOTA_TRC("Flash write succeeded to address 0x%x. length %d\n", ui32TargetAddress, amotasFlash.bufferIndex);

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
    if ( am_hal_flash_program_main(AM_HAL_FLASH_PROGRAM_KEY, (uint32_t *)buf, (uint32_t *)addr, len / 4) != 0 )
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
static BOOLEAN
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
    uint32_t *pOtaDesc = (uint32_t *)(OTA_POINTER_LOCATION & ~(AM_HAL_FLASH_PAGE_SIZE - 1));
    uint8_t  magic = *((uint8_t *)(amotasCb.newFwFlashInfo.addr + 3));

    // Initialize OTA descriptor - This should ideally be initiated through a separate command
    // to facilitate multiple image upgrade in a single reboot
    // Will need change in the AMOTA app to do so
    am_hal_ota_init(AM_HAL_FLASH_PROGRAM_KEY, pOtaDesc);
    // Set OTAPOINTER
    am_hal_ota_add(AM_HAL_FLASH_PROGRAM_KEY, magic, (uint32_t *)amotasCb.newFwFlashInfo.addr);
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
    if ( amotasCb.fwHeader.storageType == AMOTA_FW_STORAGE_EXTERNAL )
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
    if ( amotasCb.fwHeader.fwDataType != 0 )
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
    BOOLEAN resumeTransfer = false;

    switch(cmd)
    {
        case AMOTA_CMD_FW_HEADER:
            if ( len < AMOTA_FW_HEADER_SIZE )
            {
                status = AMOTA_STATUS_INVALID_HEADER_INFO;
                amotas_reply_to_client(cmd, status, NULL, 0);
                break;
            }

            if ( amotasCb.state == AMOTA_STATE_GETTING_FW )
            {
                BT_UNPACK_LE_4_BYTE(&ver, buf + 32);
                BT_UNPACK_LE_4_BYTE(&fwCrc, buf + 12);

                if ( ver == amotasCb.fwHeader.version && fwCrc == amotasCb.fwHeader.fwCrc )
                {
                    resumeTransfer = true;
                }
            }

            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.encrypted, buf);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.fwStartAddr, buf + 4);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.fwLength, buf + 8);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.fwCrc, buf + 12);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.secInfoLen, buf + 16);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.version, buf + 32);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.fwDataType, buf + 36);
            BT_UNPACK_LE_4_BYTE(&amotasCb.fwHeader.storageType, buf + 40);

            if ( resumeTransfer )
            {
                AMOTA_TRC("OTA process start from offset = 0x%x\n", amotasCb.newFwFlashInfo.offset);
                AMOTA_TRC("beginning of flash addr = 0x%x\n", amotasCb.newFwFlashInfo.addr);
            }
            else
            {
                AMOTA_TRC("OTA process start from beginning\n");
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
            AMOTA_TRC("============= fw header start ===============\n");
            AMOTA_TRC("encrypted = 0x%x\n", amotasCb.fwHeader.encrypted);
            AMOTA_TRC("version = 0x%x\n", amotasCb.fwHeader.version);
            AMOTA_TRC("fwLength = 0x%x\n", amotasCb.fwHeader.fwLength);
            AMOTA_TRC("fwCrc = 0x%x\n", amotasCb.fwHeader.fwCrc);
            AMOTA_TRC("fwStartAddr = 0x%x\n", amotasCb.fwHeader.fwStartAddr);
            AMOTA_TRC("fwDataType = 0x%x\n", amotasCb.fwHeader.fwDataType);
            AMOTA_TRC("storageType = 0x%x\n", amotasCb.fwHeader.storageType);
            AMOTA_TRC("============= fw header end ===============\n");
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
            if ( amotas_verify_firmware_crc() )
            {
                AMOTA_TRC("crc verify success\n");

                amotas_reply_to_client(cmd, AMOTA_STATUS_SUCCESS, NULL, 0);

                //
                // Update flash flag page here
                //
                amotas_update_ota();
            }
            else
            {
                AMOTA_TRC("crc verify failed\n");
                amotas_reply_to_client(cmd, AMOTA_STATUS_CRC_ERROR, NULL, 0);
            }
            FLASH_OPERATE(g_pFlash, flash_deinit);
            amotasCb.state = AMOTA_STATE_INIT;
            g_pFlash = &g_intFlash;
        break;

        case AMOTA_CMD_FW_RESET:
        {
            API_RESULT retval = API_SUCCESS;
            AMOTA_TRC("Apollo will reset itself in 500ms.\n");
            amotas_reply_to_client(cmd, AMOTA_STATUS_SUCCESS, NULL, 0);

            //
            // Delay here to let packet go through the RF before we reset
            //
            if ( BT_TIMER_HANDLE_INIT_VAL != amotasCb.disconnectTimer_handle )
            {
                retval = BT_stop_timer (amotasCb.disconnectTimer_handle);
                AMOTA_TRC (
                "[AMOTA]: Stopping Timer with result 0x%04X, timer handle %p\n",
                retval, amotasCb.disconnectTimer_handle);
                amotasCb.disconnectTimer_handle = BT_TIMER_HANDLE_INIT_VAL;
            }

            retval = BT_start_timer
                 (
                     &amotasCb.disconnectTimer_handle,
                     DISCONNECT_TIMEOUT_DEFAULT,
                     amotas_disconnect_timer_expired,
                     NULL,
                     0
                 );
            AMOTA_TRC (
            "[AMOTA]: Started Timer with result 0x%04X, timer handle %d\n",
            retval, amotasCb.disconnectTimer_handle);
        }
        break;

        default:
        break;
    }
}

void appl_amotas_disconnect (void)
{
    // we don't need disconnect timer anymore
    if ( BT_TIMER_HANDLE_INIT_VAL != amotasCb.disconnectTimer_handle )
    {
        BT_stop_timer (amotasCb.disconnectTimer_handle);
        amotasCb.disconnectTimer_handle = BT_TIMER_HANDLE_INIT_VAL;
    }


    amotasCb.resetTimer_handle = BT_TIMER_HANDLE_INIT_VAL;

    amotasCb.pkt.offset = 0;
    amotasCb.pkt.len = 0;
    amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
}

API_RESULT appl_amotas_write_cback
           (
                GATT_DB_HANDLE  * handle,
                ATT_VALUE       * value
           )
{
    API_RESULT retval = API_SUCCESS;
    uint8_t dataIdx = 0;
    uint32_t calDataCrc = 0;

#ifdef AMOTA_DEBUG_ON
    uint16_t i = 0;
    AMOTA_TRC("============= data arrived start ===============\n");
    for (i = 0; i < value->len; i++)
    {
        AMOTA_TRC("%x\t", value->val[i]);
    }
    AMOTA_TRC("============= data arrived end ===============\n");
#endif

    if ( amotasCb.pkt.offset == 0 && value->len < AMOTA_HEADER_SIZE_IN_PKT )
    {
        AMOTA_TRC("Invalid packet!!!\n");
        amotas_reply_to_client(AMOTA_CMD_FW_HEADER, AMOTA_STATUS_INVALID_PKT_LENGTH, NULL, 0);
        return retval;
    }

    // new packet
    if ( amotasCb.pkt.offset == 0 )
    {
        BT_UNPACK_LE_2_BYTE(&amotasCb.pkt.len, value->val);
        amotasCb.pkt.type = (eAmotaCommand) value->val[2];
        dataIdx = 3;
#ifdef AMOTA_DEBUG_ON
        AMOTA_TRC("pkt.len = 0x%x\n", amotasCb.pkt.len);
        AMOTA_TRC("pkt.type = 0x%x\n", amotasCb.pkt.type);
#endif
        if (dataIdx > amotasCb.pkt.len)
        {
            AMOTA_TRC("packet length is wrong since it's smaller than 3!");
            return retval;
        }
    }

    // make sure we have enough space for new data
    if ( amotasCb.pkt.offset + value->len - dataIdx > AMOTA_PACKET_SIZE )
    {
        AMOTA_TRC("not enough buffer size!!!\n");
        amotas_reply_to_client(amotasCb.pkt.type, AMOTA_STATUS_INSUFFICIENT_BUFFER, NULL, 0);
        return retval;
    }

    // copy new data into buffer and also save crc into it if it's the last frame in a packet
    // 4 bytes crc is included in pkt length
    memcpy(amotasCb.pkt.data + amotasCb.pkt.offset, value->val + dataIdx, value->len - dataIdx);
    amotasCb.pkt.offset += (value->len - dataIdx);

    // whole packet received
    if ( amotasCb.pkt.offset >= amotasCb.pkt.len )
    {
        uint32_t peerCrc = 0;
        // check CRC
        BT_UNPACK_LE_4_BYTE(&peerCrc, amotasCb.pkt.data + amotasCb.pkt.len - AMOTA_CRC_SIZE_IN_PKT);
        calDataCrc = AmotaCrc32(0xFFFFFFFFU, amotasCb.pkt.len - AMOTA_CRC_SIZE_IN_PKT, amotasCb.pkt.data);
#ifdef AMOTA_DEBUG_ON
        AMOTA_TRC("calDataCrc = 0x%x\n", calDataCrc);
        AMOTA_TRC("peerCrc = 0x%x\n", peerCrc);
#endif

        if ( peerCrc != calDataCrc )
        {
            amotas_reply_to_client(amotasCb.pkt.type, AMOTA_STATUS_CRC_ERROR, NULL, 0);

            // clear pkt
            amotasCb.pkt.offset = 0;
            amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
            amotasCb.pkt.len = 0;

            return retval;
        }

        amotas_packet_handler(amotasCb.pkt.type, amotasCb.pkt.len - AMOTA_CRC_SIZE_IN_PKT, amotasCb.pkt.data);
        // clear pkt after handled
        amotasCb.pkt.offset = 0;
        amotasCb.pkt.type = AMOTA_CMD_UNKNOWN;
        amotasCb.pkt.len = 0;
    }

    return retval;
}

#endif /* AMOTAS */

