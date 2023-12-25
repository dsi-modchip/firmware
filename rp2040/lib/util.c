
#include <stdio.h>

#include <hardware/clocks.h>
#include <hardware/dma.h>

#include "thread.h"

#include "util.h"

void hexdump(const char* pfix, uintptr_t baseaddr, const void* data_, size_t nbytes) {
	char fmt[8];
	const uint8_t* data = (const uint8_t*)data_;

	if (pfix) iprintf("%s:\n", pfix);

	uint32_t nybs = ((32 - __builtin_clz(baseaddr + nbytes)) + 3) >> 2;
	sniprintf(fmt, sizeof fmt, "%%0%lulx\t", nybs);

	for (size_t i = 0; i < nbytes; i += 16) {
		iprintf(fmt, baseaddr + i);
		for (size_t j = 0; i + j < nbytes && j < 16; ++j) {
			iprintf("%02hhx%c", data[i+j], (j == 15 || i+j == nbytes-1) ? '\n' : ' ');
		}
	}
}

uint32_t dma_crc32(uint32_t start, const void* addr, uint32_t len, bool threaded) {
	static uint32_t stuff = 0;

	uint dmach = dma_claim_unused_channel(true);
	dma_channel_config c = dma_channel_get_default_config(dmach);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_8); // TODO: can we speed it up?
	channel_config_set_sniff_enable(&c, true);

	dma_sniffer_enable(dmach, 0, false);
	dma_hw->sniff_data = start;

	dma_channel_configure(dmach, &c, &stuff, addr, len, true);
	if (threaded) {
		while (dma_channel_is_busy(dmach)) thread_yield();
	} else {
		dma_channel_wait_for_finish_blocking(dmach);
	}

	uint32_t r = dma_hw->sniff_data;
	dma_sniffer_disable();

	dma_channel_unclaim(dmach);

	return r;
}

static uint32_t crctable[256];

static uint32_t reflect(uint32_t refl, uint8_t b) {
	uint32_t value = 0;

	for (size_t i = 1; i < (b + 1u); i++) {
		if (refl & 1)
			value |= 1u << (b - i);
		refl >>= 1;
	}

	return value;
}

static void inittable(void) {
	const uint32_t polynomial = 0x04C11DB7;

	for (size_t i = 0; i < 0x100; i++) {
		crctable[i] = reflect(i, 8) << 24;

		for (size_t j = 0; j < 8; j++)
			crctable[i] = (crctable[i] << 1) ^ (crctable[i] & (1u << 31) ? polynomial : 0);

		crctable[i] = reflect(crctable[i],  32);
	}
}

uint32_t crc32(uint32_t start, const void* addr, uint32_t len) {
	static bool inited = false;
	if (!inited) {
		inittable();
		inited = true;
	}

	const uint8_t* data = addr;
	uint32_t crc = start ^ 0xFFFFFFFFu;
	for (; len; --len) {
		crc = (crc >> 8) ^ crctable[(crc & 0xFF) ^ *data];
		++data;
	}

	return (crc ^ 0xFFFFFFFFu);
}

bool pio_alloc_prgm(PIO* pio, uint* sm, uint* off, const pio_program_t* prgm) {
	if (!pio || !sm || !off || !prgm) return false;

	PIO p = *pio;
	if (p == NULL) {
		if (pio_can_add_program(pio0, prgm)) {
			int r = pio_claim_unused_sm(pio0, false);
			if (r >= 0) {
				*pio = pio0;
				*sm = r;
				goto alloc;
			}
		}
		if (pio_can_add_program(pio1, prgm)) {
			int r = pio_claim_unused_sm(pio1, false);
			if (r >= 0) {
				*pio = pio1;
				*sm = r;
				goto alloc;
			}
		}
	} else if (pio_can_add_program(p, prgm)) {
		int r = pio_claim_unused_sm(p, false);
		if (r >= 0) {
			//*pio = p;
			*sm = r;
			goto alloc;
		}
	}

	return false;

alloc:
	*off = pio_add_program(*pio, prgm);
	return true;
}

void print_clock_config(void) {
	static const char* clklut[] = {
		"gpio0", "gpio1", "gpio2", "gpio3",
		"ref", "sys", "peri", "usb",
		"adc", "rtc", NULL
	};

	for (enum clock_index i = 0; i < CLK_COUNT; ++i) {
		uint32_t cfg = clock_get_hz(i);
		uint32_t meas = frequency_count_khz(i);
		iprintf("[clocks] clk_%s: config = %lu Hz, measure = %lu kHz\n", clklut[i], cfg, meas);
	}
}

