
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <hardware/pio.h>
#include <pico/binary_info.h>
#include <pico/util/queue.h>

#include "pinout.h"
#include "spiperi.h"
#include "boothax.h"
#include "glitchitf.h"

#include "spictl.h"

#define COMM_IDX 0
#include "tusb_comms.h"

#define ENABLE_DBG_PRINT

static spiperi_t spiperi;
static uint8_t curcmd;
static uint32_t curarg;
static uint32_t byteind;
static uint8_t reads_since_rst;
static bool trig_on_next_cs = false;

#ifdef ENABLE_DBG_PRINT
static queue_t queue_dbg={0};
#endif
static queue_t queue_com={0};

static uint8_t msgbuf[256];
static size_t msglen;

static const uint8_t spi_succeed_msg[4] = {0x05,0x13,0x37,0x42};
static bool spimsggood = true;

// yeah technically there's race conditions between these irqs and the spiperi
// isr. but i dont think they're reentrant + its very unlikely that they'll
// happen at the same (not just "low uniform probability" but it doesn't make
// sense wrt the timing of the incoming signals) time so it's Fine™
void __not_in_flash_func(spictl_on_rst)(void) {
	spiperi_restart(&spiperi);
	reads_since_rst = 0;

/*#ifdef ENABLE_DBG_PRINT
	uint32_t x = 0xaaaaaaaa;
	queue_try_add(&queue_dbg, &x);
#endif*/
}
static void __not_in_flash_func(handle_irq_gpio)(void) {
	if (gpio_get_irq_event_mask(PINOUT_SPI_nCS_IN) & GPIO_IRQ_EDGE_RISE) {
		gpio_acknowledge_irq(PINOUT_SPI_nCS_IN, GPIO_IRQ_EDGE_RISE);

		byteind = 0;

		if (trig_on_next_cs) {
			SPICTL_NOTIFY_TRIG();
			trig_on_next_cs = false;

/*#ifdef ENABLE_DBG_PRINT
			uint32_t x = 0xfefefefe;
			queue_try_add(&queue_dbg, &x);
#endif*/
		}

/*#ifdef ENABLE_DBG_PRINT
		uint32_t x = 0xbbbbbbbb;
		queue_try_add(&queue_dbg, &x);
#endif*/
	}
}

static uint16_t __not_in_flash_func(spiperi_isr)(const spiperi_t* hw, uint32_t v) {
	(void)hw;
	uint32_t x; (void)x;
	uint16_t r = 0xffff;

	if (byteind == 0) {
/*#ifdef ENABLE_DBG_PRINT
		uint32_t x = 0xff000000u | v;
		queue_try_add(&queue_dbg, &x);
#endif*/
		curarg = 0;
		curcmd = v;
		++byteind;
		if (curcmd == 0x03 && reads_since_rst < 0xff) {
			++reads_since_rst;
		}
	} else {
		switch (curcmd) {
		case 0x03:
			if (byteind < 4) {
				curarg = (curarg << 8) | v;
				++byteind;
				if (byteind == 4) {
					// match reads_since_rst and curarg (== read addr) with stuff:
					// read 1, addr 0x000000 (len 0x028): magic header bytes
					// read 2, addr 0x0002ff (len 0x001): bootflags
					// read 3, addr 0x000200 (len 0x200): boothdr  -> TRIGGER ; IF ADDR WRONG NOT A SPIBOOT!
					// read 4, addr 0x000020 (len 0x002): stage2 config stuff -> FAILURE
					// read 4, addr 0x001000 (len 0xXXX): SPIboot loading 'ARM7' binary (ARM9 payload) -> SUCCESS
					// on failure, no other SPI xfers may happen (well, only to the
					// PMIC), so for that (plus for crashes) a timeout is used as well
					if (reads_since_rst <= 3 && curarg == 0x000200) {
						trig_on_next_cs = true;
						asm volatile("":::"memory");
					}
#ifdef ENABLE_DBG_PRINT
					x = curarg | ((uint32_t)curcmd << 24);
					queue_try_add(&queue_dbg, &x);
#endif
				}
			}
			break;

		case 0x05: {
			if (byteind == 1) {
				curarg = v;
/*#ifdef ENABLE_DBG_PRINT
				uint32_t x = 0x05000000u | v;
				queue_try_add(&queue_dbg, &x);
#endif*/
			} else switch (curarg) {
			case 0x00: break; // official firmware stuff bit for the real status command
			case 0x13:
				if (byteind < 4) {
					spimsggood &= spi_succeed_msg[byteind] == v;

					if (spimsggood && byteind == 3) {
						SPICTL_NOTIFY_SUCCESS();
#ifdef ENABLE_DBG_PRINT
						x = 0x05133742;
						queue_try_add(&queue_dbg, &x);
#endif
					}
				}
				break;
			case 0x50: // 'P'
				queue_try_add(&queue_com, &v);
				break;
			default: break;
			}

			// saturating increment (in case this would ever overflow...)
			if (byteind != 0xfffffffu) ++byteind;
			} break; // 0x05 command byte
		default: break;
		}
	}

	return r;
}

void spictl_init(void) {
	spictl_task_init();

	curarg = byteind = curcmd = reads_since_rst = 0;
	PIO pio = NULL;
#ifdef PINOUT_SPI_PIODEV
	pio = PINOUT_SPI_PIODEV;
#endif
	if (!spiperi_init(&spiperi, pio, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST,
			PINOUT_SPI_SCLK, PINOUT_SPI_COPI, -1/*PINOUT_SPI_CIPO*/, PINOUT_SPI_nCS_IN)) {
		panic("aaa spiperi init failed");
	}

	irq_set_enabled(IO_IRQ_BANK0, false);
	//iprintf("[spictl] adding irq handler 0x%08x\n", 1u<<PINOUT_SPI_nCS_IN);
	gpio_add_raw_irq_handler(PINOUT_SPI_nCS_IN, handle_irq_gpio);
	//iprintf("[spictl] added irq handler\n");
	gpio_set_irq_enabled(PINOUT_SPI_nCS_IN, GPIO_IRQ_EDGE_RISE, true);
	irq_set_enabled(IO_IRQ_BANK0, true);

	spiperi_irq_set_enabled(&spiperi, false);
	spiperi_irq_set_callback(&spiperi, spiperi_isr);
	spiperi_irq_set_enabled(&spiperi, true);

	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_COPI, "SPI bus sniffing COPI"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_SCLK, "SPI bus sniffing SCLK"));
	bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_nCS_IN, "SPI bus sniffing nCS_IN"));
	//bi_decl_if_func_used(bi_1pin_with_name(PINOUT_SPI_CIPO, "SPI bus sniffing CIPO"));
}
void spictl_deinit(void) {
	spiperi_irq_set_enabled(&spiperi, false);
	spiperi_irq_set_callback(&spiperi, NULL);
	spiperi_irq_set_enabled(&spiperi, true);

	irq_set_enabled(IO_IRQ_BANK0, false);
	gpio_remove_raw_irq_handler(PINOUT_SPI_nCS_IN, handle_irq_gpio);
	gpio_set_irq_enabled(PINOUT_SPI_nCS_IN, GPIO_IRQ_EDGE_RISE, false);
	irq_set_enabled(IO_IRQ_BANK0, true);

	spiperi_deinit(&spiperi);

	curarg = byteind = curcmd = reads_since_rst = 0;
}

void spictl_task_init(void) {
#ifdef ENABLE_DBG_PRINT
	if (queue_dbg.element_size == 0) {
		queue_init(&queue_dbg, sizeof(uint32_t), 64);
	}
#endif
	if (queue_com.element_size == 0) {
		msglen = 0;
		queue_init(&queue_com, sizeof(uint8_t), 256);
	}
}
void spictl_task(void) {
#ifdef ENABLE_DBG_PRINT
	if (!queue_is_empty(&queue_dbg)) {
		uint32_t x;
		queue_remove_blocking(&queue_dbg, &x);
		iprintf("[spictl] ==> %08lx\n", x);
	}
#endif

	if (!queue_is_empty(&queue_com)) {
		uint8_t x;
		queue_remove_blocking(&queue_com, &x);

		msgbuf[msglen] = x;
		if (msglen == count_of(msgbuf)-1 || x == '\n') {
			tuc_write(msgbuf, (uint32_t)msglen);
			msgbuf[msglen] = 0;
			if (msgbuf[msglen-1]=='\r') msgbuf[msglen-1]=0;
			iprintf("[spicom] '%s'\n", msgbuf);
			msglen = 0;
		} else ++msglen;
	}
}

