
#include <stdio.h>
#include <string.h>

#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/binary_info.h>
#include <pico/time.h>

#include "glitch.h"
#include "boothax.h"
#include "pinout.h"

#include "glitchitf.h"


#define ROUND_DOWN_PIOCLK(x) (((x)/HAXBOARD_CYCLE_DIV)*HAXBOARD_CYCLE_DIV)
#define ROUND_UP_PIOCLK(x) ((((x)+((HAXBOARD_CYCLE_DIV)-1))/HAXBOARD_CYCLE_DIV)*HAXBOARD_CYCLE_DIV)

static int alarmno;
static void __no_inline_not_in_flash_func(handle_irq_timer)(uint alarm);

// --- parameter control ------------------------------------------------------


static const struct glitchitf_params defparams = {
	.offset_ns = {
		.min = ROUND_DOWN_PIOCLK(991890/*915*/),
		.max = ROUND_UP_PIOCLK(  991960/*25*/),
		//.init= ROUND_DOWN_PIOCLK(991920),
	},
	.length_ns = {
		.min = ROUND_DOWN_PIOCLK(60/*80*/),
		.max = ROUND_UP_PIOCLK( 100/*85*/),
		//.init= ROUND_DOWN_PIOCLK(80),
	},
	.repeat = 5,
};


#define MAX_ELEMENTS (64*16) /* 1 KiB of memory */
static uint8_t heatmap_data[MAX_ELEMENTS];
static struct glitchitf_heatmap hmap;
static struct glitchitf_params curparam;

static struct glitch_param_sweep gp_offset, gp_length;
static struct glitch_params gparams = {
	.trigger_in_pin = -1,
	.glitch_out_pin = PINOUT_GLITCH_OUT,
	.trigger_in_polarity = glitch_positive, // FIXME (currently hardcoded in PIO code)
	.glitch_out_polarity = glitch_positive,
	.impl = glitch_impl_pio,
	.sys_clock_mhz = HAXBOARD_MAX_SPEED_MHZ,
	.reconfig_sysclk = false,
};

static bool do_record = false;

static inline void glitchitf_heatmap_incr(uint32_t oidx, uint32_t lidx) {
	if (oidx >= hmap.n_offset || lidx >= hmap.n_length || !do_record) return;
	uint8_t* p = &heatmap_data[oidx * hmap.stride_offset + lidx * hmap.stride_length];
	if (*p != 0xff) ++*p; // clamp instead of overflowing
}

const struct glitchitf_params* glitchitf_params_default(void) { return &defparams; }
const struct glitchitf_params* glitchitf_params_current(void) { return &curparam; }
const struct glitchitf_heatmap* glitchitf_heatmap_get(void) {
	return hmap.ptr ? &hmap : NULL;
}

bool glitchitf_init(const struct glitchitf_params* params, bool record) {
	alarmno = hardware_alarm_claim_unused(true);
	//hardware_alarm_claim((alarmno = 0));
	irq_set_enabled(TIMER_IRQ_0+alarmno, false);
	hardware_alarm_set_callback(alarmno, handle_irq_timer);

	curparam = *params;

	uint32_t
		num_o = ((curparam.offset_ns.max - curparam.offset_ns.min) / HAXBOARD_CYCLE_DIV) + 1,
		num_l = ((curparam.length_ns.max - curparam.length_ns.min) / HAXBOARD_CYCLE_DIV) + 1;

	gp_offset.min = curparam.offset_ns.min;
	gp_offset.max = curparam.offset_ns.max;
	gp_offset.step = HAXBOARD_CYCLE_DIV;
	gp_offset.period = curparam.repeat;

	gp_length.min = curparam.length_ns.min;
	gp_length.max = curparam.length_ns.max;
	gp_length.step = HAXBOARD_CYCLE_DIV;
	gp_length.period = curparam.repeat * num_o;

	glitch_param_sweep_init(&gp_offset);
	glitch_param_sweep_init(&gp_length);

	if (curparam.offset_ns.init != 0) {
		gp_offset.cur_index = (curparam.offset_ns.init - curparam.offset_ns.min) / HAXBOARD_CYCLE_DIV;
	}
	if (curparam.length_ns.init != 0) {
		gp_length.cur_index = (curparam.length_ns.init - curparam.length_ns.min) / HAXBOARD_CYCLE_DIV;
	}

	gparams.offset_ns = GLITCH_PARAM(gp_offset);
	gparams.length_ns = GLITCH_PARAM(gp_length);

	if (!glitch_ready(&gparams))
		return false;

	iprintf("[glitchitf] init\n");
	memset(heatmap_data, 0, sizeof(heatmap_data));
	if (record) {
		if (num_o * num_l > MAX_ELEMENTS) {
			do_record = false;
			memset(&hmap, 0, sizeof hmap);  // won't be able to record!
		} else {
			do_record = true;
			hmap.ptr = heatmap_data;
			hmap.n_offset = num_o;
			hmap.n_length = num_l;
			hmap.stride_offset = sizeof(uint8_t);
			hmap.stride_length = sizeof(uint8_t)*num_o;
		}
	} else do_record = false;

	return true;
}

void glitchitf_deinit(void) {
	glitchitf_stop();
	glitch_stop();
	gpio_pull_down(PINOUT_GLITCH_OUT); // make sure there's no spurious glitches
	do_record = false;
}

bool glitchitf_paramrange_from_heatmap(struct glitchitf_params* dst) {
	if (!dst || !hmap.ptr) return false;

	return false; // TODO: implement!
}

// --- glitch sequencing and stuff --------------------------------------------


// sequencing logic:
// * reset release: start timeout timer
// * on timer timeout: pulse reset line low (using timer)
// * on success:
//   * if stop at first: stop now
//   * else if stop sweep & last one: stop now
//   * else:
//     * pulse reset line low (using timer) after N delay
//   * if do_record: record

static enum {
	timer_idle,           // do nothing (wait for next reset)
	timer_reset_assert,   // start asserting reset
	timer_reset_deassert, // stop asserting reset
	timer_glitch_wait,    // wait for glitch success/failure
	timer_success_wait,   // wait after glitch success before taking action
} timer_action;
enum {
	timer_delay_reset   =   90,
	timer_delay_wait    =  333, // reset rise -> glitch success should be ~161 ms typically
	timer_delay_success =  400,
};

static enum glitchitf_start_mode startmode;
static enum glitchitf_stop_mode stopmode;

static void __no_inline_not_in_flash_func(timer_start_base)(int action, uint32_t timeout_ms, bool print) {
	hardware_alarm_cancel(alarmno);
	if (timeout_ms == 0) return;

	//if (print) iprintf("[glitchitf] irq timer start: alarmno=%d act=%d ms=%lu\n",
	//		alarmno, action, timeout_ms);
	//else boothax_notify_strmsg("[glitchitf] irq timer start");

	timer_action = action;
	irq_set_enabled(TIMER_IRQ_0+alarmno, true);
	hardware_alarm_set_target(alarmno, make_timeout_time_ms(timeout_ms));
}
#define timer_start(a, t, ...) timer_start_base(a, t, __VA_OPT__(1 ? (__VA_ARGS__) :) false)

static void __no_inline_not_in_flash_func(handle_irq_timer)(uint alarm) {
	(void)alarm;
	bool stop_sweep = false;
	//boothax_notify_strmsg("[glitchitf] on irq timer");

	switch (timer_action) {
	case timer_glitch_wait:
		// timer expired -> failure -> next one OR stop
		// i.e. just fallthru
	case timer_success_wait:
		GLITCHITF_NOTIFY_ATTEMPT(glitch_param_cur.offset_ns.cur, glitch_param_cur.length_ns.cur);
		if (stopmode == glitchitf_stop_never) {
			//stop_sweep = false; // continue with next try
		} else if (stopmode == glitchitf_stop_sweep) {
			if (glitch_param_cur.offset_ns.loops == 0 || glitch_param_cur.length_ns.loops == 0)
				/*stop_sweep = true*/;
			else stop_sweep = true;
		} else { //if (stopmode == glitchitf_stop_first) {
			stop_sweep = (timer_action == timer_success_wait);
		}
		if (stop_sweep) {
			GLITCHITF_NOTIFY_DONE();
			break;
		}
	case timer_reset_assert:
#ifndef PASSIVE_TEST
		gpio_set_dir(PINOUT_nRESET, GPIO_OUT);
		//boothax_notify_strmsg("[glitchitf] reset assert");
#endif
		timer_start(timer_reset_deassert, timer_delay_reset);
		break;
	case timer_reset_deassert:
#ifndef PASSIVE_TEST
		gpio_set_dir(PINOUT_nRESET, GPIO_IN);
		//boothax_notify_strmsg("[glitchitf] reset deassert");
#endif
		//timer_start(timer_glitch_wait, timer_delay_wait); // see glitchitf_on_twlrst
		break;
	case timer_idle:
	default:
		break;
	}
}
void __not_in_flash_func(glitchitf_on_twlrst)(void) {
	//boothax_notify_strmsg("[glitchitf] on twlrst");
	switch (startmode) {
	case glitchitf_start_now:
	case glitchitf_start_rst:
		timer_start(timer_glitch_wait, timer_delay_wait);
		break;
	case glitchitf_start_ignore:
		startmode = glitchitf_start_rst;
		break;
	case glitchitf_start_inert:
	default:
		break;
	}
}
void __not_in_flash_func(glitchitf_on_success)(void) {
	timer_start(timer_success_wait, timer_delay_success);
	uint32_t off_cur = glitch_param_cur.offset_ns.cur,
			 len_cur = glitch_param_cur.length_ns.cur;
	uint32_t oidx = (off_cur - curparam.offset_ns.min) / HAXBOARD_CYCLE_DIV,
			 lidx = (len_cur - curparam.length_ns.min) / HAXBOARD_CYCLE_DIV;

	if (do_record) glitchitf_heatmap_incr(oidx, lidx);
}

static const char* startstrlut[] = {
	"inert",
	"now",
	"rst",
	"ignore",
	NULL
};
static const char* stopstrlut[] = {
	"never",
	"sweep",
	"first"
};

void glitchitf_begin(enum glitchitf_start_mode start, enum glitchitf_stop_mode stop) {
	startmode = start;
	stopmode = stop;
#ifndef PASSIVE_TEST
	gpio_put(PINOUT_nRESET, 0);
	gpio_set_dir(PINOUT_nRESET, GPIO_IN);
	gpio_set_function(PINOUT_nRESET, GPIO_FUNC_SIO);
#endif

	iprintf("[glitchitf] begin: start=%s, stop=%s alarmno=%d\n", startstrlut[start], stopstrlut[stop], alarmno);

	if (start == glitchitf_start_now) {
		startmode = glitchitf_start_rst;

#ifndef PASSIVE_TEST
		gpio_set_dir(PINOUT_nRESET, GPIO_OUT);
		//iprintf("[glitchitf] reset assert\n");
#endif
		timer_start(timer_reset_deassert, timer_delay_reset, true);
	}

#ifndef PASSIVE_TEST
	if (start != glitchitf_start_inert) {
		glitch_arm();
		iprintf("[glitch] ARMED!\n");
	}
#endif
}
void glitchitf_stop(void) {
#ifndef PASSIVE_TEST
	glitch_disarm();
#endif
	iprintf("[glitchitf] stop\n");
	if (alarmno != -1) hardware_alarm_cancel(alarmno); // cancel but don't unclaim

	if (startmode == glitchitf_start_rst) startmode = glitchitf_start_ignore;
#ifndef PASSIVE_TEST
	gpio_set_dir(PINOUT_nRESET, GPIO_IN);
#endif

	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_GLITCH_OUT, "GLITCH_OUT"));
}

