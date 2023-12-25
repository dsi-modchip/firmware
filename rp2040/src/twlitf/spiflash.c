
#include <stdio.h>
#include <string.h>

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <hardware/timer.h>  /* busy_wait_xxx */
#include <pico/binary_info.h>

#include "pinout.h"
#include "boothax.h"
#include "thread.h"
#include "serprog.h"
#include "util.h"

#include "spiflash.h"

#define USE_DMA 0 /* FIXME: writing data is broken!!! */

#define iprintf(fmt, ...) do{}while(0)/**/


#if defined(PINOUT_SPI_DEV) /*|| !defined(HAXMODE_OVERRIDE) || HAXMODE_OVERRIDE != haxmode_attack*/
static bool cs_asserted, is_crc32;

static uint32_t freq, crc32res;
static enum serprog_flags sflags;
static uint8_t bpw;

static uint8_t selchip = 0;

//#if USE_DMA
static int dmach_rx, dmach_tx;
//#endif


static const uint32_t CSLINE_LUT[] = {
	[0] = 0,
	[1] = PINOUT_SPI_nCS3, // boothax SPI
	[2] = PINOUT_SPI_nCS2  // wifi flash SPI
};


bool spiflash_init(uint8_t sel) {
	if (selchip != 0) return false;
	//if (boothax_haxmode != haxmode_flash) return false;

	cs_asserted = false;

	freq = 512*1000;  // default to 512 kHz
	sflags = 3; // CPOL 1, CPHA 1, MSB first, std. SPI, chipsel active-low, not 3-wire
	bpw = 8;

	selchip = sel;

//#if USE_DMA
	dmach_rx = dma_claim_unused_channel(true);
	dmach_tx = dma_claim_unused_channel(true);
//#endif

	selchip = sel;
	spi_init(PINOUT_SPI_DEV, freq);

	gpio_set_function(PINOUT_SPI_CIPO, GPIO_FUNC_SPI);
	gpio_set_function(PINOUT_SPI_COPI, GPIO_FUNC_SPI);
	gpio_set_function(PINOUT_SPI_SCLK, GPIO_FUNC_SPI);

	uint32_t cspin = CSLINE_LUT[sel];
	if (cspin == 0) return 0;
	gpio_init(cspin);
	gpio_put(cspin, 1);
	gpio_set_dir(cspin, GPIO_OUT);
	gpio_set_function(cspin, GPIO_FUNC_SIO);
	iprintf("[spiflash] CS init\n");

	return true;

	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_COPI, "SPI bus COPI"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_CIPO, "SPI bus CIPO"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_SCLK, "SPI bus SCLK"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_nCS2, "SPI bus nCS2"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_nCS3, "SPI bus nCS3"));
}
void spiflash_deinit(void) {
//#if USE_DMA
	dma_channel_unclaim(dmach_rx);
	dma_channel_unclaim(dmach_tx);
//#endif

	uint32_t cspin = CSLINE_LUT[selchip];
	gpio_init(cspin);
	gpio_put(cspin, 1);
	gpio_set_dir(cspin, GPIO_OUT);

	gpio_set_slew_rate(PINOUT_SPI_SCLK, GPIO_SLEW_RATE_SLOW);
	gpio_set_drive_strength(PINOUT_SPI_SCLK, GPIO_DRIVE_STRENGTH_2MA);

	gpio_disable_pulls(PINOUT_SPI_SCLK);
	gpio_disable_pulls(PINOUT_SPI_CIPO);
	gpio_disable_pulls(PINOUT_SPI_COPI);

	gpio_set_function(PINOUT_SPI_CIPO, GPIO_FUNC_NULL);
	gpio_set_function(PINOUT_SPI_COPI, GPIO_FUNC_NULL);
	gpio_set_function(PINOUT_SPI_SCLK, GPIO_FUNC_NULL);

	spi_deinit(PINOUT_SPI_DEV);

	selchip = 0;
}

uint32_t spiflash_set_freq(uint32_t freq_wanted) {
	freq = spi_set_baudrate(PINOUT_SPI_DEV, freq_wanted);
	return freq;
}

static void apply_settings(void) {
	/*spi_set_format(PINOUT_SPI_DEV, bpw,
			(sflags & S_FLG_CPOL) ? SPI_CPOL_1 : SPI_CPOL_0,
			(sflags & S_FLG_CPHA) ? SPI_CPHA_1 : SPI_CPHA_0,
			SPI_MSB_FIRST);*/

	hw_write_masked(&spi_get_hw(PINOUT_SPI_DEV)->cr0
			, ((uint32_t)(bpw - 1) << SPI_SSPCR0_DSS_LSB)
			| ((uint32_t)((sflags & S_FLG_CPOL) ? SPI_CPOL_1 : SPI_CPOL_0) << SPI_SSPCR0_SPO_LSB)
			| ((uint32_t)((sflags & S_FLG_CPHA) ? SPI_CPHA_1 : SPI_CPHA_0) << SPI_SSPCR0_SPH_LSB)
			| ((uint32_t)((sflags >> 2) & 3) << SPI_SSPCR0_FRF_LSB),
		SPI_SSPCR0_DSS_BITS | SPI_SSPCR0_SPO_BITS | SPI_SSPCR0_SPH_BITS | SPI_SSPCR0_FRF_BITS
	);
}

enum serprog_flags spiflash_set_flags(enum serprog_flags flags) {
	iprintf("[spiflash] set flags: 0x%x\n", flags);
	if ((flags & (3<<2)) == (3<<2)) flags &= ~(uint32_t)(3<<2); // change to moto if bad value

	sflags = flags & ~S_FLG_LSBFST; // ignore LSB-first flag, we don't support it

	apply_settings();

	return sflags;
}
uint8_t spiflash_set_bpw(uint8_t bpw_) {
	bpw = bpw_;
	iprintf("[spiflash] set bpw: %hhx\n", bpw);
	if (bpw <  4) bpw =  4;
	if (bpw > 16) bpw = 16;

	apply_settings();

	return bpw;
}

void __not_in_flash_func(spiflash_cs_deselect)(uint8_t csflags) {
	busy_wait_us_32(1);
	gpio_put(CSLINE_LUT[csflags], 1);
	cs_asserted = false;
	//iprintf("[spiflash] CS high\n");
}
void __not_in_flash_func(spiflash_cs_select)(uint8_t csflags) {
	gpio_put(CSLINE_LUT[csflags], 0);
	busy_wait_us_32(1);
	cs_asserted = true;
	//iprintf("[spiflash] CS low\n");
}

void __not_in_flash_func(spiflash_op_begin)(uint8_t csflags) {
	if (!cs_asserted) {
		gpio_put(CSLINE_LUT[csflags], 0);
		busy_wait_us_32(1);
		//iprintf("[spiflash] CS low\n");
	}
}
void __not_in_flash_func(spiflash_op_end)(uint8_t csflags) {
	if (!cs_asserted) {   // YES, this condition is the intended one!
		busy_wait_us_32(1);
		gpio_put(CSLINE_LUT[csflags], 1);
		//iprintf("[spiflash] CS high\n");
	}
}

//#if USE_DMA
static void dma_kick_off(const void* wrdat, void* rddat, uint32_t nxfer) {
	if (!wrdat && !rddat) return;

	static uint32_t stuffbits = 0xffffffff;
	stuffbits = 0xffffffff;

	uint dreq_tx = spi_get_dreq(PINOUT_SPI_DEV, true );
	uint dreq_rx = spi_get_dreq(PINOUT_SPI_DEV, false);

	dma_channel_config c_tx = dma_channel_get_default_config(dmach_tx);
	dma_channel_config c_rx = dma_channel_get_default_config(dmach_rx);

	channel_config_set_transfer_data_size(&c_tx, (bpw > 8) ? DMA_SIZE_16 : DMA_SIZE_8);
	channel_config_set_transfer_data_size(&c_rx, (bpw > 8) ? DMA_SIZE_16 : DMA_SIZE_8);

	channel_config_set_read_increment (&c_tx, wrdat != NULL);
	channel_config_set_read_increment (&c_rx, false);
	channel_config_set_write_increment(&c_tx, false);
	channel_config_set_write_increment(&c_rx, rddat != NULL);

	channel_config_set_dreq(&c_tx, dreq_tx);
	channel_config_set_dreq(&c_rx, dreq_rx);

	if (is_crc32) {
		channel_config_set_sniff_enable(&c_rx, true);
		dma_sniffer_enable(dmach_rx, 0, false);
		dma_hw->sniff_data = crc32res;
	}

	dma_channel_configure(dmach_tx, &c_tx,
		&spi_get_hw(PINOUT_SPI_DEV)->dr,
		wrdat ? wrdat : &stuffbits,
		nxfer,
		true
	);
	dma_channel_configure(dmach_rx, &c_rx,
		rddat ? rddat : &stuffbits,
		&spi_get_hw(PINOUT_SPI_DEV)->dr,
		nxfer,
		true
	);

	while (dma_channel_is_busy(dmach_rx) || dma_channel_is_busy(dmach_tx)) {
		thread_yield(); // let other threads run while we're waiting
	}
	__compiler_memory_barrier();

	if (is_crc32) {
		crc32res = dma_hw->sniff_data;
		dma_sniffer_disable();
	}
}
//#endif

static void spi_do_xfer(spi_inst_t* spi, const void* src, void* dst, size_t nxfer) {
	// Never have more transfers in flight than will fit into the RX FIFO,
	// else FIFO will overflow if this code is heavily interrupted.
	const size_t fifo_depth = 8;
	size_t rx_remaining = nxfer, tx_remaining = nxfer;
	bool b16 = bpw > 8;

	const uint8_t* src8 = src;
	uint8_t* dst8 = dst;
	const uint16_t* src16 = src;
	uint16_t* dst16 = dst;

	while (rx_remaining || tx_remaining) {
		//bool try_next = false;

		if (tx_remaining && spi_is_writable(spi) && rx_remaining < tx_remaining + fifo_depth) {
			if (src) {
				if (b16) {
					spi_get_hw(spi)->dr = (uint32_t)*src16;
					++src16;
				} else {
					spi_get_hw(spi)->dr = (uint32_t)*src8;
					++src8;
				}
			} else {
				spi_get_hw(spi)->dr = b16 ? 0xffff : 0xff;
			}
			--tx_remaining;
			//try_next = true;
		}
		if (rx_remaining && spi_is_readable(spi)) {
			uint32_t v = spi_get_hw(spi)->dr;
			if (dst) {
				if (b16) {
					*dst16 = v;
					++dst16;
				} else {
					*dst8 = v;
					++dst8;
				}
			}
			--rx_remaining;
			//try_next = true;
		}

		//if (!try_next) thread_yield(); // doesn't quite work like this sadly,
		// this needs to be IRQ-driven instead... (FIXME)
	}

	// FIXME: handle dst==NULL case
	/*if (is_crc32 && dst) {
		crc32res = dma_crc32(crc32res, dst, nxfer*(b16?2:1), false);
	}*/
}


void spiflash_op_write(uint32_t write_len, const void* write_data) {
	uint32_t nwords = (bpw > 8) ? (write_len >> 1) : write_len;

#if USE_DMA
	if (write_len >= 0x10) {
		dma_kick_off(write_data, NULL, nwords);
		return;
	}
#endif

	spi_do_xfer(PINOUT_SPI_DEV, write_data, NULL, nwords);
}
void spiflash_op_read(uint32_t read_len, void* read_data) {
	uint32_t nwords = (bpw > 8) ? (read_len >> 1) : read_len;

//#if USE_DMA
	if (read_len >= 0x10) {
		dma_kick_off(NULL, read_data, nwords);
		return;
	}
//#endif

	spi_do_xfer(PINOUT_SPI_DEV, NULL, read_data, nwords);
}
void spiflash_op_read_write(uint32_t len, void* read_data, const void* write_data) {
	uint32_t nwords = (bpw > 8) ? (len >> 1) : len;

#if USE_DMA
	if (len >= 0x10) {
		dma_kick_off(write_data, read_data, nwords);
		return;
	}
#endif

	spi_do_xfer(PINOUT_SPI_DEV, write_data, read_data, nwords);
}

// ----------------------------------------------------------------------------

uint8_t spiflash_get_status(void) {
	uint8_t cmd[] = { flashcmd_stat, 0xff };

	spiflash_op_begin(selchip);
	spiflash_op_read_write(sizeof(cmd), cmd, cmd);
	spiflash_op_end(selchip);

	return cmd[1];
}

void spiflash_read_start(uint32_t addr, bool fast) {
	uint8_t cmd[] = {
		fast ? flashcmd_fast : flashcmd_read,
		addr>>16, addr>>8, addr,
		0
	};

	while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
		thread_yield();
	}

	spiflash_op_begin(selchip);
	spiflash_op_write(sizeof(cmd) - (fast ? 0 : 1), cmd);
}
void spiflash_read_cont(uint32_t rdlen, void* rddata) {
	spiflash_op_read(rdlen, rddata);
}
void spiflash_read_end(void) {
#if USE_DMA
	if (is_crc32) {
		crc32res = dma_hw->sniff_data;
		is_crc32 = false;
	}
#endif
	spiflash_op_end(selchip);
}

void spiflash_crc32_next_read(uint32_t start) {
	is_crc32 = true;
	crc32res = start;
}
uint32_t spiflash_crc32_get_result(void) {
	return crc32res;
}

uint32_t spiflash_read_jedec_id(void) {
	while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
		thread_yield();
	}

	uint8_t cmd[] = { flashcmd_rdid, 0xff, 0xff, 0xff };

	spiflash_op_begin(selchip);
	spiflash_op_read_write(sizeof(cmd), cmd, cmd);
	spiflash_op_end(selchip);

	return cmd[3] | ((uint32_t)cmd[2] << 8) | ((uint32_t)cmd[1] << 16);
}

bool spiflash_wren(void) {
	if (selchip != 1) return false; // only write to boothax flash

	while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
		thread_yield();
	}

	uint8_t cmd[] = { flashcmd_wren };

	spiflash_op_begin(selchip);
	spiflash_op_write(sizeof(cmd), cmd);
	spiflash_op_end(selchip);

	return spiflash_get_status() & SPIFLASH_STAT_WREN; // check for hw WP
}

void spiflash_wrdis(void) {
	if (selchip != 1) return; // only write to boothax flash

	while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
		thread_yield();
	}

	uint8_t cmd[] = { flashcmd_wrdi };

	spiflash_op_begin(selchip);
	spiflash_op_write(sizeof(cmd), cmd);
	spiflash_op_end(selchip);
}

bool spiflash_sector_erase(uint32_t addr) {
	addr &= ~(uint32_t)0xfff;

	if (!spiflash_wren()) return false;

	uint8_t cmd[] = {
		flashcmd_esec,
		addr>>16, addr>>8, addr>>0
	};

	spiflash_op_begin(selchip);
	spiflash_op_write(sizeof(cmd), cmd);
	spiflash_op_end(selchip);

	while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
		thread_yield();
	}

	return true;
}

bool spiflash_page_write(uint32_t addr, const uint8_t data[static 256], bool quirked) {
	addr &= ~(uint32_t)0xff;

	if (quirked) {
		uint8_t cmd[] = {
			flashcmd_prog,
			addr>>16, addr>>8, addr>>0,
			0
		};

		for (size_t i = 0; i < 256; ++i) {
			if (!spiflash_wren()) return false;

			cmd[4] = data[i];
			spiflash_op_begin(selchip);
			spiflash_op_write(sizeof(cmd), cmd);
			spiflash_op_end(selchip);

			while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
				thread_yield();
			}
		}
	} else {
		uint8_t cmd[] = {
			flashcmd_prog,
			addr>>16, addr>>8, addr>>0
		};

		if (!spiflash_wren()) return false;

		spiflash_op_begin(selchip);
		spiflash_op_write(sizeof(cmd), cmd);
		spiflash_op_write(256, data);
		spiflash_op_end(selchip);

		while (spiflash_get_status() & SPIFLASH_STAT_BUSY) {
			thread_yield();
		}
	}

	return true;
}
#else /* defined(PINOUT_SPI_DEV) || !defined(HAXMODE_OVERRIDE) || HAXMODE_OVERRIDE != haxmode_attack */
bool spiflash_init(uint8_t sel) { (void)sel; return true; }
void spiflash_deinit(void) { }
uint32_t spiflash_set_freq(uint32_t freq_wanted) { return freq_wanted; }
enum serprog_flags spiflash_set_flags(enum serprog_flags flags) { return flags; }
uint8_t spiflash_set_bpw(uint8_t bpw) { return bpw; }
void __not_in_flash_func(spiflash_cs_deselect)(uint8_t csflags) { (void)csflags; }
void __not_in_flash_func(spiflash_cs_select)(uint8_t csflags) { (void)csflags; }
void __not_in_flash_func(spiflash_op_begin)(uint8_t csflags) { (void)csflags; }
void __not_in_flash_func(spiflash_op_end)(uint8_t csflags) { (void)csflags; }
void spiflash_op_write(uint32_t write_len, const void* write_data) { (void)write_len; (void)write_data; }
void spiflash_op_read(uint32_t read_len, void* read_data) { (void)read_len; (void)read_data; }
void spiflash_op_read_write(uint32_t len, void* read_data, const void* write_data) { (void)len; (void)read_data; (void)write_data; }
uint8_t spiflash_get_status(void) { return 0; }
void spiflash_read_start(uint32_t addr, bool fast) { (void)addr; (void)fast; }
void spiflash_read_cont(uint32_t rdlen, void* rddata) { (void)rdlen; (void)rddata; }
void spiflash_read_end(void) { }
void spiflash_crc32_next_read(uint32_t start) { (void)start; }
uint32_t spiflash_crc32_get_result(void) { return 0; }
uint32_t spiflash_read_jedec_id(void) { return 0x133742; }
bool spiflash_wren(void) { return true; }
void spiflash_wrdis(void) { }
bool spiflash_sector_erase(uint32_t addr) { (void)addr; return true; }
bool spiflash_page_write(uint32_t addr, const uint8_t data[static 256], bool quirked) { (void)addr; (void)data; (void)quirked; return true; }
#endif /* defined(PINOUT_SPI_DEV) || !defined(HAXMODE_OVERRIDE) || HAXMODE_OVERRIDE != haxmode_attack */

