
#ifndef THREAD_H_
#define THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include <libco.h>

#include "util.h"  /* for critical_section stuff */


#if LIBCO_SMALL_IMPL
#define LIBCO_BUFFER_SIZE  8
#else
#define LIBCO_BUFFER_SIZE  64
#endif
#define MPU_MIN_REGION_SIZE 32
#define IRQ_SAFETY_SIZE 128 /* yeah, IRQs on cortex-m0 don't automatically go back to MSP... deep sigh */
#define DEFAULT_THREAD_SIZE 512

#define THREAD_MAGIC 0x13371337


void thread_yield(void);

// only call from main thread!
void sched_init(void);
void scheduler(void);

// quickly switch to tud task (and only that) and back from a worker thread
// yeah I should implement real scheduling I know. but eh.
void thread_do_tud_task(void);

struct thread {
	uint32_t magic;
	const char* name;
	cothread_t thread;
	void (*taskfn)(void);
	size_t stacksize;
	void* stack;
};

extern struct thread threads[];

#endif

