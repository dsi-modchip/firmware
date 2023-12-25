
#include <stdio.h>
#include <stdlib.h>

#include <RP2040.h>
#include <system_RP2040.h>
#include <core_cm0plus.h>

#include <tusb.h>
#include <libco.h>

#include "thread.h"


// defined by linker script
extern char __StackLimit;
extern char __StackBottom;


static uint set_stack_guard(uint index, void* addr_) {
	if (!addr_) return index;
	iprintf("[thread] set stack guard %u: %p\n", index, addr_);

	uintptr_t addr = (uintptr_t)addr_;

	uint32_t subregion_select = 0xffu ^ (1u << ((addr >> 5u) & 7u));

	// select address range, MPU region, and switch to it (VALID)
	MPU->RBAR = (addr & (uint)~0xff) | MPU_RBAR_VALID_Msk | (index<<MPU_RBAR_REGION_Pos);
	// set region attributes
	MPU->RASR = MPU_RASR_ENABLE_Msk // enable region
	          | (subregion_select << MPU_RASR_SRD_Pos)
	          | (0x7 << MPU_RASR_SIZE_Pos) // size 2^(7 + 1) = 256
	          | MPU_RASR_XN_Msk; // disable instruction fetch; no other bits means no permissions

	return index + 1;
}
static void set_stack_guards(bool do_threads, bool always_enable, int thrdno) {
	const uint32_t do_en_flg = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
	uint32_t nregs = (MPU->TYPE >> 8) & 0xff;

	uint32_t irqen = enter_critical_section();
	uint32_t was_en = MPU->CTRL & MPU_CTRL_ENABLE_Msk;
	MPU->CTRL ^= was_en; // disable MPU for a bit

	uint index = 0;

	if (!do_threads || thrdno < 0) {
		index = set_stack_guard(index, &__StackBottom);
	}
	if (do_threads) {
		if (thrdno >= 0) {
			index = set_stack_guard(index, threads[thrdno].stack);
		} else for (size_t i = 0; threads[i].magic == THREAD_MAGIC && index < nregs; ++i) {
			index = set_stack_guard(index, threads[i].stack);
		}
	}

	// TODO: other regions?

	for (size_t i = index; i < nregs; ++i) {
		MPU->RBAR = 0 | MPU_RBAR_VALID_Msk | (i<<MPU_RBAR_REGION_Pos);
		MPU->RASR = 0; // disable unused region
	}

	MPU->CTRL |= was_en | (always_enable ? do_en_flg : 0);
	exit_critical_section(irqen);
}


// we have to do this here because SDK code is kinda broken
__attribute__((__constructor__)) static void init_stack_guard(void) {
	if (MPU->CTRL & MPU_CTRL_ENABLE_Msk) return;

	// set up regions
	set_stack_guards(false, true, -1);
}


static cothread_t mainthread;

extern cothread_t co_active_handle;
extern uint32_t co_active_buffer[LIBCO_BUFFER_SIZE/sizeof(uint32_t)];
cothread_t co_active_handle;
uint32_t co_active_buffer[LIBCO_BUFFER_SIZE/sizeof(uint32_t)];

static bool do_tud_stuff = false;


void thread_yield(void) {
	if (co_active() == mainthread) return;

	__DSB();
	__ISB();
	//set_stack_guards(false, false, -1);
	co_switch(mainthread);
}


void sched_init(void) {
	// disable MPU for stack init
	MPU->CTRL ^= (MPU->CTRL & MPU_CTRL_ENABLE_Msk);

	mainthread = co_active();
	for (size_t i = 0; threads[i].magic == THREAD_MAGIC; ++i) {
		size_t size = threads[i].stacksize;
		if (size == 0) size = DEFAULT_THREAD_SIZE;
		size += MPU_MIN_REGION_SIZE + LIBCO_BUFFER_SIZE + IRQ_SAFETY_SIZE;
		threads[i].stacksize = size;

		void* stackptr = aligned_alloc(MPU_MIN_REGION_SIZE, size);

		if (stackptr == NULL) {
			iprintf("\n\nNot enough space to store all thread stacks!\n");
			for (size_t j = 0; j < i; ++j) {
				iprintf("thread[%u] ('%s') stack bottom = %p; size = %x\n",
						j, threads[j].name, threads[j].stack, threads[j].stacksize);
			}
			iprintf("thread[%u] ('%s') requests %x stack space...\n",
						i, threads[i].name, threads[i].stacksize);
			panic("Panicking now...");
		}

		threads[i].stack = stackptr;

		iprintf("[thread] initing '%s': stack=%p, size=%x, fn=%p, regs=%p, bot=%p\n",
				threads[i].name, threads[i].stack, size, threads[i].taskfn,
				threads[i].stack+MPU_MIN_REGION_SIZE,
				threads[i].stack+MPU_MIN_REGION_SIZE+LIBCO_BUFFER_SIZE+DEFAULT_THREAD_SIZE);
		memset(threads[i].stack, 0, size);
		threads[i].thread = co_derive(threads[i].stack + MPU_MIN_REGION_SIZE,
				size - MPU_MIN_REGION_SIZE, threads[i].taskfn);
	}

	set_stack_guards(true, true, -1);
}

void scheduler(void) { // simple round-robin
	for (size_t i = 0; threads[i].magic == THREAD_MAGIC; ++i) {
		do {
			tud_task();
			//iprintf("[sched] switch to task '%s': thrd=%p, pc=%8lx, sp=%8lx\n",
			//		threads[i].name, threads[i].thread,
			//		((uint32_t*)threads[i].thread)[/*10*/0], ((uint32_t*)threads[i].thread)[1/*9*/]); busy_wait_ms(16);
			do_tud_stuff = false;
			__DSB();
			__ISB();
			co_switch(threads[i].thread);
		} while (do_tud_stuff); // HACK: use a real scheduler lol
	}
}

void thread_do_tud_task(void) {
	do_tud_stuff = true;
	thread_yield();
}


// want to override this but pico_runtime said no :(
//void *_sbrk(int incr) {
//	const char* END = HEAP_END; //&__StackLimit;
//
//	extern char end; /* Set by linker.  */
//	static char *heap_end;
//	char *prev_heap_end;
//
//	if (heap_end == 0)
//		heap_end = &end;
//
//	prev_heap_end = heap_end;
//	char *next_heap_end = heap_end + incr;
//
//	if (__builtin_expect(next_heap_end > END, false)) {
//#if PICO_USE_OPTIMISTIC_SBRK
//		if (heap_end == END) {
//			return (char *) -1;
//		}
//		next_heap_end = END;
//#else
//		return (char *) -1;
//#endif
//	}
//
//	heap_end = next_heap_end;
//	return (void *) prev_heap_end;
//}

