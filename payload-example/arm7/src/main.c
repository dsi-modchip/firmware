
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#define REG_EXMEMSTAT (*(volatile uint16_t*)0x04000204)


inline static char hex2nyb(uint8_t v) {
	v &= 0xf;
	if (v < 0xa) v = v - 0 + '0';
	else v = v - 0xa + 'a';
	return v;
}


__attribute__((__always_inline__))
inline static void bios_memcpy(const void* src, volatile void* dst, size_t len) {
	void (*bios_memcpy_fptr)(const void*, volatile void*, size_t) = 0x10e0;
	bios_memcpy_fptr(src, dst, len);
}


void swi_wait_by_loop(int32_t count);
__attribute__((__target__("arm"), __naked__))
void swi_wait_by_loop(int32_t count) {
	asm volatile(
		"swi 0x030000\n"
		"bx lr\n"
		:::"r0"
	);
}


#define REG_I2CDATA (*(volatile uint8_t*)0x04004500)
#define REG_I2CCNT  (*(volatile uint8_t*)0x04004501)

#define wait_busy() do{while(REG_I2CCNT&0x80);}while(0)
#define get_result() ({wait_busy();(REG_I2CCNT>>4)&1;})
#define delay() do{wait_busy();for (int iii = 0; iii < del; ++iii) asm volatile("nop");}while(0)
#define stop(x) do{if(del){REG_I2CCNT = ((x)<<5)|0xc0;delay();REG_I2CCNT=0xc5;}else REG_I2CCNT = ((x)<<5)|0xc1;}while(0)

static void i2c_write(int dev, int reg, uint8_t data) {
	int del = (dev == 0x4A) ? 0x180 : 0;

	for (int i = 0; i < 8; ++i) {
		// select device
		wait_busy();
		REG_I2CDATA = dev;
		REG_I2CCNT = 0xc2;
		if (get_result()) {
			// select register
			delay();
			REG_I2CDATA = reg;
			REG_I2CCNT = 0xc0;
			if (get_result()) {
				delay();
				REG_I2CDATA = data;
				stop(0);
				if (get_result()) return;
			}
		}

		REG_I2CCNT = 0xc5;
	}
}
static void i2c_write_n(int dev, int reg, size_t n, const uint8_t* data) {
	int del = (dev == 0x4A) ? 0x180 : 0;

	for (int i = 0; i < 8; ++i) {
		// select device
		wait_busy();
		REG_I2CDATA = dev;
		REG_I2CCNT = 0xc2; // ie+do, start
		if (get_result()) {
			// select register
			delay();
			REG_I2CDATA = reg;
			REG_I2CCNT = 0xc0; // ie+do
			for (size_t i = 0; i < n; ++i) {
				if (get_result()) {
					delay();
					REG_I2CDATA = data[i];

					if (i == n - 1) {
						stop(0);
						if (get_result()) return;
					} else {
						REG_I2CCNT = 0xc0; // ie+do
					}
				}
			}
		}

		REG_I2CCNT = 0xc5;
	}
}
static int i2c_read(int dev, int reg) {
	int del = (dev == 0x4A) ? 0x180 : 0;

	for (int i = 0; i < 8; ++i) {
		// select device
		wait_busy();
		REG_I2CDATA = dev;
		REG_I2CCNT = 0xc2;
		if (get_result()) {
			// select register
			delay();
			REG_I2CDATA = reg;
			REG_I2CCNT = 0xc0;
			if (get_result()) {
				delay();
				// select device again?
				REG_I2CDATA = dev|1;
				REG_I2CCNT = 0xc2;
				if (get_result()) {
					delay();
					stop(1);
					wait_busy();
					return REG_I2CDATA;
				}
			}
		}

		REG_I2CCNT = 0xc5;
	}

	return -1;
}

#undef stop
#undef delay
#undef get_result
#undef wait_busy


#define REG_SPICNT  (*(volatile uint16_t*)0x040001c0)
#define REG_SPIDATA (*(volatile uint8_t *)0x040001c2)

static void spi_init(void) {
	// pwman device, 1 MHz, enable, hold CS // FIXME
	REG_SPICNT = (2<<0) | (0<<8) | (0<<10) | (1<<11) | (1<<15);
}

static uint8_t spi_xfer_one(uint8_t v, bool start, bool stop) {
	while (REG_SPICNT & (1<<7)) ; // wait until ready

	//if (start || stop)
		REG_SPICNT = (2<<0/*1 MHz*/) | (1<<8/*CS2*/) | ((stop?0:1)<<11) | (1<<15/*enable*/);

	REG_SPIDATA = v; // register 1 (battery), read (bit 7) // FIXME
	while (REG_SPICNT & (1<<7)) ; // wait until ready
	v = REG_SPIDATA;

	return v;
}


static uint8_t pmic_xfer(uint8_t regno, uint8_t writeval, bool read) {
	while (REG_SPICNT & (1<<7)) ; // wait until ready

	REG_SPICNT = (2<<0/*1 MHz*/) | (0<<8/*CS1*/) | (1<<11/*CS hold*/) | (1<<15/*enable*/);

	REG_SPIDATA = (regno & 0x7f) | (read ? 0x80 : 0);
	while (REG_SPICNT & (1<<7)) ; // wait until ready

	REG_SPICNT = (2<<0/*1 MHz*/) | (0<<8/*CS1*/) | (0<<11/*CS hold*/) | (1<<15/*enable*/);
	REG_SPIDATA = writeval;
	while (REG_SPICNT & (1<<7)) ; // wait until ready
	return REG_SPIDATA;
}
static inline uint8_t pmic_read(uint8_t regno) {
	return pmic_xfer(regno, 0, true);
}
static inline uint8_t pmic_write(uint8_t regno, uint8_t v) {
	return pmic_xfer(regno, v, false);
}


static void spi_write(const void* payload, size_t n, bool start, bool stop) {
	const uint8_t* pl = payload;
	for (size_t i = 0; i < n; ++i) {
		spi_xfer_one(pl[i], start && (i == 0), stop && (i == (n - 1)));
	}
}
static void spi_read(uint8_t* pl, size_t n, bool start, bool stop) {
	for (size_t i = 0; i < n; ++i) {
		pl[i] = spi_xfer_one(0xff, start && (i == 0), stop && (i == (n - 1)));
	}
}
static void spi_rdwr(const uint8_t* wrdat, uint8_t* rddat, size_t n, bool start, bool stop) {
	for (size_t i = 0; i < n; ++i) {
		rddat[i] = spi_xfer_one(wrdat[i], start && (i == 0), stop && (i == (n - 1)));
	}
}
static void spi_send(const void* payload, size_t len_bytes) {
	spi_xfer_one(0x05, true, false);
	spi_xfer_one('P', false, false);
	spi_write(payload, len_bytes, false, true);
}
static void spi_print(const char* s) {
	spi_xfer_one(0x05, true, false);
	spi_xfer_one('P', false, false);
	for (; *s; ++s) {
		char c = *s;

		spi_xfer_one(c, false, (s[1]==0));
	}
}
static void spi_hexdump(const void* data, size_t size) {
	// do it in chunks of 4 bytes because bus size bs
	char bleh[12+1];
	bleh[12] = 0;
	for (size_t i = 0; i < size; i += 4) {
		uint32_t v = ((const uint32_t*)data)[i>>2];
		bleh[ 0] = hex2nyb(v>> 4);
		bleh[ 1] = hex2nyb(v>> 0);
		bleh[ 2] = ' ';
		bleh[ 3] = hex2nyb(v>>12);
		bleh[ 4] = hex2nyb(v>> 8);
		bleh[ 5] = ' ';
		bleh[ 6] = hex2nyb(v>>20);
		bleh[ 7] = hex2nyb(v>>16);
		bleh[ 8] = ' ';
		bleh[ 9] = hex2nyb(v>>28);
		bleh[10] = hex2nyb(v>>24);
		bleh[11] = ' ';

		size_t todo = 12;
		if (size - i < 4) {
			todo = 3*(size - i);
			bleh[todo] = 0;
		}
		spi_send(bleh, todo);

		if (((i + 4) & 0xf) == 0 || size - i <= 4) { // split at line, *or* final one to do
			spi_send("\r\n", 2);
		}
	}
}


#define REG_GPIO_DATA (*(volatile uint8_t*)0x04004c00)
#define REG_GPIO_DIR  (*(volatile uint8_t*)0x04004c01)
#define REG_GPIO_EDGE (*(volatile uint8_t*)0x04004c02)
#define REG_GPIO_IE   (*(volatile uint8_t*)0x04004c03)

static void gpio_init(void) {
	// disable interrupts
	REG_GPIO_EDGE = 0xff;
	REG_GPIO_IE   = 0;

	REG_GPIO_DATA = 0xff;
	REG_GPIO_DIR = 7|(1<<4)|(1<<7); // enable all GPIO18, GPIO330, GPIO333 as writeable
}

static inline void gpio_write_330(bool value) {
	REG_GPIO_DATA = (0xff^(1<<4)) | ((value?1:0)<<4);
}


__attribute__((__used__, __externally_visible__, __noreturn__))
int main(void) {
	void (*ipc_notifyID)(int id) = 0x55f8|1;
	uint32_t (*ipc_notifyrecv)(void) = 0x5608|1;

	ipc_notifyID(42);

	spi_print("*hacker voice* im in\r\n");
	gpio_init();
	gpio_write_330(false);
	swi_wait_by_loop(0x20ba/10);
	gpio_write_330(true);
	swi_wait_by_loop(0x20ba/10);
	gpio_write_330(false);

	// NOTE: this hangs sometimes. so only notify the glitch controller AFTER
	//       this has succeeded (FIXME why does it hang??)
	while (!(REG_EXMEMSTAT & 0x4000)) asm volatile("":::"memory");
	spi_print("fcram inited\r\n");

	const uint8_t v[] = {0x05,0x13,0x37,0x42};
	spi_write(v, sizeof(v), true, true);

	while (ipc_notifyrecv() != 43) asm volatile("":::"memory");
	i2c_write(0x4a, 0x31, 1); // set camled high

	// enable LCD power
	//pmic_write(0, 0x0c);
	pmic_write(0x10, 0xc);
	// enable LCD backlights
	//pmic_write(4, 0x03); // max
	//i2c_write(0x4a, 0x41, 0x04); // max

	ipc_notifyID(44);

	bios_memcpy((const void*)0x0000, (void*)0x02010000, 0x10000);
	spi_print("magic:\r\n");
	spi_hexdump((const void*)0x02020000, 0x10);
	spi_print("ARM9i:\r\n");
	spi_hexdump((const void*)0x02008000, 0x20);
	spi_print("ARM7i:\r\n");
	spi_hexdump((const void*)0x02018000, 0x20);

	i2c_write(0x4a, 0x31, 0); // set camled low again (doesn't get reset by BPTWL)

	while(1);
	__builtin_unreachable();
}

