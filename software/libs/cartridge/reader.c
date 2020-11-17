/*
 * reader.c
 *
 *  Created on: Sep 27, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "reader.h"

#include "am_util.h"
#include "platform.h"
#include "string.h"

CHECKPOINT_EXCLUDE_BSS
uint8_t romSize;
CHECKPOINT_EXCLUDE_BSS
uint8_t ramSize;
CHECKPOINT_EXCLUDE_BSS
uint8_t cartridgeType;
CHECKPOINT_EXCLUDE_BSS
uint8_t numRomBanks;

// Save these through power failure
uint8_t romTitle[17];
uint32_t cartridgeSizeBytes = 0;

uint32_t cartridgeGetSizeBytes() { return cartridgeSizeBytes; }

void cartridgeConfig() {
  cartridgeInterfaceConfig();

  if (cartridgeSanityCheck()) {
    am_util_stdio_printf("Error reading cartridge!\n");
    while (1) {
      ;
    }
  } else {
    am_util_stdio_printf("Cartridge found proceeding...\n");
  }
}

uint8_t checkSavedCartridge(const uint8_t* startOfRom) {
  if (startOfRom[CartridgeStart] != 0x00 &&
      startOfRom[CartridgeStart + 1] != 0xC3) {
    return 0;
  }

  memcpy(romTitle, &startOfRom[CartridgeTitleAddress], 0x10);
  romTitle[16] = '\0';

  cartridgeType = startOfRom[CartridgeTypeAddress];
  romSize = startOfRom[CartridgeRomSizeAddress];
  ramSize = startOfRom[CartridgeRamSizeAddress];

  numRomBanks = 2;  // 32K default ROM size
  if (romSize >= 1) {
    numRomBanks = 2 << romSize;
  }

  cartridgeSizeBytes = numRomBanks * CartridgeRomBankSize;
  return 1;
}

uint8_t cartridgeSanityCheck() {
  uint8_t startOfCartridge[2] = {};
  for (uint16_t address = CartridgeStart; address < CartridgeStart + 2;
       address++) {
    cartridgeInterfaceReadData(address,
                               &startOfCartridge[address - CartridgeStart]);
  }

  if (startOfCartridge[0] != 0x00 || startOfCartridge[1] != 0xC3) {
    return 1;  // error
  }
  return 0;  // success!
}

void cartridgeReadInfo() {
  for (uint16_t address = CartridgeTitleAddress;
       address < CartridgeTitleAddress + 0x10; address++) {
    cartridgeInterfaceReadData(address,
                               &romTitle[address - CartridgeTitleAddress]);
  }
  romTitle[16] = '\0';

  cartridgeInterfaceReadData(CartridgeTypeAddress, &cartridgeType);
  cartridgeInterfaceReadData(CartridgeRomSizeAddress, &romSize);
  cartridgeInterfaceReadData(CartridgeRamSizeAddress, &ramSize);

  numRomBanks = 2;  // 32K default ROM size
  if (romSize >= 1) {
    numRomBanks = 2 << romSize;
  }

  am_util_stdio_printf("Reading cartridge: %s\n", romTitle);
  am_util_stdio_printf("Type: %d, RomSize %d, RamSize %d, RomBanks %d \n",
                       cartridgeType, romSize, ramSize, numRomBanks);
  am_util_stdio_printf("Reserved memory region hex: 0x%p, to 0x%p \n",
                       GameRomStart, GameRomEnd);
}

void switchCartridgeRomBank(uint8_t bank) {
  if (cartridgeType >= 5) {  // For MBC 2
    cartridgeInterfaceWriteData(CartridgeRamBankRegisterAddress, bank);
  } else {  // For MBC 1
    cartridgeInterfaceWriteData(0x6000, 0);
    cartridgeInterfaceWriteData(0x4000, bank >> 5);
    cartridgeInterfaceWriteData(0x2000, bank & 0x1F);
  }
}

CHECKPOINT_EXCLUDE_BSS
uint8_t flashBuffer[AM_HAL_FLASH_PAGE_SIZE] __attribute__((aligned(4)));

void cartridgeReadRom() {
  uint32_t flashRomSizeCounter = 0;

  for (uint8_t romBank = 1; romBank < numRomBanks; ++romBank) {
    switchCartridgeRomBank(romBank);

    uint16_t address = (romBank > 1) ? CartridgeRamBankAddress : 0x0000;
    if ((romBank * CartridgeRomBankSize + CartridgeRomBankSize - 1) >=
        (GameRomEnd - GameRomStart)) {
      am_util_stdio_printf("ROM to big, increase ROM size ... \n");
      return;
    }

    uint32_t flashPageCounter = 0;
    for (; address < (CartridgeRomBankSize + CartridgeRamBankAddress);
         ++address) {
      cartridgeInterfaceReadData(
          address, &flashBuffer[flashRomSizeCounter % AM_HAL_FLASH_PAGE_SIZE]);
      flashRomSizeCounter++;
      flashPageCounter++;

      if (flashPageCounter >= AM_HAL_FLASH_PAGE_SIZE) {
        uint32_t flashRomAddress = flashRomSizeCounter -
                                   AM_HAL_FLASH_PAGE_SIZE +
                                   (uint32_t)GameRomStart;
        flashPageCounter = 0;
        am_util_stdio_printf("  ... programming flash instance %d, page %d.\n",
                             AM_HAL_FLASH_ADDR2INST(flashRomAddress),
                             AM_HAL_FLASH_ADDR2PAGE(flashRomAddress));

        am_hal_flash_page_erase(AM_HAL_FLASH_PROGRAM_KEY,
                                AM_HAL_FLASH_ADDR2INST(flashRomAddress),
                                AM_HAL_FLASH_ADDR2PAGE(flashRomAddress));

        am_hal_flash_program_main(
            AM_HAL_FLASH_PROGRAM_KEY, (uint32_t*)flashBuffer,
            (uint32_t*)flashRomAddress, AM_HAL_FLASH_PAGE_SIZE / 4);
      }
    }
  }

  cartridgeSizeBytes = numRomBanks * CartridgeRomBankSize;
}
