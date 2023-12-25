OUTPUT_FORMAT("elf32-littlearm","elf32-bigarm","elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)


MEMORY
{
	ITCM(rwx) : ORIGIN = 0x01ff8000, LENGTH = 0x8000
	DTCM(rwx) : ORIGIN = 0x02ff9200, LENGTH = 0x2dc0
}

PHDRS
{
	/*phdr_itcm PT_LOAD AT(0x01ff8000) FLAGS(7);*/
	phdr_dtcm PT_LOAD AT(0x02ff9200) FLAGS(7);
}


__irq_flags  = 0x02ff8000 + 0x3ffc;
__irq_vector = 0x02ff8000 + 0x3ff8;
__und_vector = 0x02fffd9c;


__dtcm_origin = 0x02ff9200;
__ropchain_start = 0x02ffbb6c;


SECTIONS
{
	.startup : {
		__itcm_start = .;
		KEEP (*(.text.startup._start))
		*(.text.startup* .text.entry*)
		. = ALIGN(4);
	} >ITCM AT>DTCM :phdr_dtcm

	__vectors_start = .;
	.vectors : {
		KEEP (*(.vectors))
		. = ALIGN(4);
	} >ITCM AT>DTCM :phdr_dtcm
	__vectors_end = .;

	.text : {
		*(.text* .gnu.linkonce.t* .glue_7* .v4_bx)
		. = ALIGN(4);
	} >ITCM AT>DTCM :phdr_dtcm
	__itcm_end = .;
	__itcm_size = (__itcm_end - __itcm_start);
	__itcm_size_4 = __itcm_size >> 2;
	__itcm_lma = LOADADDR(.startup);

	.data : {
		__data_start = .;
		*(.rodata* .data* .gnu.linkonce.d* .gnu.linkonce.r*)
		. = ALIGN(4);
	} >DTCM :phdr_dtcm
	__data_end = .;
	__bss_start = .;
	.bss : {
		*(.bss* .dynbss* .gnu.linkonce.b* .sbss*)
		. = ALIGN(4);
	} >DTCM :phdr_dtcm
	__bss_end = .;
	__bss_size = (__bss_end - __bss_start);
	__bss_size_4 = __bss_size >> 2;

	__dtcm_end = .;
	__dtcm_size = (__dtcm_end - __dtcm_origin);
	__dtcm_size_4 = __dtcm_size >> 2;

	.ropchain : {
		. = ALIGN(4);
		/* fill value for ARM9 stack, to point to start of exploit code */
		/* looks like GNU ld (bfd) has a fatal internal bug when using a
		 * non-immediate expression in a fill expression... welp */
		/*FILL(__dtcm_start);*/
		/* actually no here we just need some random nonsense fill, until we
		 * get to the actual ROP chain data */
		/*FILL(0xdeadbeef);*/
		. += (__ropchain_start - __dtcm_origin - __dtcm_size);
		KEEP(*(.ropchain))
	}>DTCM :phdr_dtcm
	__dtcm_END = .;

	PROVIDE(__stack_sys = 0x02ffbc00);
	PROVIDE(__stack_irq = 0x02ffbf80);
	PROVIDE(__stack_swi = 0x02ffbfc0);

	/DISCARD/ : {
		*(.comment*)
		*(.stab*)
		*(.debug*)
		*(.line*)
		*(.gcc*)
		*(.eh_frame*)
		*(.jcr*)
		*(.got*)
		*(.plt*)
		*(.dtors*)
		*(.ctors*)
		*(.fini_array*)
		*(.init_array*)
		*(.preinit_array*)
		*(.ARM*)
		*(.COMMON*)
		*(.vfp11_veneer*)
		*(.iplt*)
		*(.rel.*)
	}
}

