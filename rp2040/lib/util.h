
#ifndef UTIL_H_
#define UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <hardware/pio.h>
#include <hardware/sync.h>

void hexdump(const char* pfix, uintptr_t baseaddr, const void* data, size_t nbytes);

uint32_t dma_crc32(uint32_t start, const void* addr, uint32_t len, bool threaded);
uint32_t crc32(uint32_t start, const void* addr, uint32_t len);

bool pio_alloc_prgm(PIO* pio, uint* sm, uint* off, const pio_program_t* prgm);

void print_clock_config(void);


#define ENUMERATE_BITS(x, n, ...) do { \
		for (uint32_t __bits = (x); __bits; ) { \
			uint32_t n = __builtin_ctz(__bits); \
			__bits ^= 1u << (n); \
			do{__VA_ARGS__;}while(0); \
		} \
	} while (0) \


static inline uint32_t enter_critical_section(void) {
	return save_and_disable_interrupts();
	/*__DSB();
	__ISB();
	uint32_t dis = __get_PRIMASK();
	__disable_irq();
	return dis;*/
}
static inline void exit_critical_section(uint32_t dis) {
	restore_interrupts(dis);
	/*__DSB();
	__ISB();
	__set_PRIMASK(dis);*/
}

#define CRITICAL_SECTION(...) do { \
		uint32_t __v = enter_critical_section(); \
		do{__VA_ARGS__;}while(0);\
		exit_critical_section(__v); \
	} while (0); \


#endif

