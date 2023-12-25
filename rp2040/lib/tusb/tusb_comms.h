
#ifndef TUSB_CDC_COM_H_
#define TUSB_CDC_COM_H_

#include <stdint.h>
#include <stdbool.h>

#include "tusb_config.h"
#include "thread.h"

struct tusb_comm {
	bool (*connected)(uint8_t index);
	uint32_t (*available)(uint8_t index);
	int (*read)(uint8_t index, void* buf, uint32_t length);
	void (*write)(uint8_t index, const void* buf, uint32_t length);
	uint8_t index;
};

extern struct tusb_comm tusb_comms[CFG_TUD_CDC+CFG_TUD_VENDOR];


void tusb_comm_init(void);


#ifdef COMM_IDX
static inline bool tuc_connected(void) { return tusb_comms[COMM_IDX].connected(COMM_IDX); }
static inline uint32_t tuc_available(void) { return tusb_comms[COMM_IDX].available(COMM_IDX); }
static inline int tuc_read(void* buf, uint32_t len) {
	return tusb_comms[COMM_IDX].read(COMM_IDX, buf, len);
}
static inline void tuc_write(const void* buf, uint32_t len) {
	tusb_comms[COMM_IDX].write(COMM_IDX, buf, len);
}

static inline uint8_t tuc_read_byte_blocking(void) {
	uint8_t v = 0;
	while (tuc_read(&v, 1) < 1) {
		for (size_t i = 0; i < 1000; ++i)
			asm volatile("nop");
		thread_yield();
	}
	return v;
}
#endif

#endif

