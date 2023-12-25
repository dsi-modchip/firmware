
#include <hardware/structs/iobank0.h>
#include <hardware/structs/padsbank0.h>
#include <hardware/structs/sio.h>
#include <hardware/gpio.h>
#include <pico/binary_info.h>

#include "util.h"
#include "pinout.h"

#include "ws2812.pio.h"


#define IS_RGBW true
#define COLOR(r,g,b) (((uint32_t)(r)<<16)|((uint32_t)(g)<<24)|((uint32_t)(b)<<8))

static PIO pio = NULL;
static uint sm;

void ledctl_init(void) {
#ifdef PINOUT_LED_WS2812_POWER
	gpio_put(PINOUT_LED_WS2812_POWER, 1);
	gpio_set_dir(PINOUT_LED_WS2812_POWER, GPIO_OUT);
	gpio_disable_pulls(PINOUT_LED_WS2812_POWER);
	gpio_set_function(PINOUT_LED_WS2812_POWER, GPIO_FUNC_SIO);
#endif

#ifdef PINOUT_LED_WS2812_PIODEV
	pio = PINOUT_LED_WS2812_PIODEV;
#endif
	uint off;
	if (pio_alloc_prgm(&pio, &sm, &off, &ws2812_program)) {
		ws2812_program_init(pio, sm, off, PINOUT_LED_WS2812, 800*1000, IS_RGBW);
		pio_sm_set_enabled(pio, sm, true);
	} else {
		pio = NULL;
	}

	bi_decl(bi_1pin_with_name(PINOUT_LED_WS2812, "WS2812 LED"));
#ifdef PINOUT_LED_WS2812_POWER
	bi_decl(bi_1pin_with_name(PINOUT_LED_WS2812_POWER, "WS2812 LED power switch"));
#endif
}

void ledctl_mode_set_impl(enum led_mode mode) {
	static uint32_t LUT[] = {
		[ledmode_idle] = COLOR(0,0,16),
		[ledmode_attack] = COLOR(64,0,32),
		[ledmode_done] = COLOR(0,16,0),
		[ledmode_train] = COLOR(32,32,0),
		[ledmode_flash_idle] = COLOR(0,16,16),
		[ledmode_flash_act] = COLOR(16,16,16),
		[ledmode_error] = COLOR(64,0,0)
	};

	if (mode < 0 || mode > ledmode_error) mode = ledmode_error;

	uint32_t v = LUT[mode];
	v = COLOR(128,128,128);
	if (pio) pio_sm_put_blocking(pio, sm, v);
}

