
#ifndef GLITCH_H_
#define GLITCH_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <hardware/pio.h>

#include "glitch_param.h"

enum glitch_polarity {
	glitch_positive,
	glitch_negative,
};
enum glitch_impl {
	glitch_impl__none,
	glitch_impl_pio
};

struct glitch_params {
	uint32_t sys_clock_mhz;
	struct glitch_param offset_ns;
	struct glitch_param length_ns;
	int trigger_in_pin; // use -1 for a signal coming from core 0
	int glitch_out_pin;
	enum glitch_polarity trigger_in_polarity;
	enum glitch_polarity glitch_out_polarity;
	enum glitch_impl impl;
	PIO trigctl_pio;
	uint32_t trigctl_sm;
	bool armed;
	bool reconfig_sysclk;
};

extern volatile struct glitch_params glitch_param_cur;

// return false: something wrong about the parameters
bool glitch_ready(const struct glitch_params* params);
void glitch_stop(void);

void __not_in_flash_func(glitch_pio_seed)(void);
static inline void glitch_trigger_sw_pio(void) {
	glitch_param_cur.trigctl_pio->irq_force =
		1 << ((glitch_param_cur.trigctl_sm + 1) & 3);
	/*trigctl_send_trig_irq(glitch_param_cur.trigctl_pio,
			glitch_param_cur.trigctl_sm);*/
}

static inline void glitch_arm(void) {
	pio_sm_restart(glitch_param_cur.trigctl_pio, glitch_param_cur.trigctl_sm);
	glitch_pio_seed();
	pio_sm_set_enabled(glitch_param_cur.trigctl_pio, glitch_param_cur.trigctl_sm, true);

	glitch_param_cur.armed = true;
}
static inline void glitch_disarm(void) {
	pio_sm_set_enabled(glitch_param_cur.trigctl_pio,
		glitch_param_cur.trigctl_sm, false);

	glitch_param_cur.armed = false;
}

//void glitch_isr_secondary_handler(void);

static inline uint32_t glitch_get_last_offset(void) {
	return glitch_param_cur.offset_ns.cur;
}
static inline uint32_t glitch_get_last_length(void) {
	return glitch_param_cur.length_ns.cur;
}

#endif

