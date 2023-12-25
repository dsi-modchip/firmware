@ vim: set ft=armv4 noet:

	.arch armv4t
	.cpu  arm7tdmi

	.extern main
	.extern __stack_sys
	.extern __bss_start
	.extern __bss_end

	.section .text.startup._start, "ax", %progbits

	@ don't know what mode we're executing at, so let's try something
	.arm
	.global _start
	.type   _start, %function
_start:
	@ disable IRQs
	mov r0, #0x04000000
	str r0, [r0, #0x208]  @ REG_IME

	@ reset stack pointer
	ldr sp, =__stack_sys

	@ clear bss
	mov r0, #0
	ldr r1, =__bss_start
	ldr r2, =__bss_end
	@ both are aligned to 4 bytes so we can use 32-bit writes here
1:	cmp r1, r2
	beq 2f
3:	str r0, [r1], #4
	cmp r1, r2
	bne 3b
2:

	ldr r0, =main
	adr lr, 1f
	bx  r0

1:	b 1b

	.pool

