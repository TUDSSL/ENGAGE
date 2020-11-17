/*
 * gby-v1-0.c
 *
 *  Created on: Jan 11, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "platform.h"

//
// Start of display pins
//
const am_hal_gpio_pincfg_t DisplayPinConfigCS = {
    .uFuncSel = GPIO_NCE(DISPLAY_CS_PIN),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL,
    .eGPInput = AM_HAL_GPIO_PIN_INPUT_NONE,
    .eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI,
    .uIOMnum = DISPLAY_SPI_IOM,
    .uNCE = 0,
    .eCEpol = AM_HAL_GPIO_PIN_CEPOL_ACTIVEHIGH};

const am_hal_gpio_pincfg_t DisplayPinConfigCLK = {
    .uFuncSel = GPIO_IOM_SCK(DISPLAY_SPI_IOM, DISPLAY_CLK_PIN),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .uIOMnum = DISPLAY_SPI_IOM};

const am_hal_gpio_pincfg_t DisplayPinConfigMOSI = {
    .uFuncSel = GPIO_IOM_SDAWIR3(DISPLAY_SPI_IOM, DISPLAY_MOSI_PIN),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .uIOMnum = DISPLAY_SPI_IOM};

const am_hal_gpio_pincfg_t DisplayPinConfigVCC = {
    .uFuncSel = GPIO_PIN_PSOURCE(DISPLAY_VCC_PIN),
    .ePowerSw = AM_HAL_GPIO_PIN_POWERSW_VDD,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL};

//
// End of display
//

//
// Start of cartridge pins
//

const am_hal_gpio_pincfg_t CartridgePinConfigIom4SCL = {
    .uFuncSel = GPIO_IOM_SCL(CARTRIDGE_I2C_IOM_ADDRESS, CARTRIDGE_IOM4_SCL_PIN),
    .ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K,
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN,
    .uIOMnum = CARTRIDGE_I2C_IOM_ADDRESS};

const am_hal_gpio_pincfg_t CartridgePinConfigIom4SDA = {
    .uFuncSel =
        GPIO_IOM_SDAWIR3(CARTRIDGE_I2C_IOM_ADDRESS, CARTRIDGE_IOM4_SDA_PIN),
    .ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K,
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN,
    .uIOMnum = CARTRIDGE_I2C_IOM_ADDRESS};

const am_hal_gpio_pincfg_t CartridgePinConfigIom5SCL = {
    .uFuncSel = GPIO_IOM_SCL(CARTRIDGE_I2C_IOM_DATA, CARTRIDGE_IOM5_SCL_PIN),
    .ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K,
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN,
    .uIOMnum = CARTRIDGE_I2C_IOM_DATA};

const am_hal_gpio_pincfg_t CartridgePinConfigIom5SDA = {
    .uFuncSel =
        GPIO_IOM_SDAWIR3(CARTRIDGE_I2C_IOM_DATA, CARTRIDGE_IOM5_SDA_PIN),
    .ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K,
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN,
    .uIOMnum = CARTRIDGE_I2C_IOM_DATA};

const am_hal_gpio_pincfg_t CartridgePinConfigRSTn = {
    .uFuncSel = GPIO_PIN_IO(CARTRIDGE_nRST_PIN),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .ePullup = AM_HAL_GPIO_PIN_PULLUP_WEAK,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN};

const am_hal_gpio_pincfg_t CartridgePinConfigVCC = {
    .uFuncSel = GPIO_PIN_PSOURCE(CARTRIDGE_VCC_PIN),
    .ePowerSw = AM_HAL_GPIO_PIN_POWERSW_VDD,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL};

//
// End cartridge interface.
//

//
// Start FRAM interface
//
const am_hal_gpio_pincfg_t g_PIN_FRAM_WP = {
    .uFuncSel = GPIO_PIN_IO(PIN_FRAM_WP),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL};

const am_hal_gpio_pincfg_t g_PIN_FRAM_HOLD = {
    .uFuncSel = GPIO_PIN_IO(PIN_FRAM_HOLD),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL};

const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_CE0 = {
    .uFuncSel = GPIO_NCE(AM_BSP_GPIO_MSPI_CE0),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL,
    .eGPInput = AM_HAL_GPIO_PIN_INPUT_NONE,
    .eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI,
    .uIOMnum = IOMNUM_MSPI,
    .uNCE = 0,
    .eCEpol = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW};

const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_D0 = {
    .uFuncSel = GPIO_MSPI_D0(AM_BSP_GPIO_MSPI_D0),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI,
    .uIOMnum = IOMNUM_MSPI};

const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_D1 = {
    .uFuncSel = GPIO_MSPI_D1(AM_BSP_GPIO_MSPI_D1),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI,
    .uIOMnum = IOMNUM_MSPI};

const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_SCK = {
    .uFuncSel = GPIO_MSPI_D8(AM_BSP_GPIO_MSPI_SCK),
    .eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA,
    .eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI,
    .uIOMnum = IOMNUM_MSPI};

//
// End FRAM interface
//
