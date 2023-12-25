
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/vreg.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include <tusb.h>

#include "stdio_uart_dma.h"
#include "thread.h"
#include "util.h"
#include "tusb_comms.h"
#include "glitchitf.h"
#include "ledctl.h"
#include "boothax.h"
#include "pinout.h"
#include "info.h"

#include "csmux.h"


int main() {
	vreg_set_voltage(VREG_VOLTAGE_1_25);
	set_sys_clock_khz(HAXBOARD_MAX_SPEED_MHZ*1000, true);
	for (size_t i = 0; i < 0x100; ++i) asm volatile("nop");
	busy_wait_ms(16);

	stdio_uart_dma_init();
#if PINOUT_UART_TX != PICO_DEFAULT_UART_TX_PIN
	gpio_set_function(PINOUT_UART_TX, GPIO_FUNC_UART);
#endif

	alarm_pool_init_default();
	iprintf("\n\n[main] ping (%d MHz = %d ns per cycle)\n\n",
			HAXBOARD_MAX_SPEED_MHZ, HAXBOARD_CYCLE_DIV);

	ledctl_init();
	mcenv_init();
	boothax_init(mcenv_get_current());

	tusb_init();
	tusb_comm_init();
	sched_init();
	iprintf("[main] inited haxboardfw!\n");
	while (true) {
		scheduler();
	}

	panic("??? wut ???");
	__builtin_unreachable();
	return 0;

	bi_decl(bi_program_description(INFO_PRODUCT));
	bi_decl(bi_program_version_string(INFO_VERSION_STRING));
	bi_decl(bi_program_url(INFO_URL));
	bi_decl(bi_program_build_attribute("vendor=" INFO_VENDOR));

#ifdef PASSIVE_TEST
	bi_decl(bi_program_feature("PASSIVE_TEST"));
#endif
#ifdef HAXMODE_DEV_FEATURES
	bi_decl(bi_program_feature("HAXMODE_DEV_FEATURES"));
#endif
#ifdef HAXMODE_OVERRIDE
	bi_decl(bi_program_feature("HAXMODE_OVERRIDE=" STRINGIFY(HAXMODE_OVERRIDE)));
#endif
}

