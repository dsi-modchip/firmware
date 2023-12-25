@ vim: set ft=armv4 noet:

	.arch armv4t
	.cpu  arm7tdmi

	.extern _start

	.section .fakestack, "aw", %progbits

	.arm
	.global fakestack
	.type   fakestack, %function
fakestack:
	@ sp is 0x03fffb44 at pop instruction at 0x9790
	@ registers r4-r8 and lr are popped, lr is immediately branched to

	@ so, add some filler for r4-r8 (20 bytes)
	.byte 0x0a
	.byte 0xf0,0x9f,0x8f,0xb3,0xef,0xb8,0x8f,0xe2,0x80,0x8d,0xe2,0x9a,0xa7,0xef,0xb8,0x8f
	.byte 0x0a,0x00,0x00

	@ now, the return address, i.e. the payload code
	.4byte _start

fakestack_end:
