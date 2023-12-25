
#include <hardware/structs/iobank0.h>
#include <hardware/structs/padsbank0.h>
#include <hardware/structs/sio.h>
#include <hardware/gpio.h>
#include <pico/binary_info.h>

#include "pinout.h"

enum led_id {
	led_done = 1u<<PINOUT_LED_B2,
	led_mode = 1u<<PINOUT_LED_P2,
	led_act  = 1u<<PINOUT_LED_W,
	led_err  = 1u<<PINOUT_LED_P1,
	led_info = 1u<<PINOUT_LED_B1,
};

#define MASK_ALL_LEDS (led_done|led_mode|led_act|led_err|led_info)

void ledctl_init(void) {
	const uint32_t func = GPIO_FUNC_SIO;

	sio_hw->gpio_clr = MASK_ALL_LEDS;
	sio_hw->gpio_oe_set = MASK_ALL_LEDS;

	for (uint32_t i = PINOUT_LED_B2; i <= PINOUT_LED_B1; ++i) {
		gpio_disable_pulls(i);
		hw_write_masked(&padsbank0_hw->io[i]
			// stuff to set
			, ((uint)GPIO_SLEW_RATE_SLOW << PADS_BANK0_GPIO0_SLEWFAST_LSB)
			| (PADS_BANK0_GPIO0_DRIVE_VALUE_4MA << PADS_BANK0_GPIO0_DRIVE_LSB)
			// mask
			, PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
			| PADS_BANK0_GPIO0_SLEWFAST_BITS | PADS_BANK0_GPIO0_DRIVE_BITS
		);
		hw_write_masked(&iobank0_hw->io[i].ctrl
			// stuff to set
			, (func << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB)
			| ((uint)GPIO_OVERRIDE_NORMAL << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB)
			// mask
			, IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS | IO_BANK0_GPIO0_CTRL_OUTOVER_BITS
		);
	}

	bi_decl(bi_1pin_with_name(PINOUT_LED_B1, "status LED B1"));
	bi_decl(bi_1pin_with_name(PINOUT_LED_P1, "status LED P1"));
	bi_decl(bi_1pin_with_name(PINOUT_LED_W , "status LED W"));
	bi_decl(bi_1pin_with_name(PINOUT_LED_P2, "status LED P2"));
	bi_decl(bi_1pin_with_name(PINOUT_LED_B2, "status LED B2"));
}

void ledctl_mode_set_impl(enum led_mode mode) {
	static const uint32_t LUT[] = {
		[ledmode_idle] = led_info,
		[ledmode_attack] = led_act,
		[ledmode_done] = led_done,
		[ledmode_train] = led_info|led_act,
		[ledmode_flash_idle] = led_mode,
		[ledmode_flash_act] = led_mode|led_act,
		[ledmode_error] = led_err
	};

	if (mode < 0 || mode > ledmode_error) mode = ledmode_error;

	uint32_t v = LUT[mode];
	sio_hw->gpio_clr = MASK_ALL_LEDS^v;
	sio_hw->gpio_set = v;
}

