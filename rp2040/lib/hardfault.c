
#include <RP2040.h>
#include <system_RP2040.h>
#include <core_cm0plus.h>

#include <hardware/structs/padsbank0.h>
#include <hardware/structs/iobank0.h>
#include <hardware/structs/dma.h>
#include <hardware/structs/usb.h>
#include <hardware/dma.h>
#include <hardware/gpio.h> /* GPIO_FUNC_xxx */
#include <hardware/resets.h>
#include <hardware/uart.h> /* uart_default */
#include <pico/bootrom.h>
#include <pico/config.h>
#include <pico/time.h>

#include "pinout.h"

#if defined(PICO_DEFAULT_UART_TX_PIN) && defined(uart_default)
extern bool was_in_hardfault;
bool was_in_hardfault = false;

__attribute__((__force_inline__))
static inline void __not_in_flash_func(print_u32)(uint32_t v) {
	for (size_t i = 0; i < 8; ++i, v <<= 4) {
		uint8_t nyb = v >> 28;
		if (nyb < 0xa) nyb += '0';
		else nyb += 'a' - 0xa;

		while (!uart_is_writable(uart_default));
		uart_get_hw(uart_default)->dr = nyb;
	}
}

__attribute__((__force_inline__))
static inline void __not_in_flash_func(prints)(const char* s) {
	for (; *s; ++s) {
		while (!uart_is_writable(uart_default));
		uart_get_hw(uart_default)->dr = *s;
	}
}

__attribute__((__noreturn__, __naked__))
void __not_in_flash_func(isr_hf_stuff)(uintptr_t orig_sp, uint32_t* stackframe) {
	// abort all currently-ongoing DMA xfers
	for (size_t i = 0; i < NUM_DMA_CHANNELS; ++i) {
		dma_channel_abort(i);
	}

	// reset USB so the host doesn't get confused why things are not responding
	hw_clear_alias(usb_hw)->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;

	busy_wait_ms(16); // wait for UART to finish stuff if this is the case

	// reinit UART
	reset_block(PICO_DEFAULT_UART ? RESETS_RESET_UART1_BITS : RESETS_RESET_UART0_BITS);
	unreset_block_wait(PICO_DEFAULT_UART ? RESETS_RESET_UART1_BITS : RESETS_RESET_UART0_BITS);
	uart_set_baudrate(uart_default, PICO_DEFAULT_UART_BAUD_RATE);
	uart_set_format(uart_default, 8, 1, UART_PARITY_NONE); // 8N1
	// enable uart, TX only
	uart_get_hw(uart_default)->cr = UART_UARTCR_UARTEN_BITS | UART_UARTCR_TXE_BITS;
	// init FIFO
	hw_set_bits(&uart_get_hw(uart_default)->lcr_h, UART_UARTLCR_H_FEN_BITS);

	// uart TX pin: enable output, disable irq, pullup, 4ma drive
	hw_write_masked(&padsbank0_hw->io[PICO_DEFAULT_UART_TX_PIN],
		0,//PADS_BANK0_GPIO0_PUE_BITS
		//|(GPIO_DRIVE_STRENGTH_4MA<<PADS_BANK0_GPIO0_DRIVE_LSB),
		PADS_BANK0_GPIO0_IE_BITS|PADS_BANK0_GPIO0_OD_BITS
		//|PADS_BANK0_GPIO0_PUE_BITS|PADS_BANK0_GPIO0_PDE_BITS
		//|PADS_BANK0_GPIO0_DRIVE_BITS
	);
	iobank0_hw->io[PICO_DEFAULT_UART_TX_PIN].ctrl =
		GPIO_FUNC_UART << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

#if PICO_DEFAULT_UART_TX_PIN != PINOUT_UART_TX
	hw_write_masked(&padsbank0_hw->io[PINOUT_UART_TX],
		0,//PADS_BANK0_GPIO0_PUE_BITS
		//|(GPIO_DRIVE_STRENGTH_4MA<<PADS_BANK0_GPIO0_DRIVE_LSB),
		PADS_BANK0_GPIO0_IE_BITS|PADS_BANK0_GPIO0_OD_BITS
		//|PADS_BANK0_GPIO0_PUE_BITS|PADS_BANK0_GPIO0_PDE_BITS
		//|PADS_BANK0_GPIO0_DRIVE_BITS
	);
	iobank0_hw->io[PINOUT_UART_TX].ctrl =
		GPIO_FUNC_UART << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
#endif

	// stackframe format is:
	// r8 r9 r10 r11  r4  r5 r6 r7
	// r0 r1  r2  r3 r12 r14 pc psr
	prints("\r\nHardFault!\r\n");

	static const int lutind[] = {
		0x08, 0x09, 0x0a, 0x0b,
		0x04, 0x05, 0x06, 0x07,
		0x00, 0x01, 0x02, 0x03,
		0x0c, 0x10, 0x0d, 0x0e,
		0x0f, 0x11
	};
	static const char* const nameind[] = {
		"\r\nr0:  ", "\tr1: ", "\tr2:  ", "\tr3:  ",
		"\r\nr4:  ", "\tr5: ", "\tr6:  ", "\tr7:  ",
		"\r\nr8:  ", "\tr9: ", "\tr10: ", "\tr11: ",
		"\r\nr12: ", "\tsp: ", "\tlr:  ", "\tpc:  ",
		"\r\npsr: ", "\tcpu:"
	};

	stackframe[0x10] = orig_sp + 8*sizeof(uint32_t); // cheat a bit. not like we're going to return anyway
	stackframe[0x11] = get_core_num();
	for (size_t i = 0; i < count_of(lutind); ++i) {
		prints(nameind[i]);
		print_u32(stackframe[lutind[i]]);
	}
	prints("\r\n");

#if PICO_NO_FLASH
	for (size_t i = 0; i < 1<<24; ++i) asm volatile("nop");
	rom_reset_usb_boot_fn rst = (rom_reset_usb_boot_fn)rom_func_lookup_inline(ROM_FUNC_RESET_USB_BOOT);
	rst(0, 0);
#else
	while(true);
#endif
}

__attribute__((__used__, __externally_visible__, __noreturn__, __naked__))
void __not_in_flash_func(isr_hardfault)(void) {
	asm volatile(
		// NOTE: r4-r11 (and sp) aren't yet backed up!

		// disable MPU
		"ldr r0, =0xe000ed94\n" // MPU_CTRL
		"mov r1, #0\n"
		"str r1, [r0]\n"
		// disable IRQs
		"cpsid i\n"

		// check for double-fault
		"ldr  r3, =was_in_hardfault\n"
		"ldrb r3, [r3]\n"
		"cmp  r3, #0\n"
		"beq  1f\n"
		"bkpt #0\n" // double-fault
		// set "double-fault' flag
	"1:	 mov r2, #1\n"
		"str r2, [r3]\n"

		// check if system or thread mode
		"mov r0, lr\n"
		"lsr r0, #3\n"
		// get relevant saved stack pointer
		"bcs 1f\n"
		"mrs r0, msp\n"
		"b 2f\n"
	"1:	 mrs r0, psp\n"
	"2:	 \n"

		// run "isr_hf_stuff" in IRQ level 4 context

		// extra registers to pass to fn via stack (old regs from exn)
		"push {r4-r7}\n"
		"mov   r4, r8\n" // sigh...
		"mov   r5, r9\n"
		"mov   r6, r10\n"
		"mov   r7, r11\n"
		"push {r4-r7}\n"
		"mov   r4, sp\n" // save pointer to this for isr_hf_stuff r0

		// upon exn entry, the CPU pushes r0-r3, r12, lr, pc, xPSR to the stack
		// and sets lr to a magic address to return to
		// we can also branch to another magic address with our own stack frame
		// to mimic HF return into another system state (in our case, exn 4)
		"mov   r1, lr\n"
		"ldr   r2, =isr_hf_stuff\n"
		"ldr   r3, =0x01000004\n"
		"push {r0-r3}\n" // r12(sp), lr, pc, xPSR for isr_hf_stuff
		"sub   sp, #8\n"   // r2-r3 for isr_hf_stuff (don't care)
		"push {r0,r4}\n" // r0,r1 for isr_hf_stuff (orig sp, full register backup)
		"ldr   r0, =0xfffffff1\n" // de-escalate to irq4
		"bx    r0\n"

		".pool\n"
	);
	__builtin_unreachable();
}
#else
// isr_hardfault by default goes to a bkpt #0 instruction
#endif


