
#include <stdlib.h>

#include <hardware/structs/iobank0.h>
#if 0
#include <hardware/adc.h>
#endif
#include <hardware/gpio.h>
#include <pico/platform.h>
#include <pico/stdlib.h>

#include "glitch_param.h"

// PCG impl lifted from Wikipedia
#define PCG_MULTIPLIER 6364136223846793005uLL

__attribute__((__force_inline__))
inline static uint32_t pcg32_next(uint64_t* st) {
	uint64_t x = *st;
	unsigned count = (unsigned)(x >> 61);
	*st = x * PCG_MULTIPLIER;
	x ^= x >> 22;
	return (uint32_t)(x >> (22 + count));
}

uint32_t CORE0_FUNC(glitch_param_const_fn)(volatile struct glitch_param* gpb) {
	return ((struct glitch_param_const*)gpb->ud)->value;
}
uint32_t CORE0_FUNC(glitch_param_randrange_fn)(volatile struct glitch_param* gpb) {
	struct glitch_param_randrange* p = (struct glitch_param_randrange*)gpb->ud;

	return p->min + pcg32_next(&p->rand_state) % p->max/*actually delta now*/;
}
uint32_t CORE0_FUNC(glitch_param_sweep_fn)(volatile struct glitch_param* gpb) {
	struct glitch_param_sweep* p = (struct glitch_param_sweep*)gpb->ud;

	uint32_t rv = p->min + p->step * p->cur_index;
	if (rv > p->max) {
		p->cur_index = 0;
		++gpb->loops;
		rv = p->min;
	} else {
		++p->cur_phase;
		if (p->cur_phase >= p->period) {
			p->cur_phase = 0;
			++p->cur_index;
		}
	}

	return rv;
}
#if 0
uint32_t CORE0_FUNC(glitch_param_adc_fn)(volatile struct glitch_param* gpb) {
	struct glitch_param_adc* p = (struct glitch_param_adc*)gpb->ud;

	uint16_t av;
	{
		adc_select_input(1);
		av = adc_read();
	}

	uint64_t vvv = ((uint64_t)av * (uint64_t)p->max/*actually delta now*/) >> 12;

	return p->min + (uint32_t)vvv;
}
#endif

void glitch_param_randrange_init(struct glitch_param_randrange* p) {
	// use stdlib randomness only for the seed, but use a custom rng sitting in
	// SRAM 5 to avoid bus contention
	p->rand_state = 2*random()+1;
	pcg32_next(&p->rand_state);
	p->max = p->max - p->min;
}
#if 0
void glitch_param_adc_init(struct glitch_param_adc* p) {
	{
		// ADC1: ADC mode
		adc_init();
		adc_gpio_init(27);
		adc_select_input(1);
		// ADC0 = GPIO26 = fixed high (as vref)
		sio_hw->gpio_set = 1u << 26;
		sio_hw->gpio_oe_set = 1u << 26;
		gpio_set_function(26, GPIO_FUNC_SIO);
	}
	p->max = p->max - p->min;
}
#endif

