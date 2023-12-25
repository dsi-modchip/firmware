
#ifndef SPIFLASH_H_
#define SPIFLASH_H_

#include <stdint.h>
#include <stdbool.h>

#include "serprog.h"


enum spiflash_cmd {
	flashcmd_prog = 0x02, // write (1->0), max 256b
	flashcmd_read = 0x03, // read
	flashcmd_wrdi = 0x04, // write disable
	flashcmd_stat = 0x05, // get status
	flashcmd_wren = 0x06, // write enable
	flashcmd_fast = 0x0b, // fast read
	flashcmd_esec = 0x20, // sector (4k) erase
	flashcmd_rdid = 0x9f, // read jedec id
};

#define SPIFLASH_STAT_WREN 0x02
#define SPIFLASH_STAT_BUSY 0x01


bool spiflash_init(uint8_t sel);
void spiflash_deinit(void);

uint32_t spiflash_set_freq(uint32_t freq_wanted);
enum serprog_flags spiflash_set_flags(enum serprog_flags flags);
uint8_t spiflash_set_bpw(uint8_t bpw);

// ----

void spiflash_cs_deselect(uint8_t csline);
void spiflash_cs_select(uint8_t csline);
void spiflash_op_begin(uint8_t csline);
void spiflash_op_end(uint8_t csline);

void spiflash_op_write(uint32_t wrlen, const void* wrdata);
void spiflash_op_read(uint32_t rdlen, void* rddata);
void spiflash_op_read_write(uint32_t len, void* rddata, const void* wrdata);

// ----

uint8_t spiflash_get_status(void);
void spiflash_read_start(uint32_t addr, bool fast);
void spiflash_read_cont(uint32_t rdlen, void* rddata);
void spiflash_read_end();

#define spiflash_read(addr, rdlen, rddat, ...) do{\
		spiflash_read_start(addr, __VA_OPT__(1 ? (__VA_ARGS__) :) false);\
		spiflash_read_cont(rdlen, rddat); \
		spiflash_read_end(); \
	}while(0)\

void spiflash_crc32_next_read(uint32_t start);
uint32_t spiflash_crc32_get_result(void);

uint32_t spiflash_read_jedec_id(void);

bool spiflash_wren(void);
void spiflash_wrdis(void);
bool spiflash_sector_erase(uint32_t addr);
bool spiflash_page_write(uint32_t addr, const uint8_t data[static 256], bool quirked);

inline static bool spiflash_pagewrite_is_bugged(void) {
	// SST/microchip eeprom/flash chips only support one-byte write commands...
	// this sucks
	return (spiflash_read_jedec_id() & 0xff0000) == 0xbf0000;
}

#endif

