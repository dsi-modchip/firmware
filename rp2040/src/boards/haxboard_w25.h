
#ifndef HAXBOARD_BOARD_W25_H_
#define HAXBOARD_BOARD_W25_H_

#include "haxboard_common.h"

// correct model
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

// 133 MHz max on chip
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

// 32 Mbit -> 4 Mbyte
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

#endif

