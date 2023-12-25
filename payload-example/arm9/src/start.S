@ vim: set ft=armv5 noet:

	.arch armv5te
	.cpu  arm946e-s

	.extern main
	.extern __stack_sys
	.extern __bss_start
	.extern __bss_end

	.section .text.startup._start, "ax", %progbits

	.arm
	.global _start
	.type   _start, %function
_start:
	@ disable IRQs
	mov r0, #0x04000000
	str r0, [r0, #0x208]  @ REG_IME

	@ disable MPU protection measures and stuff because it makes life easier
	ldr r12, =0xffff84c8  @ cp15_dcache_flush
	blx r12
	ldr r12, =0xffff8678  @ cp15_icache_invalidate
	blx r12
	ldr r12, =0xffff8450  @ cp15_dcache_disable
	blx r12
	ldr r12, =0xffff8638  @ cp15_icache_disable
	blx r12
	ldr r12, =0xffff87c4  @ cp15_mpu_disable
	blx r12

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

