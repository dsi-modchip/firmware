
#include "tusb_config.h"
#include <tusb.h>

#include "util.h"
#include "spiflash.h"
#include "boothax.h"
#include "ledctl.h"

#include "dfu.h"


#define BLKSZ 4096
#define MEMSZ (256*1024)


struct altinfo {
	uint32_t offset;
	uint32_t curlen;
	uint32_t maxlen;
	uint32_t crcacc;
	uint16_t curblkacc;
	bool quirk;
};


static uint8_t dl_active = 0, ul_active = 0;
static uint8_t dlbuf[BLKSZ];
static struct altinfo info[DFU_ALT_COUNT];

#define NEEDHDR(x) ((x) >= 1 && (x) < 5)
#define ALT2CS(alt) (((alt)==5)?2:1)
#define CAN_USE_FASTRD(alt) ((alt)!=5)


inline static uint32_t b2u32(const uint8_t* bs) {
	return bs[0] | ((uint32_t)bs[1] << 8) | ((uint32_t)bs[2] << 16)
		| ((uint32_t)bs[3] << 24);
}
inline static uint32_t align_up(uint32_t v, uint32_t al) {
	if (v & (al - 1)) {
		v += al - (v & (al - 1));
	}

	return v;
}


static bool read_hdrinfo(uint8_t alt) {
	struct altinfo* nfo = &info[alt];

	uint8_t boothdr[17];
	spiflash_read(0x2ff, 1, &boothdr[16]);
	iprintf("[DFU] read hdrinfo: [2ff] = %hhx\n", boothdr[16]);
	if (boothdr[16] & 0x80) return false; // NAND boot

	uint32_t off;
	switch (alt) {
	case 1: off = 0x220; break;
	case 2: off = 0x230; break;
	case 3: off = 0x3c0; break;
	case 4: off = 0x3d0; break;
	default: return false;
	}
	spiflash_read(off, 0x10, &boothdr);

	nfo->offset = b2u32(&boothdr[0]);
	nfo->maxlen = align_up(b2u32(&boothdr[12]), BLKSZ);
	iprintf("[DFU] offset = %lx, maxlen = %lx -> %lx\n", nfo->offset,
			b2u32(&boothdr[12]), nfo->maxlen);

	if (nfo->offset != align_up(nfo->offset, BLKSZ)) return false;
	if (nfo->offset + nfo->maxlen >= MEMSZ) return false;

	return true;
}

static bool init_base(uint8_t alt) {
	struct altinfo* nfo = &info[alt];

	nfo->offset = 0;
	nfo->curlen = 0;
	nfo->maxlen = MEMSZ; // 256k by default
	nfo->curblkacc = 0;
	nfo->crcacc = ~(uint32_t)0;

	spiflash_init(ALT2CS(alt));
	spiflash_set_freq(16*1000*1000);

	nfo->quirk = spiflash_pagewrite_is_bugged();
	if (NEEDHDR(alt)) {
		return read_hdrinfo(alt);
	}

	ledctl_mode_set(ledmode_flash_act);
	return true;
}

static bool init_upload(uint8_t alt) {
	if (dl_active & (1u << alt)) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_UNKNOWN);
		return false;
	}

	if (!init_base(alt)) goto err;

	ul_active |= 1u << alt;
	iprintf("[DFU] init upload %hhu\n", alt);
	return true;

err:
	iprintf("[DFU] init upload %hhu FAIL\n", alt);
	spiflash_deinit();
	tud_dfu_finish_flashing(DFU_STATUS_ERR_FILE);
	return false;
}
static void deinit_upload(uint8_t alt) {
	iprintf("[DFU] deinit upload %hhu\n", alt);
	ul_active &= (uint8_t)~(1u << alt);
	ledctl_mode_set(ledmode_flash_idle);
	spiflash_deinit();
}

static bool init_download(uint8_t alt) {
	if (ul_active & (1u << alt)) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_UNKNOWN);
		return false;
	}

	if (!init_base(alt)) goto err;

	dl_active |= 1u << alt;
	iprintf("[DFU] init download %hhu\n", alt);
	return true;

err:
	iprintf("[DFU] init download %hhu FAIL\n", alt);
	spiflash_deinit();
	tud_dfu_finish_flashing(DFU_STATUS_ERR_FILE);
	return false;
}
static void deinit_download(uint8_t alt) {
	dl_active &= (uint8_t)~(1u << alt);
	ledctl_mode_set(ledmode_flash_idle);
	spiflash_deinit();
}
static bool download_do_block(uint8_t alt) {
	struct altinfo* nfo = &info[alt];

	uint32_t addr = nfo->offset + nfo->curlen;
	//iprintf("[DFU] download %lx\n", addr);

	// erase sector
	if (!spiflash_sector_erase(addr)) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_ERASE);
		return false;
	}

	// check for erasure
	uint8_t buf[16];
	spiflash_read_start(addr, CAN_USE_FASTRD(alt));
	for (size_t i = 0; i < BLKSZ; i += sizeof(buf)) {
		spiflash_read_cont(sizeof(buf), buf);
		for (size_t j = 0; j < sizeof(buf); ++j) {
			if (buf[j] != 0xff) {
				tud_dfu_finish_flashing(DFU_STATUS_ERR_CHECK_ERASED);
				return false;
			}
		}
	}

	// accumulate CRC check
	nfo->crcacc = dma_crc32(nfo->crcacc, dlbuf, BLKSZ, false);
	iprintf("[DFU] intermediary CRC @ %06lx = %08lx\n", addr, nfo->crcacc);

	// program
	for (size_t i = 0; i < BLKSZ; i += 256) {
		//iprintf("[DFU] pagewrite %lx\n", addr + i);
		if (!spiflash_page_write(addr + i, &dlbuf[i], nfo->quirk)) {
			tud_dfu_finish_flashing(DFU_STATUS_ERR_PROG);
			return false;
		}
	}

	return true;
}

// ----------------------------------------------------------------------------

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
	if (boothax_mode_current() != boothax_flash) return 0;
	if (DFU_ALT_IS_RDONLY(alt)) return 0;

	switch (state) {
	case DFU_DNBUSY:
		return 100;
	case DFU_MANIFEST:
		return 500;
	default:
		return 0;
	}
}

void tud_dfu_download_cb(uint8_t alt, uint16_t blockno, uint8_t const* data, uint16_t length) {
	if (DFU_ALT_IS_RDONLY(alt)) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_TARGET);
		return;
	}
	if (boothax_mode_current() != boothax_flash) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_FILE);
		return;
	}

	//iprintf("[DFU] download alt %hhu block %hu len %hu\n", alt, blockno, length);

	if (!(dl_active & (1u << alt))) {
		if (blockno == 0) {
			if (!init_download(alt)) return;
		} else tud_dfu_finish_flashing(DFU_STATUS_ERR_UNKNOWN);
	}

	struct altinfo* nfo = &info[alt];

	uint32_t len = length;
	bool fini = false;
	if (nfo->curlen + nfo->curblkacc + len >= nfo->maxlen) {
		len = nfo->maxlen - (nfo->curlen + nfo->curblkacc);
		fini = true;
	}

	// push to current buf
	if (nfo->curblkacc + len < BLKSZ) {
		memcpy(&dlbuf[nfo->curblkacc], data, len);
		nfo->curblkacc += len;
	} else {
		uint32_t can_add = BLKSZ - nfo->curblkacc;
		memcpy(&dlbuf[nfo->curblkacc], data, can_add);

		fini &= download_do_block(alt);
		nfo->curlen += BLKSZ;

		nfo->curblkacc = len - can_add;
		if (nfo->curblkacc) memcpy(dlbuf, data + can_add, nfo->curblkacc);
	}

	if (fini) {
		//deinit_download(alt);
	}

	tud_dfu_finish_flashing(DFU_STATUS_OK);
}

void tud_dfu_manifest_cb(uint8_t alt) {
	if (DFU_ALT_IS_RDONLY(alt)) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_TARGET);
		return;
	}
	if (boothax_mode_current() != boothax_flash) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_FILE);
		return;
	}

	if (!(dl_active & (1u << alt))) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_UNKNOWN);
		return;
	}

	//iprintf("[DFU] manifest alt %hhu\n", alt);

	// get CRC32 really quickly
	struct altinfo* nfo = &info[alt];
	/*spiflash_crc32_next_read(~(uint32_t)0);
	spiflash_read(nfo->offset, nfo->maxlen, NULL, CAN_USE_FASTRD(alt));
	uint32_t verif = spiflash_crc32_get_result();
	iprintf("[DFU] crc from %08lx to %08lx\n", nfo->offset, nfo->maxlen);*/
	uint32_t crc = ~(uint32_t)0;
	spiflash_read_start(nfo->offset, CAN_USE_FASTRD(alt));
	for (size_t i = nfo->offset; i < nfo->maxlen; i += BLKSZ) {
		spiflash_read_cont(BLKSZ, dlbuf);
		crc = dma_crc32(crc, dlbuf, BLKSZ, false);
		//iprintf("[DFU] partial check CRC @ %06x = %08lx\n", i, crc);
	}
	spiflash_read_end();
	uint32_t verif = crc;

	deinit_download(alt);

	iprintf("[DFU] manifest alt %hhu: read=%08lx wrote=%08lx\n",
			alt, verif, nfo->crcacc);

	if (verif != nfo->crcacc) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_VERIFY);
	} else {
		tud_dfu_finish_flashing(DFU_STATUS_OK);
	}
}

uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t blockno, uint8_t* data, uint16_t length) {
	if (boothax_mode_current() != boothax_flash) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_FILE);
		return 0;
	}
	// only allow reading from wifiboard memory if it is actually attached
	if (DFU_ALT_IS_RDONLY(alt) && !(mcenv_get_current() & mcflg_dwm)) {
		tud_dfu_finish_flashing(DFU_STATUS_ERR_TARGET);
		return 0;
	}

	//iprintf("[DFU] upload alt %hhu block %hu len %hu\n", alt, blockno, length);

	if (!(ul_active & (1u << alt))) {
		if (blockno == 0) {
			if (!init_upload(alt)) return 0;
		} else tud_dfu_finish_flashing(DFU_STATUS_ERR_UNKNOWN);
	}

	struct altinfo* nfo = &info[alt];

	bool need_exit = false;
	uint32_t len_todo = length;
	if (nfo->curlen + len_todo > nfo->maxlen) {
		len_todo = nfo->maxlen - nfo->curlen;
		need_exit = true;
	}
	spiflash_read(nfo->offset + nfo->curlen, len_todo, data);
	nfo->curlen += len_todo;

	if (need_exit) deinit_upload(alt);

	return len_todo;
}

void tud_dfu_abort_cb(uint8_t alt) {
	if (boothax_mode_current() != boothax_flash) return;

	iprintf("[DFU] ABORT! %hhu\n", alt);
	if (ul_active & (1u << alt)) deinit_upload  (alt);
	if (dl_active & (1u << alt)) deinit_download(alt);
	ledctl_mode_set(ledmode_error);
}

void tud_dfu_detach_cb(void) {
	iprintf("[DFU] detach\n");
	// nothing to do here...
}

