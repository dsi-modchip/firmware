
#ifndef SPIPERI_H_
#define SPIPERI_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <hardware/dma.h>
#include <hardware/pio.h>
#include <hardware/spi.h>

struct spiperi {
	PIO pio;
	const pio_program_t* prgm;
	void* cb;
	uint8_t off;
	uint8_t sm;
	uint8_t bits;
	int8_t sclk, copi, cipo, ncs;
};
typedef struct spiperi spiperi_t;

enum spiperi_dir {
	spiperi_dir_cipo,
	spiperi_dir_copi,
};

typedef uint16_t (*spiperi_cb)(const spiperi_t* hw, uint32_t in);

// NOTE: don't set cipo to something zero or positive, FIFO refill latency is
//       too big to actually emulate being a real SPI peripheral
bool spiperi_init(spiperi_t* out, PIO pio, uint data_bits, spi_cpol_t cpol,
		spi_cpha_t cpha, spi_order_t order, int sclk, int copi, int cipo, int ncs);
void spiperi_deinit(spiperi_t* hw);

static inline bool spiperi_check_avail(const spiperi_t* hw) {
	return !pio_sm_is_rx_fifo_empty(hw->pio, hw->sm);
}
static inline uint16_t spiperi_get(const spiperi_t* hw) {
	return pio_sm_get_blocking(hw->pio, hw->sm);
	while (pio_sm_is_rx_fifo_empty(hw->pio, hw->sm)) tight_loop_contents();

	union {
		const io_rw_8* p8;
		const io_rw_16* p16;
		const io_rw_32* p32;
	} ptr;
	ptr.p32 = &hw->pio->rxf[hw->sm];

	if (hw->bits <= 8) return *ptr.p8;
	else return *ptr.p16;
}
static inline void spiperi_put(const spiperi_t* hw, uint16_t value) {
	if (hw->cipo < 0) return;

	(void)value;
	/*while (pio_sm_is_tx_fifo_full(hw->pio, hw->sm)) tight_loop_contents();

	if (hw->bits <= 8) *(io_rw_8*)&hw->pio->txf[hw->sm] = value;
	else *(io_rw_16*)&hw->pio->txf[hw->sm] = value;*/
}

static inline void __not_in_flash_func(spiperi_restart)(const spiperi_t* hw) {
	const uint32_t fdbmask = (1u << PIO_FDEBUG_TXOVER_LSB)
		| (1u << PIO_FDEBUG_RXUNDER_LSB)
		| (1u << PIO_FDEBUG_TXSTALL_LSB)
		| (1u << PIO_FDEBUG_RXSTALL_LSB);

	pio_sm_set_enabled(hw->pio, hw->sm, false);
	pio_sm_clear_fifos(hw->pio, hw->sm);
	hw->pio->fdebug = fdbmask << hw->sm;
	pio_sm_restart(hw->pio, hw->sm);
	pio_sm_clkdiv_restart(hw->pio, hw->sm);
	pio_sm_exec(hw->pio, hw->sm, pio_encode_jmp(hw->off));
	pio_sm_set_enabled(hw->pio, hw->sm, true);
}

// NOTE: needs to be already set in config/dmach:
//       * read address (if cipo) / write address (if copi) + whether to incr
//       * xfer count
//       * optional: chain to, ring, bswap, irq, prio, sniff, ...
void spiperi_dma_enable(const spiperi_t* hw, enum spiperi_dir dir,
		int ch, dma_channel_config* cfg, bool start);

void spiperi_irq_set_callback(spiperi_t* hw, spiperi_cb cb);
void spiperi_irq_set_enabled(const spiperi_t* hw, bool en);

#endif

