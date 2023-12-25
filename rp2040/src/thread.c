
#include <stdio.h>
#include <stdbool.h>

#include <pico/stdlib.h>

#ifdef ENABLE_SERPROG
#include "serprog.h"
#endif
#include "spictl.h"
#include "boothax.h"

#include "thread.h"


#ifdef ENABLE_SERPROG
static void serprog_thread(void) {
	uint8_t csno = 1;

	serprog_init();
	thread_yield();

	while (true) {
		csno = serprog_task(csno);
		thread_yield();
	}
}
#endif
static void spictl_thread(void) {
	spictl_task_init();
	thread_yield();

	while (true) {
		spictl_task();
		thread_yield();
	}
}

struct thread threads[] = {
	{ .name = "boothax",.taskfn = boothax_thread, .magic=THREAD_MAGIC, .stacksize=0x380},
#ifdef ENABLE_SERPROG
	{ .name = "serprog",.taskfn = serprog_thread, .magic=THREAD_MAGIC, .stacksize=0x380}, // NOTE: use 380 here when enabling printf!
#endif
	{ .name = "spictl", .taskfn = spictl_thread , .magic=THREAD_MAGIC, .stacksize=0x380},
};

