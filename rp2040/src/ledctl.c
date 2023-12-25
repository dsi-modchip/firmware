
#include <stdbool.h>

#include <pico/config.h>  /* will include the selected board's header file */

#include "ledctl.h"


static void ledctl_mode_set_impl(enum led_mode mode);


static enum led_mode last_mode;
static bool has_error;


void ledctl_mode_set(enum led_mode mode) {
	last_mode = mode;
	if (mode == ledmode_error) {
		has_error = true;
	} else {
		ledctl_mode_set_impl(mode);
	}
}
void ledctl_clear_error(void) {
	if (has_error) {
		has_error = false;
		ledctl_mode_set_impl(last_mode);
	}
}

#if defined(DSIHAXBOARD_BOARD_R1)
#include "ledctl-v1.c"
#elif defined(PICO_DEFAULT_WS2812_PIN)
#include "ledctl-ws2812.c"
#elif defined(PICO_DEFAULT_LED_PIN)
#include "ledctl-pico.c"
#elif defined(CYW43_WL_GPIO_LED_PIN)
#error "CYW43-based boards (eg Pico W) not supported in LED driver, sorry"
#else
#error "unknown board, can't use the LED driver"
#endif

