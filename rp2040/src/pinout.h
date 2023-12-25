
#ifndef PINOUT_H_
#define PINOUT_H_

#include <pico/config.h>  /* will include the selected board's header file */

#if defined(ADAFRUIT_ITSYBITSY_RP2040)
#include "pinout/itsybitsy.h"
#elif defined(WAVESHARE_RP2040_ZERO) || defined(WAVESHARE_RP2040_ONE) || defined(WAVESHARE_RP2040_TINY)
#include "pinout/ws.h"
#elif defined(SEEED_XIAO_RP2040)
#include "pinout/xiao.h"
#elif defined(DSIHAXBOARD_BOARD_R1)
#include "pinout/rev1.h"
#elif defined(DSIHAXBOARD_BOARD_R2)
#include "pinout/rev2.h"
#elif defined(RASPBERRYPI_PICO)
#include "pinout/pico.h"
#else
#error "Unknown board type!"
#endif /* board type */

#ifndef HAXBOARD_MAX_SPEED_MHZ
#define HAXBOARD_MAX_SPEED_MHZ 250
#endif
#define HAXBOARD_CYCLE_DIV (1000/HAXBOARD_MAX_SPEED_MHZ)

#endif

