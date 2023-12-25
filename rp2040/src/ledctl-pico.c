
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/binary_info.h>
#include <pico/time.h>

#include "boothax.h"


static volatile bool phase;
static int alarmno;
static volatile uint32_t blink_period;
#define MKPERI(a,b) ((uint32_t)(a)|((uint32_t)(b)<<16))
#define PERI_GET_A(v) ((v)&0xffffu)
#define PERI_GET_B(v) (((v)>>16)&0xffffu)


inline static void set(int v) {
	gpio_put(PICO_DEFAULT_LED_PIN, v);
}

static void __no_inline_not_in_flash_func(handle_irq_timer)(uint alarm) {
	(void)alarm;
	//boothax_notify_strmsg("[ledctl] <pico> timer irq");
	set(phase ? 0 : 1);
	hardware_alarm_set_target(alarmno, make_timeout_time_ms(
				phase ? PERI_GET_B(blink_period) : PERI_GET_A(blink_period)));
	phase = !phase;
}

void ledctl_init(void) {
	phase = false;
	alarmno = -1;
	blink_period = 0;
	gpio_put(PICO_DEFAULT_LED_PIN, 0);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	gpio_disable_pulls(PICO_DEFAULT_LED_PIN);
	gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_SIO);

	// set up timer IRQ for timeouts
	alarmno = hardware_alarm_claim_unused(true);
	irq_set_enabled(TIMER_IRQ_0+alarmno, true);
	hardware_alarm_set_callback(alarmno, handle_irq_timer);
	//iprintf("[ledctl] <pico> inited\n");

	bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "status LED"));
}

void ledctl_mode_set_impl(enum led_mode mode) {
	static const uint32_t LUT[] = {
		[ledmode_idle] = ~(uint32_t)0,           // continuous on
		[ledmode_attack] = MKPERI(500,500),      // 1 Hz 50% duty cycle
		[ledmode_done] = 0,                      // continuous off
		[ledmode_train] = MKPERI(1000,1000),     // 0.5 Hz 50% duty cycle
		[ledmode_flash_idle] = MKPERI(400,1600), // 0.5 Hz 20% duty cycle
		[ledmode_flash_act] = MKPERI(200,800),   // 1 Hz 20% duty cycle
		[ledmode_error] = MKPERI(250,250),       // 2 Hz 50% duty cycle

		// any faster than 2 Hz may cause photosensitivity seizures
		// (according to wikipedia). W3 limiti s 3 Hz but I'm erring on the
		// safe side here.
	};

	if (mode < 0 || mode > ledmode_error) mode = ledmode_error;

	uint32_t peri = LUT[mode];

	if (alarmno != -1) hardware_alarm_cancel(alarmno);
	blink_period = peri;
	phase = true;
	uint32_t a = PERI_GET_A(peri);
	//iprintf("[ledctl] <pico> ledmode %d: peri = 0x%08lx\n", mode, peri);
	if (a == 0) {
		set(0); // always off -> turn off, no timer
	} else {
		set(1); // turn on
		if (~a == 0) {
			// always on -> nothing to do
		} else {
			hardware_alarm_set_target(alarmno, make_timeout_time_ms(a));
		}
	}
}

