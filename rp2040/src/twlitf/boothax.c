
#include <stdio.h>

#include <pico/util/queue.h>

#include <tusb.h>

#include "thread.h"
#include "csmux.h"
#include "glitchitf.h"
#include "spictl.h"
#include "ledctl.h"
#include "pinout.h"

#include "boothax.h"


enum bhevent {
	ev_modechg,
	ev_begin,
	ev_done,
	ev_dwmavail,
	ev_dwmremove,
	ev_attempt,
	ev_strmsg,
};


static const char* modestrlut[] = {
	[boothax_idle    ] = "idle",
	[boothax_safemode] = "safemode",
	[boothax_flash   ] = "flash",
	[boothax_attack  ] = "attack",
	[boothax_train   ] = "train",
	NULL,
};


static queue_t queue;

static enum boothax_mode curmode;
enum boothax_mode boothax_mode_current(void) { return curmode; }


static enum boothax_mode derive_mode(enum mcenv_modeflags flg) {
	enum boothax_mode ret;

#ifdef HAXMODE_OVERRIDE
	(void)flg;
	enum {
		haxmode_attack = boothax_attack,
		haxmode_train  = boothax_train,
		haxmode_flash  = boothax_flash
	};

	ret = (enum boothax_mode)HAXMODE_OVERRIDE;
#else
	if (flg & mcflg_twl) {
		if (flg & mcflg_r7) {
			ret = boothax_safemode;
		} else {
			ret = boothax_train;//attack;//train; // TODO: decide between attack & train!
		}
	} else if (flg & mcflg_usb) {
		ret = boothax_flash;
	} else {
		ret = boothax_idle; // ???
	}
#endif

	return ret;
}

static void modeswitch(enum boothax_mode new) {
	if (new == curmode) return;
	enum boothax_mode old = curmode;

	iprintf("[boothax] switching from mode %s to mode %s...\n", modestrlut[old], modestrlut[new]);

	if (old == boothax_attack || old == boothax_train) {
		iprintf("[boothax] deiniting csmux and spictl!\n");
		spictl_deinit();
		csmux_deinit();
	} else if (old == boothax_flash) {
		// ?
	}

	if (old != boothax_idle) {
		glitchitf_stop();
	}

	enum glitchitf_start_mode start;
	enum glitchitf_stop_mode stop;
	switch (new) {
	case boothax_train:
		start = glitchitf_start_now;
		stop = glitchitf_stop_sweep;
		break;
	case boothax_attack:
		start = glitchitf_start_now;
		stop = glitchitf_stop_first;
		break;
	case boothax_safemode:
		start = glitchitf_start_ignore;
		stop = glitchitf_stop_first;
		break;
	case boothax_idle:
	case boothax_flash:
	default:
		start = glitchitf_start_inert;
		stop = glitchitf_stop_never;
		break;
	}

	curmode = new;

/*#if !defined(HAXMODE_OVERRIDE)
	if (new == boothax_flash || old == boothax_flash) {
		// reset USB bus to make host reenumerate the (new) device config
		iprintf("[boothax] reconnecting usb...\n");
		if (tud_disconnect()) {
			// a bus reset initiatd by the host seems to take at least 10 ms:
			// https://www.usbmadesimple.co.uk/ums_3.htm
			// not sure how in-spec it is to do this from the device side, but
			// let's do it anyway
			// as this is running inside a thread (as opposed to an IRQ), just
			// calling sleep() here is fine
			busy_wait_ms(12);
			tud_connect();
		} // else: not supported on mcu, oh well
	}
#endif*/

	if (new == boothax_attack || new == boothax_train) {
		iprintf("[boothax] initing csmux and spictl!\n");
		csmux_init(csmux_to_cs3); // TODO: is this the correct mode?
		spictl_init();
	} else if (new == boothax_flash) {
		// ?
	}

	if (new != boothax_idle) {
		uint32_t x = ev_begin;
		queue_try_add(&queue, &x);
		x = ((uint32_t)start << 16) | stop;
		queue_try_add(&queue, &x);
	}

	switch (new) {
	case boothax_idle:
	case boothax_safemode: // TODO: own led thingy!
		ledctl_mode_set(ledmode_idle);
		break;
	case boothax_attack:
		ledctl_mode_set(ledmode_attack);
		break;
	case boothax_train:
		ledctl_mode_set(ledmode_train);
		break;
	case boothax_flash:
		ledctl_mode_set(ledmode_flash_idle);
		break;
	default: break;
	}

	iprintf("[boothax] done initing\n");
}


void boothax_init(enum mcenv_modeflags initmode) {
	queue_init(&queue, sizeof(uint32_t), 256);
	glitchitf_init(glitchitf_params_default(), true); // TODO

	curmode = boothax_idle;
	enum boothax_mode new = derive_mode(initmode);
	iprintf("[boothax] init as %s from flags 0x%02x\n", modestrlut[new], initmode);
	modeswitch(new);
}

void boothax_thread(void) {
	for (uint32_t ev; ; thread_yield()) {
		if (!queue_try_remove(&queue, &ev)) {
			continue;
		}

		switch (ev) {
		case ev_modechg: {
			enum mcenv_modeflags flg;
			queue_remove_blocking(&queue, &flg);
			enum boothax_mode mode = derive_mode(flg);
			modeswitch(mode);
			} break;
		case ev_begin: {
			uint32_t x;
			enum glitchitf_start_mode start;
			enum glitchitf_stop_mode stop;
			queue_remove_blocking(&queue, &x);
			start = x >> 16;
			stop = x & 0xffff;
			glitchitf_begin(start, stop);
			} break;
		case ev_done:
			iprintf("[boothax] done!\n");
			ledctl_mode_set(ledmode_done);
			glitchitf_stop();
			glitchitf_begin(glitchitf_start_rst, glitchitf_stop_first);
			break;
		case ev_dwmavail:
			iprintf("[boothax] DWM available!\n");
			break;
		case ev_dwmremove:
			iprintf("[boothax] DWM removed!\n");
			break;
		case ev_attempt: {
			uint32_t off, len;
			queue_remove_blocking(&queue, &off);
			queue_remove_blocking(&queue, &len);
			struct glitchitf_params* cur = glitchitf_params_current();
			uint32_t oidx = (off - cur->offset_ns.min) / HAXBOARD_CYCLE_DIV,
					 lidx = (len - cur->length_ns.min) / HAXBOARD_CYCLE_DIV;
			iprintf("[boothax] attempt: offset=%lu ns, length=%lu ns oidx=%lu lidx=%lu\n", off, len, oidx, lidx);
			} break;
		case ev_strmsg: {
			const char* msg;
			queue_remove_blocking(&queue, &msg);
			iprintf("[boothax] strmsg: '%s'\n", msg);
			} break;
		default:
			break;
		}
	}
}


void __not_in_flash_func(boothax_notify_modechg)(enum mcenv_modeflags newmode) {
	uint32_t x = ev_modechg;
	queue_try_add(&queue, &x);
	x = newmode;
	queue_try_add(&queue, &x);
}
void __not_in_flash_func(boothax_notify_done)(void) {
	uint32_t x = ev_done;
	queue_try_add(&queue, &x);
}
void __not_in_flash_func(boothax_notify_dwmavail)(bool avail) {
	uint32_t x = avail ? ev_dwmavail : ev_dwmremove;
	queue_try_add(&queue, &x);
}
void __not_in_flash_func(boothax_notify_attempt)(uint32_t off, uint32_t len) {
	uint32_t x = ev_attempt;
	queue_try_add(&queue, &x);
	x = off;
	queue_try_add(&queue, &x);
	x = len;
	queue_try_add(&queue, &x);
}
void __not_in_flash_func(boothax_notify_strmsg)(const char* msg) {
	uint32_t x = ev_strmsg;
	queue_try_add(&queue, &x);
	x = (uint32_t)msg;
	queue_try_add(&queue, &x);
}

