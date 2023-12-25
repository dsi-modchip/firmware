
#ifndef HAXBOARD_BOARD_GENERIC_H_
#define HAXBOARD_BOARD_GENERIC_H_

#include "haxboard_common.h"

// correct model
#define PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H 1

// 80 MHz max on chip
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 4
#endif

// 2 Mbit -> 256 kbyte
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (256 * 1024)
#endif

#endif

