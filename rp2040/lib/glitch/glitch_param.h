
#ifndef GLITCH_PARAM_H_
#define GLITCH_PARAM_H_

#include <stdint.h>


#define __STRINGIFY(f) #f
#define STRINGIFY(f) __STRINGIFY(f)
#define CORE0_FUNC(f) __scratch_x(STRINGIFY(f)) f
#define CORE1_FUNC(f) __scratch_y(STRINGIFY(f)) f

struct glitch_param;

typedef uint32_t (*glitch_param_fn)(volatile struct glitch_param*);

struct glitch_param {
	glitch_param_fn getter;
	void* ud;
	uint32_t cur;
	uint32_t loops;
};

struct glitch_param_const {
	uint32_t value;
};
struct glitch_param_randrange {
	uint32_t min;
	uint32_t max;
	uint64_t rand_state;
};
struct glitch_param_sweep {
	uint32_t min;
	uint32_t max;
	uint32_t step;
	uint32_t period;
	uint32_t cur_index;
	uint32_t cur_phase;
};
struct glitch_param_adc {
	uint32_t min;
	uint32_t max;
	int adc_index;
};

uint32_t CORE0_FUNC(glitch_param_const_fn)(volatile struct glitch_param*);
uint32_t CORE0_FUNC(glitch_param_randrange_fn)(volatile struct glitch_param*);
uint32_t CORE0_FUNC(glitch_param_sweep_fn)(volatile struct glitch_param*);
uint32_t CORE0_FUNC(glitch_param_adc_fn)(volatile struct glitch_param*);

inline static void glitch_param_const_init(struct glitch_param_const* p) { (void)p; }
inline static void glitch_param_sweep_init(struct glitch_param_sweep* p) { (void)p; }
// NOTE: call this AFTER initializing min and max!
void glitch_param_randrange_init(struct glitch_param_randrange* p);
// NOTE: call this AFTER initializing min, max and adc_index!
void glitch_param_adc_init(struct glitch_param_adc* p);

#define GLITCH_PARAM(uu) \
	((struct glitch_param) { \
		.ud = &(uu), \
		.cur = 0, .loops = 0, \
		.getter = _Generic((uu), \
			struct glitch_param_const: glitch_param_const_fn, \
			struct glitch_param_randrange: glitch_param_randrange_fn, \
			struct glitch_param_sweep: glitch_param_sweep_fn, \
			struct glitch_param_adc: glitch_param_adc_fn \
		) \
	}) \

#endif

