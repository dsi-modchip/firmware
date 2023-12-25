
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <RP2040.h>
#include <system_RP2040.h>
#include <core_cm0plus.h>

#include <hardware/structs/iobank0.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/vreg.h>
#include <pico/multicore.h>
#include <pico/platform.h>
#include <pico/stdlib.h>

#include "util.h"

#include "pinout.h" /* dependency injection lmao */

#include "glitch.h"

#include "trigctl.pio.h"

volatile struct glitch_params CORE0_FUNC(glitch_param_cur) = {0};
#define param_cur glitch_param_cur


static PIO CORE0_FUNC(trigctl_pio) = NULL;
static uint CORE0_FUNC(trigctl_sm), trigctl_off;
static bool clk_changed = false;

static int trigctl_pio_can_init(void) {
	if (pio_can_add_program(pio0, &trigctl_program)) {
		int r = pio_claim_unused_sm(pio0, false);
		if (r >= 0) return r;
	}

	if (pio_can_add_program(pio1, &trigctl_program)) {
		int r = pio_claim_unused_sm(pio1, false);
		if (r >= 0) return r + 4;
	}

	return -1;
}
static void trigctl_pio_alloc_init(int piosm) {
	trigctl_pio = (piosm & 4) ? pio1 : pio0;
	trigctl_sm = piosm & 3;

	trigctl_off = pio_add_program(trigctl_pio, &trigctl_program);

	enum trigctl_source src = (param_cur.trigger_in_pin < 0)
		? trig_source_irq : trig_source_pin;
	trigctl_pio_init(trigctl_pio, trigctl_sm, trigctl_off,
		src, param_cur.glitch_out_pin, param_cur.trigger_in_pin,
		//param_cur.trigger_in_polarity, param_cur.glitch_out_polarity
		false // disabled by default; needs to be armed
	);

	param_cur.trigctl_pio = trigctl_pio;
	param_cur.trigctl_sm = trigctl_sm;
}
static void trigctl_pio_deinit(void) {
	if (trigctl_pio) {
		pio_sm_set_enabled(trigctl_pio, trigctl_sm, false);
		pio_sm_unclaim(trigctl_pio, trigctl_sm);
		pio_remove_program(trigctl_pio, &trigctl_program, trigctl_off);
	}

	trigctl_pio = NULL;
	trigctl_off = trigctl_sm = ~(uint32_t)0;
}

#define scale_val(v) ((v)/HAXBOARD_CYCLE_DIV)

#define push_param(...) do{\
	uint32_t len = param_cur.length_ns.getter(&param_cur.length_ns), \
			 off = param_cur.offset_ns.getter(&param_cur.offset_ns); \
	 \
	/*if (__VA_OPT__(1?(__VA_ARGS__):)0) iprintf("[-->] new len = %lu, off = %lu\n", len, off);*/ \
	trigctl_push_off_len(trigctl_pio, trigctl_sm, scale_val(off), scale_val(len)); \
	param_cur.offset_ns.cur = off; \
	param_cur.length_ns.cur = len; \
} while (0) \

void __not_in_flash_func(glitch_pio_seed)(void) {
	if (pio_sm_is_tx_fifo_empty(trigctl_pio, trigctl_sm)) {
		push_param();
	}
}
void CORE0_FUNC(glitch_pio_isr)(void) {
	trigctl_ack_glitch_irq(trigctl_pio, trigctl_sm);

	push_param();
}

static void glitch_pio_isr_init(void) {
	// NOTE: spiperi has PIOx_IRQ_0 irq
	const int irq = (trigctl_pio == pio0) ? PIO0_IRQ_1 : PIO1_IRQ_1;
	irq_set_enabled(irq, false);
	trigctl_ack_glitch_irq(trigctl_pio, trigctl_sm);
	trigctl_set_glitch_irq_enabled(trigctl_pio, trigctl_sm, 1, true);
	irq_set_priority(irq, PICO_HIGHEST_IRQ_PRIORITY);
	irq_set_exclusive_handler(irq, glitch_pio_isr);

	glitch_pio_seed(); // kick off first transfer

	irq_set_enabled(irq, true);
}
static void glitch_pio_isr_deinit(void) {
	// NOTE: spiperi has PIOx_IRQ_0 irq
	const int irq = (trigctl_pio == pio0) ? PIO0_IRQ_1 : PIO1_IRQ_1;
	irq_set_enabled(irq, false);
	irq_remove_handler(irq, glitch_pio_isr);
	trigctl_set_glitch_irq_enabled(trigctl_pio, trigctl_sm, 1, false);
}


static void glitch_stop_no_clock_chg(void) {
	glitch_pio_isr_deinit();

	if (param_cur.impl != glitch_impl__none) {
		if (param_cur.impl == glitch_impl_pio) {
			trigctl_pio_deinit();
		}

		// deinit GPIO
		if (param_cur.trigger_in_pin >= 0) {
			hw_write_masked(&padsbank0_hw->io[param_cur.trigger_in_pin]
				, (PADS_BANK0_GPIO0_IE_BITS)
				| (PADS_BANK0_GPIO0_SCHMITT_BITS)
				, (PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS)
				| (PADS_BANK0_GPIO0_SCHMITT_BITS)
			);
			hw_write_masked(&iobank0_hw->io[param_cur.trigger_in_pin].ctrl
				, ((uint)GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB)
				| ((uint)GPIO_OVERRIDE_NORMAL << IO_BANK0_GPIO0_CTRL_INOVER_LSB)
				, (IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS)
				| (IO_BANK0_GPIO0_CTRL_INOVER_BITS)
			);
		}

		sio_hw->gpio_clr = 1u << param_cur.glitch_out_pin;
		sio_hw->gpio_oe_clr = 1u << param_cur.glitch_out_pin;
		hw_write_masked(&padsbank0_hw->io[param_cur.glitch_out_pin]
			, (PADS_BANK0_GPIO0_IE_BITS)
			| ((uint)GPIO_SLEW_RATE_SLOW << PADS_BANK0_GPIO0_SLEWFAST_LSB)
			| (PADS_BANK0_GPIO0_DRIVE_VALUE_2MA << PADS_BANK0_GPIO0_DRIVE_LSB)
			, (PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS)
			| PADS_BANK0_GPIO0_SLEWFAST_BITS | PADS_BANK0_GPIO0_DRIVE_BITS
		);
		hw_write_masked(&iobank0_hw->io[param_cur.glitch_out_pin].ctrl
			, ((uint)GPIO_FUNC_NULL << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB)
			| ((uint)GPIO_OVERRIDE_NORMAL << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB)
			, (IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS)
			| (IO_BANK0_GPIO0_CTRL_OUTOVER_BITS)
		);
	}

	memset((void*)&param_cur, 0, sizeof param_cur);
}

bool glitch_ready(const struct glitch_params* params) {
	// check params values
	if (!params->offset_ns.getter || !params->offset_ns.ud) return false;
	if (!params->length_ns.getter || !params->length_ns.ud) return false;
	if (params->trigger_in_pin < -1 || params->trigger_in_pin >= 28
			/*|| (params->trigger_in_pin >= 23 && params->trigger_in_pin <= 25)*/)
		return false;
	if (params->glitch_out_pin < 0 || params->glitch_out_pin >= 28
			/*|| (params->glitch_out_pin >= 23 && params->glitch_out_pin <= 25)*/)
		return false;
	if (params->trigger_in_polarity != glitch_positive
			&& params->trigger_in_polarity != glitch_negative) return false;
	if (params->glitch_out_polarity != glitch_positive
			&& params->glitch_out_polarity != glitch_negative) return false;
	if (params->impl != glitch_impl_pio)
		return false;

	int r = 0x99990;
	if (params->impl == glitch_impl_pio) {
		r = trigctl_pio_can_init();
		if (r < 0) {
			iprintf("[glitch] no pio available!\n");
			return false;
		}
	}

	glitch_stop();
	memcpy((void*)&param_cur, params, sizeof param_cur); // apply new params

	param_cur.offset_ns.cur = 0;
	param_cur.length_ns.cur = 0;
	param_cur.offset_ns.loops = 0;
	param_cur.length_ns.loops = 0;
	param_cur.armed = false;

	// overclock to 250 MHz
	if (param_cur.reconfig_sysclk) {
		iprintf("[glitch] reconfigure clock to %ld MHz!\n", param_cur.sys_clock_mhz);
		vreg_set_voltage(VREG_VOLTAGE_1_15);
		set_sys_clock_khz(param_cur.sys_clock_mhz*1000, true);
		clk_changed = true;
	}
	param_cur.sys_clock_mhz = 1000 / param_cur.sys_clock_mhz; // change into PIO cycles/ns

	uint func = GPIO_FUNC_SIO;
	if (params->impl == glitch_impl_pio) {
		trigctl_pio_alloc_init(r);
		func = (trigctl_pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;
	}

	if (params->trigger_in_pin >= 0) {
		// perform all of these at once to minimize the amount of spurious edges
		sio_hw->gpio_oe_clr = 1u << param_cur.trigger_in_pin;
		hw_write_masked(&padsbank0_hw->io[param_cur.trigger_in_pin]
			, (PADS_BANK0_GPIO0_IE_BITS)
			, (PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS)
			| (PADS_BANK0_GPIO0_SCHMITT_BITS)
		);
		hw_write_masked(&iobank0_hw->io[param_cur.trigger_in_pin].ctrl
			, (func << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB)
			| ((uint)(param_cur.trigger_in_polarity ? GPIO_OVERRIDE_INVERT
				: GPIO_OVERRIDE_NORMAL) << IO_BANK0_GPIO0_CTRL_INOVER_LSB)
			, (IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS)
			| (IO_BANK0_GPIO0_CTRL_INOVER_BITS)
		);
	}

	// perform all of these at once to minimize the amount of spurious glitches
	sio_hw->gpio_clr = 1u << param_cur.glitch_out_pin;
	sio_hw->gpio_oe_set = 1u << param_cur.glitch_out_pin;
	hw_write_masked(&padsbank0_hw->io[param_cur.glitch_out_pin]
		, PADS_BANK0_GPIO0_IE_BITS
		| ((uint)GPIO_SLEW_RATE_FAST << PADS_BANK0_GPIO0_SLEWFAST_LSB)
		| (PADS_BANK0_GPIO0_DRIVE_VALUE_12MA << PADS_BANK0_GPIO0_DRIVE_LSB)
		, PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
		| PADS_BANK0_GPIO0_SLEWFAST_BITS | PADS_BANK0_GPIO0_DRIVE_BITS
	);
	hw_write_masked(&iobank0_hw->io[param_cur.glitch_out_pin].ctrl
		, (func << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB)
		| ((uint)(param_cur.glitch_out_polarity ? GPIO_OVERRIDE_INVERT
			: GPIO_OVERRIDE_NORMAL) << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB)
		, IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS | IO_BANK0_GPIO0_CTRL_OUTOVER_BITS
	);
	iprintf("[glitch] glitch out init: func %d on pin %d, piocycles/ns = %lu\n",
			func, param_cur.glitch_out_pin, param_cur.sys_clock_mhz);
	//print_clock_config();

	glitch_pio_isr_init();

	return true;
}

void glitch_stop(void) {
	glitch_stop_no_clock_chg();
	if (param_cur.reconfig_sysclk && clk_changed) {
		iprintf("[glitch] resetting sys clock back to 125 MHz!\n");
		set_sys_clock_khz(125*1000, true);
		vreg_set_voltage(VREG_VOLTAGE_DEFAULT);
		clk_changed = false;
	}
}

