
#include <stdint.h>
#include <stdbool.h>

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/spi.h>

#include "util.h"

#include "spiperi.pio.h"

#include "spiperi.h"

static spiperi_t* spiperi_hw[2];

inline static void __not_in_flash_func(spiperi_pio_isr_base)(spiperi_t* hw) {
	spiperi_ack_irq(hw->pio, hw->sm);

	union {
		const io_rw_8* p8;
		const io_rw_16* p16;
		const io_rw_32* p32;
	} ptr;

	uint32_t v;
	ptr.p32 = &hw->pio->rxf[hw->sm];
	if (hw->bits <= 8) v = *ptr.p8;
	else v = *ptr.p16;

	v = ((spiperi_cb)hw->cb)(hw, v);

	/*if (hw->cipo >= 0) {
		if (hw->bits <= 8) *(io_rw_8*)&hw->pio->txf[hw->sm] = v;
		else *(io_rw_16*)&hw->pio->txf[hw->sm] = v;
	}*/
}

static void __not_in_flash_func(spiperi_pio0_isr)(void) { spiperi_pio_isr_base(spiperi_hw[0]); }
static void __not_in_flash_func(spiperi_pio1_isr)(void) { spiperi_pio_isr_base(spiperi_hw[1]); }

bool spiperi_init(spiperi_t* out, PIO pio, uint data_bits,
		spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t order,
		int sclk, int copi, int cipo, int ncs) {
	uint sm;
	uint off;
	const pio_program_t* prgm = &spiperi_program;

	if (!pio_alloc_prgm(&pio, &sm, &off, &spiperi_program)) return false;

	spiperi_pio_init(pio, sm, off,
			data_bits, cpol, cpha, order,
			sclk, copi, cipo, ncs, false);
	pio_sm_set_enabled(pio, sm, true);

	out->pio = pio;
	out->prgm = prgm;
	out->cb = NULL;
	out->sm  = sm;
	out->off = off;
	out->bits = data_bits;
	out->sclk = sclk;
	out->copi = copi;
	out->cipo = cipo;
	out->ncs  = ncs ;
	spiperi_hw[pio_get_index(pio)] = out;

	spiperi_put(out, 0xffff); // seed first fifo entry

	iprintf("[spiperi] inited: pio=%d sm=%d off=%u\n", pio_get_index(pio), sm, off);
	return true;
}

void spiperi_deinit(spiperi_t* hw) {
	gpio_set_function(hw->sclk, GPIO_FUNC_NULL);
	gpio_set_function(hw->copi, GPIO_FUNC_NULL);
	//if (hw->cipo >= 0) gpio_set_function(hw->cipo, GPIO_FUNC_NULL);
	if (hw->ncs  >= 0) gpio_set_function(hw->ncs , GPIO_FUNC_NULL);

	pio_sm_set_enabled(hw->pio, hw->sm, false);
	pio_sm_unclaim(hw->pio, hw->sm);
	pio_remove_program(hw->pio, hw->prgm, hw->off);
}

void spiperi_dma_enable(const spiperi_t* hw, enum spiperi_dir dir,
		int ch, dma_channel_config* cfg, bool start) {
	bool tx = dir == spiperi_dir_cipo;

	channel_config_set_transfer_data_size(cfg,
			(hw->bits > 8) ? DMA_SIZE_16 : DMA_SIZE_8);
	channel_config_set_dreq(cfg, pio_get_dreq(hw->pio, hw->sm, tx));

	if (tx) {
		channel_config_set_write_increment(cfg, false);
		dma_channel_set_write_addr(ch, &hw->pio->txf[hw->sm], false);
	} else {
		channel_config_set_read_increment(cfg, false);
		dma_channel_set_read_addr(ch, &hw->pio->rxf[hw->sm], false);
	}

	dma_channel_set_config(ch, cfg, start);
}

void spiperi_irq_set_callback(spiperi_t* hw, spiperi_cb cb) {
	// NOTE: glitch has PIOx_IRQ_1 irq
	const int irq = (hw->pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
	irq_handler_t isr = (hw->pio == pio0) ? spiperi_pio0_isr : spiperi_pio1_isr;

	hw->cb = cb;
	if (cb != NULL) {
		irq_set_exclusive_handler(irq, isr);
	} else {
		irq_remove_handler(irq, isr);
	}
}
void spiperi_irq_set_enabled(const spiperi_t* hw, bool en) {
	// NOTE: glitch has PIOx_IRQ_1 irq
	const int irq = (hw->pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;

	irq_set_enabled(irq, false);
	spiperi_ack_irq(hw->pio, hw->sm);
	spiperi_set_irq_enabled(hw->pio, hw->sm, 0, en);
	if (en) {
		irq_set_priority(irq, PICO_HIGHEST_IRQ_PRIORITY);
		irq_set_enabled(irq, true);
	} else {
		irq_set_priority(irq, PICO_HIGHEST_IRQ_PRIORITY);
	}
}

