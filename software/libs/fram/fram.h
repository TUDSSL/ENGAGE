/*
 * fram.h
 *
 *  Created on: Dec 25, 2019
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_FRAM_FRAM_H_
#define LIBS_FRAM_FRAM_H_

// SPI commands
#define FRAM_SET_WRITE_EN_LATCH 0x06
#define FRAM_RESET_WRITE_EN_LATCH 0x04
#define FRAM_READ_STATUS 0x05
#define FRAM_WRITE_STATUS 0x01

#define FRAM_READ_MEM_CODE 0x03
#define FRAM_WRITE_MEM_CODE 0x02
#define FRAM_READ_DEV_ID 0x9F
#define FRAM_FAST_READ 0x0B
#define FRAM_SLEEP_MODE 0xB9

#define FRAM_REF_ID 0x03497F04  // ID to be expected when reading dev id.

#endif  // LIBS_FRAM_FRAM_H_
