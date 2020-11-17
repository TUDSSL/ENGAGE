/*
 * cartridge.c
 *
 *  Created on: Jan 11, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "cartridge.h"

#include "am_util_delay.h"
#include "platform.h"

CHECKPOINT_EXCLUDE_DATA
am_hal_iom_config_t CartridgeConfigIOM = {
    .eInterfaceMode = AM_HAL_IOM_I2C_MODE,
    .ui32ClockFreq = AM_HAL_IOM_400KHZ,
};

void i2cWrite(CartridgeIomModules module, uint8_t deviceAddress,
              const uint8_t data);
void i2cRead(CartridgeIomModules module, uint8_t deviceAddress, uint8_t* data);

CHECKPOINT_EXCLUDE_BSS
void* cartridgeAddressI2cHandle;
CHECKPOINT_EXCLUDE_BSS
void* cartridgeDataI2cHandle;

CHECKPOINT_EXCLUDE_BSS
CartridgeControlSignals cartridgeControlState;

void cartridgeInterfaceConfig() {
  am_hal_gpio_pinconfig(CARTRIDGE_IOM4_SCL_PIN, CartridgePinConfigIom4SCL);
  am_hal_gpio_pinconfig(CARTRIDGE_IOM4_SDA_PIN, CartridgePinConfigIom4SDA);
  am_hal_gpio_pinconfig(CARTRIDGE_IOM5_SCL_PIN, CartridgePinConfigIom5SCL);
  am_hal_gpio_pinconfig(CARTRIDGE_IOM5_SDA_PIN, CartridgePinConfigIom5SDA);

  am_hal_gpio_pinconfig(CARTRIDGE_nRST_PIN, g_AM_HAL_GPIO_OUTPUT);
  am_hal_gpio_state_write(CARTRIDGE_nRST_PIN, AM_HAL_GPIO_OUTPUT_SET);

  am_hal_gpio_pinconfig(CARTRIDGE_nINT0_PIN, g_AM_HAL_GPIO_INPUT_PULLUP);
  am_hal_gpio_pinconfig(CARTRIDGE_nINT1_PIN, g_AM_HAL_GPIO_INPUT_PULLUP);

  am_hal_gpio_pinconfig(CARTRIDGE_VCC_PIN, CartridgePinConfigVCC);
  am_hal_gpio_state_write(CARTRIDGE_VCC_PIN, AM_HAL_GPIO_OUTPUT_SET);

  am_hal_iom_initialize(IomAddress, &cartridgeAddressI2cHandle);
  am_hal_iom_power_ctrl(cartridgeAddressI2cHandle, AM_HAL_SYSCTRL_WAKE, false);
  am_hal_iom_configure(cartridgeAddressI2cHandle, &CartridgeConfigIOM);
  am_hal_iom_enable(cartridgeAddressI2cHandle);

  am_hal_iom_initialize(IomData, &cartridgeDataI2cHandle);
  am_hal_iom_power_ctrl(cartridgeDataI2cHandle, AM_HAL_SYSCTRL_WAKE, false);
  am_hal_iom_configure(cartridgeDataI2cHandle, &CartridgeConfigIOM);
  am_hal_iom_enable(cartridgeDataI2cHandle);

  am_util_delay_ms(10);  // delay a bit todo finetune this.

  i2cWrite(IomData, SX150X_RegDirA, 0xFF);
  i2cWrite(IomData, SX150X_RegDirB, 0x00);
  i2cWrite(IomAddress, SX150X_RegDirA, 0x00);
  i2cWrite(IomAddress, SX150X_RegDirB, 0x00);

  cartridgeControlState.CartridgeRSTn = true;
  cartridgeControlState.CartridgeWRn = true;
  cartridgeControlState.CartridgeRDn = true;
  cartridgeControlState.CartridgeCS = true;
  cartridgeInterfaceWriteControl(cartridgeControlState);
}

void cartridgeInterfaceWriteControl(CartridgeControlSignals state) {
  i2cWrite(IomData, SX150X_RegDataB, state.asUint8_t);
  cartridgeControlState = state;
}

void cartridgeInterfaceWriteData(uint16_t address, uint8_t data) {
  uint16_t address_rev = __RBIT(address) >> 16;
  i2cWrite(IomAddress, SX150X_RegDataA, address_rev);
  i2cWrite(IomAddress, SX150X_RegDataB, address_rev >> 8);

  uint8_t dataRaw = __RBIT(data) >> 24;
  i2cWrite(IomData, SX150X_RegDataA, dataRaw);
  i2cWrite(IomData, SX150X_RegDirA, 0x00);  // set data bus to output

  CartridgeControlSignals signalCpy = cartridgeControlState;
  signalCpy.CartridgeCLK = true;
  signalCpy.CartridgeCS = false;
  signalCpy.CartridgeWRn = false;  // write operation
  signalCpy.CartridgeRDn = true;
  cartridgeInterfaceWriteControl(signalCpy);

  signalCpy.CartridgeCLK = false;
  signalCpy.CartridgeCS = true;
  signalCpy.CartridgeWRn = true;
  cartridgeInterfaceWriteControl(signalCpy);

  i2cWrite(IomData, SX150X_RegDirA, 0xff);  // set data bus to input (default)
}

void cartridgeInterfaceReadData(uint16_t address, uint8_t* data) {
  uint16_t address_rev = __RBIT(address) >> 16;
  i2cWrite(IomAddress, SX150X_RegDataA, address_rev);
  i2cWrite(IomAddress, SX150X_RegDataB, address_rev >> 8);

  CartridgeControlSignals signalCpy = cartridgeControlState;
  signalCpy.CartridgeCLK = true;
  signalCpy.CartridgeCS = false;
  signalCpy.CartridgeRDn = false;  // read operation
  signalCpy.CartridgeWRn = true;
  cartridgeInterfaceWriteControl(signalCpy);

  uint8_t dataRaw;
  i2cRead(IomData, SX150X_RegDataA, &dataRaw);
  *data = __RBIT(dataRaw) >> 24;

  signalCpy.CartridgeCLK = false;
  signalCpy.CartridgeCS = true;
  signalCpy.CartridgeRDn = true;
  cartridgeInterfaceWriteControl(signalCpy);
}

void i2cWrite(CartridgeIomModules module, uint8_t deviceAddress,
              const uint8_t data) {
  am_hal_iom_transfer_t Transaction = {};

  Transaction.ui32InstrLen = 1;
  Transaction.ui32Instr = deviceAddress;
  Transaction.eDirection = AM_HAL_IOM_TX;
  Transaction.ui32NumBytes = 1;
  Transaction.pui32TxBuffer = (uint32_t*)&data;
  Transaction.uPeerInfo.ui32I2CDevAddr = SX150X_Address;

  if (module == IomAddress) {
    am_hal_iom_blocking_transfer(cartridgeAddressI2cHandle, &Transaction);
  } else if (module == IomData) {
    am_hal_iom_blocking_transfer(cartridgeDataI2cHandle, &Transaction);
  }
}

void i2cRead(CartridgeIomModules module, uint8_t deviceAddress, uint8_t* data) {
  am_hal_iom_transfer_t Transaction = {};

  Transaction.ui32InstrLen = 1;
  Transaction.ui32Instr = deviceAddress;
  Transaction.eDirection = AM_HAL_IOM_RX;
  Transaction.ui32NumBytes = 1;
  Transaction.pui32RxBuffer = (uint32_t*)data;
  Transaction.uPeerInfo.ui32I2CDevAddr = SX150X_Address;

  if (module == IomAddress) {
    am_hal_iom_blocking_transfer(cartridgeAddressI2cHandle, &Transaction);
  } else if (module == IomData) {
    am_hal_iom_blocking_transfer(cartridgeDataI2cHandle, &Transaction);
  }
}
