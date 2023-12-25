
#include <stdio.h>

#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/binary_info.h>

#include "util.h"
#include "pinout.h"

#include "csmux.h"

#include "csmux.pio.h"


#if defined(PINOUT_SPI_nCS2) && defined(PINOUT_SPI_nCS3)
static PIO pio = pio1; // FIXME: DOESN'T WORK FOR PIO0???
static uint sm = ~(uint)0;
static uint off;

bool csmux_init(enum csmux_mode initmode) {
	bool r = pio_alloc_prgm(&pio, &sm, &off, &csmux_program);
	if (!r) {
		sm = ~(uint)0;
		return false;
	}
	//iprintf("[csmux] init: pio=%p (0=%p 1=%p)\n", pio, pio0, pio1);

	sio_hw->gpio_set = (1u<<PINOUT_SPI_nCS2)|(1u<<PINOUT_SPI_nCS3);
	sio_hw->gpio_oe_set = (1u<<PINOUT_SPI_nCS2)|(1u<<PINOUT_SPI_nCS3);
	gpio_set_function(PINOUT_SPI_nCS2, GPIO_FUNC_SIO);
	gpio_set_function(PINOUT_SPI_nCS3, GPIO_FUNC_SIO);

	csmux_pio_init(pio, sm, off,
		PINOUT_SPI_nCS_IN,
		//PINOUT_SPI_nCS3, // start outputting to nCS3
		false
	);

	pio_sm_set_consecutive_pindirs(pio, sm, PINOUT_SPI_nCS3, 1, true);
	pio_sm_set_consecutive_pindirs(pio, sm, PINOUT_SPI_nCS2, 1, true);

	pio_sm_set_enabled(pio, sm, true);

	csmux_switch(initmode);

	return true;

	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_nCS_IN, "CSMUX nCS_IN"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_nCS2, "CSMUX nCS2"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_nCS3, "CSMUX nCS3"));
}
void csmux_deinit(void) {
	if (!~sm) {
		pio_sm_set_enabled(pio, sm, false);
		pio_sm_unclaim(pio, sm);
		pio_remove_program(pio, &csmux_program, off);
		sm = ~(uint)0;
	}
}

void csmux_switch(enum csmux_mode mode) {
	switch (mode) {
	case csmux_to_cs2:
		pio_sm_set_out_pins(pio, sm, PINOUT_SPI_nCS2, 1);
		gpio_set_function(PINOUT_SPI_nCS2, GPIO_FUNC_PIO0 + pio_get_index(pio));
		gpio_set_function(PINOUT_SPI_nCS3, GPIO_FUNC_SIO);
		pio_sm_set_enabled(pio, sm, true);
		iprintf("[csmux] bridging to nCS2\n");
		break;
	case csmux_to_cs3:
		pio_sm_set_out_pins(pio, sm, PINOUT_SPI_nCS3, 1);
		gpio_set_function(PINOUT_SPI_nCS2, GPIO_FUNC_SIO);
		gpio_set_function(PINOUT_SPI_nCS3, GPIO_FUNC_PIO0 + pio_get_index(pio));
		pio_sm_set_enabled(pio, sm, true);
		iprintf("[csmux] bridging to nCS3\n");
		break;
	case csmux_none:
	default:
		pio_sm_set_enabled(pio, sm, false);
		gpio_set_function(PINOUT_SPI_nCS2, GPIO_FUNC_SIO);
		gpio_set_function(PINOUT_SPI_nCS3, GPIO_FUNC_SIO);
		iprintf("[csmux] set to hi-Z\n");
		break;
	}
}
#else
#warning "NOTE: compiling without CSmux!"
bool csmux_init(enum csmux_mode initmode) { (void)initmode; return true; }
void csmux_deinit(void) { }
void csmux_switch(enum csmux_mode mode) { (void)mode; }
#endif

