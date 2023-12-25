
#ifndef GLITCHITF_H_
#define GLITCHITF_H_

#include "glitch.h"

enum glitchitf_start_mode {
	glitchitf_start_inert,  // ignore all reset asserts
	glitchitf_start_now,    // start now, force-assert reset
	glitchitf_start_rst,    // start on next reset assert
	glitchitf_start_ignore, // ignore next reset assert
};
enum glitchitf_stop_mode {
	glitchitf_stop_never, // never stop
	glitchitf_stop_sweep, // stop after parameter sweep
	glitchitf_stop_first, // stop on first success
};

struct glitchitf_param_range {
	uint32_t min, max, init;
};
struct glitchitf_params {
	struct glitchitf_param_range
		offset_ns,
		length_ns;
	uint32_t repeat;
};

struct glitchitf_heatmap {
	const uint8_t* ptr;
	uint32_t n_offset, stride_offset;
	uint32_t n_length, stride_length;
};

static inline uint8_t glitchitf_heatmap_read(const struct glitchitf_heatmap* hmap,
		uint32_t oidx, uint32_t lidx) {
	if (oidx >= hmap->n_offset || lidx >= hmap->n_length) return 0;
	return hmap->ptr[oidx * hmap->stride_offset + lidx * hmap->stride_length];
}


__attribute__((__const__))
const struct glitchitf_params* glitchitf_params_default(void);
const struct glitchitf_heatmap* glitchitf_heatmap_get(void);
const struct glitchitf_params* glitchitf_params_current(void);

bool glitchitf_init(const struct glitchitf_params* params, bool record);
void glitchitf_deinit(void);

void glitchitf_begin(
		enum glitchitf_start_mode startmode,
		enum glitchitf_stop_mode stopmode);
void glitchitf_stop(void);

/*static inline void glitchitf_arm(void) { glitch_arm(); }
static inline void glitchitf_disarm(void) { glitch_disarm(); }*/
static inline void glitchitf_trigger(void) { glitch_trigger_sw_pio(); }

void __not_in_flash_func(glitchitf_on_twlrst)(void);
void __not_in_flash_func(glitchitf_on_success)(void);

bool glitchitf_paramrange_from_heatmap(struct glitchitf_params* dst);


#define GLITCHITF_NOTIFY_DONE() do { \
		boothax_notify_done(); \
	} while (0) \


#define GLITCHITF_NOTIFY_ATTEMPT(o, l) do { \
		boothax_notify_attempt(o, l); \
	} while (0) \

#endif

