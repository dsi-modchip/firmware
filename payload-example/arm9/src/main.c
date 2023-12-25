
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#define REG_EXMEMCNT (*(volatile uint16_t*)0x04000204)


__attribute__((__always_inline__))
inline static void bios_memcpy(const void* src, volatile void* dst, size_t len) {
	void (*bios_memcpy_fptr)(const void*, volatile void*, size_t) = 0xffff0808;
	bios_memcpy_fptr(src, dst, len);
}


void itcm_wait_by_loop(int32_t x);
__attribute__((__no_inline__, __naked__, __target__("arm")))
void itcm_wait_by_loop(int32_t x) {
	asm volatile(
	"1:	\n"
		"subs r0, r0, #4\n"
		"bcs 1b\n"
		"bx lr\n"
	:::"r0"
	);
}


void fcram_magic_init_sequence(uintptr_t addr);
__attribute__((__no_inline__, __naked__, __target__("arm")))
void fcram_magic_init_sequence(uintptr_t addr) {
	asm volatile(
		"ldr  r1, =0xffff\n"
		"ldr  r2, =0xffde\n"
		"ldrh r3, [r0, #0]\n"
		"strh r3, [r0, #0]\n"
		"strh r3, [r0, #0]\n"
		"nop\n"
		"ldr  r3, =0xffea\n"
		"strh r1, [r0, #0]\n"
		"strh r2, [r0, #0]\n"
		"strh r3, [r0, #0]\n"
		"bx lr\n"
		".pool\n"
	:::"r1","r2","r3"
	);
}
static void fcram_init(uint32_t thing) {
	itcm_wait_by_loop(0x10000);
	if (REG_EXMEMCNT & 0x2000) return;

	REG_EXMEMCNT = 0x2000;
	itcm_wait_by_loop(0x10000);

	if (thing != 0) {
		fcram_magic_init_sequence(0x02fffffe);
		fcram_magic_init_sequence(0x0dfffffe);
	}

	REG_EXMEMCNT = 0xec8c;
	itcm_wait_by_loop(0x2000);

	fcram_magic_init_sequence(0x02fffffe);
	fcram_magic_init_sequence(0x0dfffffe);
}

#define REG_POWCNT1 (*(volatile uint16_t*)0x04000304)
#define REG_SCFG_EXT9 (*(volatile uint16_t*)0x04004008)
#define REG_DISPCNT (*(volatile uint32_t*)0x04000000)
#define REG_DISPCNT_SUB (*(volatile uint32_t*)0x04001000)
#define REG_DISPSTAT (*(volatile uint16_t*)0x04000004)
#define REG_VRAMCNT_A (*(volatile uint8_t*)0x04000240)
#define REG_VRAMCNT_B (*(volatile uint8_t*)0x04000241)
#define REG_MASTER_BRIGHT (*(volatile uint16_t*)0x0400006c)
#define REG_MASTER_BRIGHT_SUB (*(volatile uint16_t*)0x0400106c)
#define REG_VCOUNT (*(volatile uint16_t*)0x04000006)
#define REG_BG3CNT (*(volatile uint16_t*)0x0400000e)

#define REG_BG3P ((volatile uint16_t*)0x04000030)
#define REG_BG3O ((volatile  int32_t*)0x04000038)

#define BGPALRAM1 ((volatile uint16_t*)0x05000000)
#define BGPALRAM2 ((volatile uint16_t*)0x05000400)


#include "../data/logo.c"



#define C(a,b,c,d,e,f,g,h) ((a)|((b)<<1)|((c)<<2)|((d)<<3)|((e)<<4)|((f)<<5)|((h)<<7)|((h)<<8))


static uint8_t text[] = {
	// U
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(0,1,1,1,1,0,0,0),
	0,
	// N
	C(1,0,0,0,0,1,0,0),
	C(1,1,0,0,0,1,0,0),
	C(1,0,1,0,0,1,0,0),
	C(1,0,0,1,0,1,0,0),
	C(1,0,0,0,1,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	0,
	// L
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,1,1,1,1,1,0,0),
	0,
	// O
	C(0,0,1,1,0,0,0,0),
	C(0,1,0,0,1,0,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(0,1,0,0,1,0,0,0),
	C(0,0,1,1,0,0,0,0),
	0,
	// C
	C(0,0,1,1,1,0,0,0),
	C(0,1,0,0,0,1,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(0,1,0,0,0,1,0,0),
	C(0,0,1,1,1,0,0,0),
	0,
	// K
	C(1,0,0,0,1,0,0,0),
	C(1,0,0,1,0,0,0,0),
	C(1,0,1,0,0,0,0,0),
	C(1,1,1,0,0,0,0,0),
	C(1,0,0,1,0,0,0,0),
	C(1,0,0,0,1,0,0,0),
	C(1,0,0,0,0,1,0,0),
	0,
	// E
	C(1,1,1,1,1,1,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,1,1,1,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,0,0,0,0,0,0,0),
	C(1,1,1,1,1,1,0,0),
	0,
	// D
	C(1,1,1,1,0,0,0,0),
	C(1,0,0,0,1,0,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,0,1,0,0),
	C(1,0,0,0,1,0,0,0),
	C(1,1,1,1,0,0,0,0),
	0,

/*
	C(0,0,0,0,0,0,0,0),
	C(0,0,0,0,0,0,0,0),
	C(0,0,0,0,0,0,0,0),
	C(0,0,0,0,0,0,0,0),
	C(0,0,0,0,0,0,0,0),
	C(0,0,0,0,0,0,0,0),
	C(0,0,0,0,0,0,0,0),
	0,
*/
};


__attribute__((__used__, __externally_visible__, __noreturn__))
int main(void) {
	static const uint8_t magic[16] = {
		0xf0,0x9f,0x8f,0xb3,0xef,0xb8,0x8f,0xe2,
		0x80,0x8d,0xe2,0x9a,0xa7,0xef,0xb8,0x8f
	};

	void (*boot9i_LZ77_close)(void) = 0xffff9558;
	//void (*boot9i_panic_show_err)(uint32_t errcode) = 0xffff9828|1;
	void (*ipc_notifyrecv)(uint32_t id) = 0xffff4c30|1;
	void (*ipc_notifyID)(int id) = 0xffff4ba4|1;
	void (*swi_RLUnCompReadByCallbackWrite16bit)(const void* src, void* dst, void* ud, void* cbs) = 0xffff5204|1;
	void (*swi_RLUnCompReadNormalWrite8bit)(const void* src, void* dst) = 0xffff0568|1;
	void (*bios_memcpy)(const void*, volatile void*, size_t) = 0xffff0808;

	boot9i_LZ77_close();

	// wait for ARM7 hack to complete
	//boot9i_panic_show_err(0xa7);
	ipc_notifyrecv(42);
	// :eyes:
	//boot9i_panic_show_err(0x4acced);

	fcram_init(0x1f);

	bios_memcpy((const void*)0xffff0000, (void*)0x02000000, 65536);
	bios_memcpy(magic, (void*)0x02020000, sizeof(magic));

	ipc_notifyID(43);

	ipc_notifyrecv(44);

	REG_POWCNT1 = 0x0103; // un-powergate the display controllers. also flip displays
	REG_SCFG_EXT9 |= (1u<<31)|(3u<<12)|0x1f; // enable ext LCD,VRAM ; bugfixes in NTR stuff + 8bit VRAM access

	REG_VRAMCNT_A = 0x80; // LCDC
	REG_VRAMCNT_B = 0x81; // main engine 2D
	REG_DISPCNT = (1<<16/*ppu mode*/)|(3<<0/*bg mode 3*/)|(1<<11/*bg3 on*/);
	REG_DISPSTAT = 0; // disable video IRQs
	REG_MASTER_BRIGHT = 0; // reset brightness

	//while (!(REG_DISPSTAT & (1<<6))) asm volatile("nop":::"memory");

	REG_BG3CNT = (1<<7)|(0<<14)|(1<<2);
	volatile uint16_t* bgdata = 0x06000000;

	// unity transform
	REG_BG3P[0] = 0x100;
	REG_BG3P[1] = 0;
	REG_BG3P[2] = 0;
	REG_BG3P[3] = 0x100;
	// 64px offset
	REG_BG3O[0] = -(64<<8);
	REG_BG3O[1] = -(64<<8);

	// load bitmap data into vram
	for (size_t i = 0; i < sizeof(logo37c3Bitmap)/2; ++i) {
		uint16_t c = logo37c3Bitmap[i];
		if (!c) continue;
		bgdata[i] = c|0x8000; // need to set alpha bit manually
	}
	// text font
	for (size_t i = 0; i < sizeof(text); i+=8) {
		for (size_t j = 0; j < 8; ++j) { // char y
			uint8_t cb = text[i+j];
			for (size_t k = 0; k < 8; ++k) { // char x
				uint8_t b = cb & (1<<k);
				if (b) {
					// enlarge 2x
					bgdata[(j*2+0+32)*128 + ((i+k)*2+0)] = 0xffff;
					bgdata[(j*2+1+32)*128 + ((i+k)*2+0)] = 0xffff;
					bgdata[(j*2+0+32)*128 + ((i+k)*2+1)] = 0xffff;
					bgdata[(j*2+1+32)*128 + ((i+k)*2+1)] = 0xffff;
				}
			}
		}
	}

	// rasterbars (beam-raced because i cba to get IRQs to work)
	int32_t bar1_ypos = 24, bar2_ypos = 192 - 24;
	int16_t bar1_dir = 1, bar2_dir = -1;
	for (uint32_t frame = 0; ; ++frame) {
		for (uint16_t line = REG_VCOUNT; ; line = REG_VCOUNT) {
			while (REG_VCOUNT == line) asm volatile("nop":::"memory");

			uint16_t col = 0;
			int32_t d = line - bar1_ypos;
			if (d < 0) d = -d;
			if (d < 8) {
				d^=7;
				col |= (0x1f*d)/8;
			}

			d = line - bar2_ypos;
			if (d < 0) d = -d;
			if (d < 8) {
				d^=7;
				col |= ((0x1f*d)/8)<<5;
			}
			BGPALRAM1[0] = col;

			if (line == 262) break;
		}

		bar1_ypos += bar1_dir;
		if (bar1_ypos == 0 || bar1_ypos == 191) bar1_dir = -bar1_dir;
		bar2_ypos += bar2_dir;
		if (bar2_ypos == 0 || bar2_ypos == 191) bar2_dir = -bar2_dir;
	}

	//boot9i_panic_show_err(0x1337);

	while(1);
	__builtin_unreachable();
}

