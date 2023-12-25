
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/binary_info.h>

#include <tusb.h>

#include "pinout.h"
#include "util.h"
#include "glitchitf.h"
#include "spictl.h"
#include "boothax.h"

#include "mcenv.h"


// NOTE: the below code is written with the assumption that R7=1
//       means that the hinge is closed. TODO verify this assumption!

enum {
	pinbits
		= (1u << PINOUT_nRESET)
#ifdef PINOUT_SPI_nCS2
		| (1u << PINOUT_SPI_nCS2)
#endif
#ifdef PINOUT_MBRD_DET
		| (1u << PINOUT_MBRD_DET)
#endif
#ifdef PINOUT_USB_DET
		| (1u << PINOUT_USB_DET)
#endif
#ifdef PINOUT_R7HINGE
		| (1u << PINOUT_R7HINGE)
#endif
};

static enum mcenv_modeflags stateflags = 0;
static bool track_dwm = false;

enum mcenv_modeflags mcenv_get_current(void) { return stateflags; }

static enum mcenv_modeflags __no_inline_not_in_flash_func(gather_mode)(void) {
	uint32_t pins = sio_hw->gpio_in & pinbits;

	enum mcenv_modeflags flg = 0;

	if (!(pins & (1u << PINOUT_nRESET))) flg |= mcflg_rst;
#ifdef PINOUT_SPI_nCS2
	if (track_dwm) {
		if (pins & (1u << PINOUT_SPI_nCS2)) {
			flg |= mcflg_dwm;
		}
	} else {
		flg |= (stateflags & mcflg_dwm); // keep old value
	}
#endif
#ifdef PINOUT_MBRD_DET
	if (pins & (1u << PINOUT_MBRD_DET)) flg |= mcflg_twl;
#else
	// assume always iff not connected via USB
	if (!(flg & mcflg_usb)) flg |= mcflg_twl;
#endif
#ifdef PINOUT_USB_DET
	if (pins & (1u << PINOUT_USB_DET)) flg |= mcflg_usb;
#else
	if (!(flg & mcflg_twl))
		flg |= mcflg_usb; //(stateflags & mcflg_usb);
#endif
#ifdef PINOUT_R7HINGE
	if (pins & (1u << PINOUT_R7HINGE)) flg |= mcflg_r7;
#endif

	return flg;
}

static void __no_inline_not_in_flash_func(handle_irq_gpio)(void) {
	uint32_t chgpins = 0;
	ENUMERATE_BITS(pinbits, pin, {
		if (gpio_get_irq_event_mask(pin) & GPIO_IRQ_EDGE_RISE) {
			gpio_acknowledge_irq(pin, GPIO_IRQ_EDGE_RISE);
			if (pin == PINOUT_nRESET) {
				//boothax_notify_strmsg("[mcenv] reset deassert");
				MCENV_NOTIFY_RST(false); // deasserted
			}
			chgpins |= 1u << pin;
		}
		if (gpio_get_irq_event_mask(pin) & GPIO_IRQ_EDGE_FALL) {
			gpio_acknowledge_irq(pin, GPIO_IRQ_EDGE_FALL);
			if (pin == PINOUT_nRESET) {
				//boothax_notify_strmsg("[mcenv] reset assert");
				MCENV_NOTIFY_RST(true); // asserted
			}
			chgpins |= 1u << pin;
		}
	});

	if (chgpins != 0) {
		enum mcenv_modeflags new = gather_mode(),
							 diff = new ^ stateflags;

		if (diff & mcflg_dwm) MCENV_NOTIFY_DWM(new & mcflg_dwm);
		if (diff & mcflg_twl) MCENV_NOTIFY_TWL(new & mcflg_twl);
		if (diff & mcflg_usb) MCENV_NOTIFY_USB(new & mcflg_usb);
		if (diff & mcflg_r7 ) MCENV_NOTIFY_R7( new & mcflg_r7 );
		//if (diff & mcflg_rst) MCENV_NOTIFY_RST(new & mcflg_rst); // already done above for speed

		stateflags = new;
	}
}

/*#if !defined(PINOUT_USB_DET)
void tud_mount_cb(void);
void tud_mount_cb(void) {
	CRITICAL_SECTION(
		stateflags |= mcflg_usb;
	);
	MCENV_NOTIFY_USB(true);
}
void tud_umount_cb(void);
void tud_umount_cb(void) {
	CRITICAL_SECTION(
		stateflags &= ~mcflg_usb;
	);
	MCENV_NOTIFY_USB(false);
}
bool tud_connected(void);
#endif*/

void mcenv_track_dwm(bool do_track) { track_dwm = do_track; }

void mcenv_init(void) {
	stateflags = 0;
	track_dwm = true;

#if defined(DSIHAXBOARD_BOARD_R1) || defined(DSIHAXBOARD_BOARD_R2)
	gpio_disable_pulls(PINOUT_nRESET);
#else
	gpio_pull_up(PINOUT_nRESET);
#endif
#ifdef PINOUT_SPI_nCS2
	gpio_pull_down(PINOUT_SPI_nCS2);
#endif
#ifdef PINOUT_MBRD_DET
	gpio_disable_pulls(PINOUT_MBRD_DET);
#endif
#ifdef PINOUT_USB_DET
	gpio_disable_pulls(PINOUT_USB_DET);
#endif
#ifdef PINOUT_R7HINGE
	gpio_pull_down(PINOUT_R7HINGE);
#endif

	irq_set_enabled(IO_IRQ_BANK0, false);

	ENUMERATE_BITS(pinbits, pin, {
		//iprintf("[mcenv] init pin %lu\n", pin);
		gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true);

		if (pin == PINOUT_nRESET) continue; // nRESET is constantly held low during initialization

		gpio_set_dir(pin, GPIO_IN);
		gpio_set_function(pin, GPIO_FUNC_SIO);
	});

	//iprintf("[mcenv] adding irq handler 0x%08x\n", pinbits);
	gpio_add_raw_irq_handler_masked(pinbits, handle_irq_gpio);
	//iprintf("[mcenv] added irq handler\n");

	stateflags = gather_mode();
#ifndef PINOUT_USB_DET
	if (tud_connected()) stateflags |= mcflg_usb;
#endif

	irq_set_enabled(IO_IRQ_BANK0, true);

	bi_decl(bi_1pin_with_name(PINOUT_nRESET, "TWL nRESET"));
#ifdef PINOUT_MBRD_DET
	bi_decl(bi_1pin_with_name(PINOUT_MBRD_DET, "TWL detect"));
#endif
#ifdef PINOUT_USB_DET
	bi_decl(bi_1pin_with_name(PINOUT_USB_DET, "USB detect"));
#endif
#ifdef PINOUT_R7HINGE
	bi_decl(bi_1pin_with_name(PINOUT_R7HINGE, "R7 (hinge)"));
#endif
}

void mcenv_deinit(void) {
	irq_set_enabled(IO_IRQ_BANK0, false);

	ENUMERATE_BITS(pinbits, pin,
		gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true);
	);
	gpio_remove_raw_irq_handler_masked(pinbits, handle_irq_gpio);

	track_dwm = false;
	stateflags = 0;

	irq_set_enabled(IO_IRQ_BANK0, true);
}

