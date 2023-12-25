
OUTPUT_FORMAT("elf32-littlearm","elf32-bigarm","elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)


MEMORY
{
	IWRAM_HEAP (rwx) : ORIGIN = 0x03ff9800, LENGTH = 0x2c00
	IWRAM_STACK(rwx) : ORIGIN = 0x03ffc400, LENGTH = 0x3800
}

PHDRS
{
	phdr_heap  PT_LOAD AT(0x03ff9800) FLAGS(7);
	phdr_stack PT_LOAD AT(0x03ffc400) FLAGS(7);
}


__irq_flags2 = 0x04000000 - 0x40;
__irq_flags  = 0x04000000 - 0x08;
__irq_vector = 0x04000000 - 0x04;
__und_vector = 0x03ffffdc;

__iwram_stack_origin = 0x03ffc400;
__fakestack_origin = 0x03fffb44;


SECTIONS
{
	/* TODO */
	/*.heap ALIGN(4) : {
		__iwram_heap_start = .;
		*.heap.*(.text* .rodata* .data* .gnu.linkonce.t* .gnu.linkonce.d* .gnu.linkonce.r*)
		. = ALIGN(4);
		__sbss_start = .;
		*.heap.*(.bss* .dynbss* .gnu.linkonce.b* .sbss*)
		. = ALIGN(4);
		__sbss_end = .;
		__iwram_heap_end = .;
	} >IWRAM_HEAP :phdr_heap*/
	.text : {
		__iwram_stack_start = .;
		KEEP (*(.text.startup._start))
		. = ALIGN(4);
		*(.text* .rodata* .data* .gnu.linkonce.t* .gnu.linkonce.d* .gnu.linkonce.r*)
		. = ALIGN(4);
	} >IWRAM_STACK :phdr_stack
	.bss : {
		__bss_start = .;
		*(.bss* .dynbss* .gnu.linkonce.b* .sbss*)
		. = ALIGN(4);
	} >IWRAM_STACK :phdr_stack
	__bss_end = .;
	__bss_size = __bss_end - __bss_start;
	__iwram_stack_end = .;
	__iwram_stack_size = __iwram_stack_end - __iwram_stack_start;

	.fakestack : {
		. = ALIGN(4);
		/* fill value for ARM7 stack, to point to start of exploit code */
		/* looks like GNU ld (bfd) has a fatal internal bug when using a
		 * non-immediate expression in a fill expression... welp */
		/*FILL(__iwram_stack_start);*/
		/*FILL(0x03ffc400);
		. = . + LENGTH(IWRAM_STACK) - (. - __iwram_stack_start);
		__iwram_END = .;*/
		. += (__fakestack_origin - __iwram_stack_origin - __iwram_stack_size);
		KEEP(*(.fakestack))
	} >IWRAM_STACK :phdr_stack
	__iwram_END = .;

	PROVIDE(__stack_sys = 0x03fffc00);
	PROVIDE(__stack_irq = 0x03ffff80);
	PROVIDE(__stack_swi = 0x03ffffc0);

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
	}
}

