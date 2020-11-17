/*
 * gby-v1-0.h
 *
 *  Created on: Jan 11, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef PLATFORMS_GBY_V1_0_H_
#define PLATFORMS_GBY_V1_0_H_

#include "am_mcu_apollo.h"

#define GPIO_PIN(pin) CONCAT(AM_HAL_PIN_, pin)
#define GPIO_IOM_M(iom, pin) CONCAT(CONCAT(GPIO_PIN(pin), _M), iom)
#define GPIO_MSPI(pin) CONCAT(GPIO_PIN(pin), _MSPI)

#define GPIO_PIN_IO(a) CONCAT(GPIO_PIN(a), _GPIO)
#define GPIO_PIN_PSOURCE(a) CONCAT(GPIO_PIN(a), _PSOURCE)

#define GPIO_IOM_SDAWIR3(iom, pin) CONCAT(GPIO_IOM_M(iom, pin), SDAWIR3)
#define GPIO_IOM_SCK(iom, pin) CONCAT(GPIO_IOM_M(iom, pin), SCK)
#define GPIO_IOM_SCL(iom, pin) CONCAT(GPIO_IOM_M(iom, pin), SCL)

#define GPIO_MSPI_D0(pin) CONCAT(GPIO_MSPI(pin), 0)
#define GPIO_MSPI_D1(pin) CONCAT(GPIO_MSPI(pin), 1)
#define GPIO_MSPI_D8(pin) CONCAT(GPIO_MSPI(pin), 8)

#define GPIO_NCE(pin) CONCAT(CONCAT(GPIO_PIN(pin), _NCE), pin)

#define SWD_PIN 41

//
// Start of display pins
//
#define DISPLAY_SPI_CS_CHANNEL 0
#define DISPLAY_SPI_IOM 1

#define DISPLAY_CS_PIN 35
extern const am_hal_gpio_pincfg_t DisplayPinConfigCS;

#define DISPLAY_CLK_PIN 8
extern const am_hal_gpio_pincfg_t DisplayPinConfigCLK;

#define DISPLAY_MOSI_PIN 9
extern const am_hal_gpio_pincfg_t DisplayPinConfigMOSI;

#define DISPLAY_DISP_PIN 10
extern const am_hal_gpio_pincfg_t DisplayPinConfigDISP;

#define DISPLAY_EXTCOM_PIN 7
extern const am_hal_gpio_pincfg_t DisplayPinConfigEXTCOM;

#define DISPLAY_VCC_PIN 36
extern const am_hal_gpio_pincfg_t DisplayPinConfigVCC;

//
// End of display
//

//
// Start of cartridge pins
//

#define CARTRIDGE_I2C_IOM_ADDRESS 4
#define CARTRIDGE_I2C_IOM_DATA 5

const typedef enum {
  IomAddress = CARTRIDGE_I2C_IOM_ADDRESS,
  IomData = CARTRIDGE_I2C_IOM_DATA
} CartridgeIomModules;

#define CARTRIDGE_IOM4_SCL_PIN 39
extern const am_hal_gpio_pincfg_t CartridgePinConfigIom4SCL;

#define CARTRIDGE_IOM4_SDA_PIN 40
extern const am_hal_gpio_pincfg_t CartridgePinConfigIom4SDA;

#define CARTRIDGE_IOM5_SCL_PIN 48
extern const am_hal_gpio_pincfg_t CartridgePinConfigIom5SCL;

#define CARTRIDGE_IOM5_SDA_PIN 49
extern const am_hal_gpio_pincfg_t CartridgePinConfigIom5SDA;

#define CARTRIDGE_nRST_PIN 0
extern const am_hal_gpio_pincfg_t CartridgePinConfigRSTn;

#define CARTRIDGE_nINT0_PIN 37
extern const am_hal_gpio_pincfg_t CartridgePinConfigINT0n;

#define CARTRIDGE_nINT1_PIN 38
extern const am_hal_gpio_pincfg_t CartridgePinConfigINT1n;

#define CARTRIDGE_VCC_PIN 3
extern const am_hal_gpio_pincfg_t CartridgePinConfigVCC;

//
// End cartridge interface.
//

//
// Start FRAM interface
//

#define PIN_FRAM_WP 27
extern const am_hal_gpio_pincfg_t g_PIN_FRAM_WP;

#define PIN_FRAM_HOLD 23
extern const am_hal_gpio_pincfg_t g_PIN_FRAM_HOLD;

#define AM_BSP_GPIO_MSPI_CE0 19
extern const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_CE0;

#define AM_BSP_GPIO_MSPI_D0 22
extern const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_D0;

#define AM_BSP_GPIO_MSPI_D1 26
extern const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_D1;

#define AM_BSP_GPIO_MSPI_SCK 24
extern const am_hal_gpio_pincfg_t g_AM_BSP_GPIO_MSPI_SCK;

//
// End FRAM interface
//

//
// Start buttons
//

#define BUTTON_A 11
#define BUTTON_B 34
#define BUTTON_UP 15
#define BUTTON_DOWN 12
#define BUTTON_RIGHT 33
#define BUTTON_LEFT 13
#define BUTTON_START 17
#define BUTTON_SELECT 16
//
// End buttons
//

//
// ADC pin to the storage capacitor
//
#define ADC_VBAT_JIT 29

// Debug pins
#define DEBUG_TX 42
#define DEBUG_RX 43
#define DEBUG_RTS 44
#define DEBUG_CTS 45

#endif /* PLATFORMS_GBY_V1_0_H_ */
