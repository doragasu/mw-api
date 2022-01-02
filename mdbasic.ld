/*
 * Simple linker script supporting multitasking. To do that, two stacks are
 * defined (__ustack and __stack). These symbols are e used in sega.s to set
 * up the stacks.
 */

OUTPUT_ARCH(m68k)
SEARCH_DIR(.)
__DYNAMIC  =  0;

PROVIDE (_rom_start = 0x00000000);
PROVIDE (_ram_start = 0x00FF0000);

MEMORY
{
    rom (rx) : ORIGIN = _rom_start, LENGTH = 0x00400000
    ram : ORIGIN = _ram_start, LENGTH = 0x00010000
}

/*
 * allocate the stacks at the top of memory, since the stack
 * grows down on m68k
 */

PROVIDE (__ustack = 0x01000000); /* user stack, 512 bytes */
PROVIDE (__stack = 0x00FFFE00);  /* privileged stack */


SECTIONS
{
  /* Code and read-only data */
  .text _rom_start :
  {
    /* Make sure boot sector is not discarded by the linker */
    KEEP(*(.text.boot)) *(.text .text.*)
    . = ALIGN(4);
    *(.rodata .rodata.*)

    . = ALIGN(0x4);
    __INIT_SECTION__ = . ;
    *(.init)
    SHORT (0x4E75)    /* rts */

    __FINI_SECTION__ = . ;
    *(.fini)
    SHORT (0x4E75)    /* rts */

    _etext = .;
  } > rom
  _stext = SIZEOF(.text);
  _rom_end = .;

  .data _ram_start :
  AT ( ADDR (.text) + _stext )
  {
    *(.data .data.*)
    . = ALIGN(0x4);
    _edata = . ;
  } > ram
  _sdata = SIZEOF(.data);

  .bss _edata :
  {
    _obss = . ;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(0x4);
    _ebss = . ;
  } > ram

  .stab 0 (NOLOAD) :
  {
    *(.stab)
  }

  .stabstr 0 (NOLOAD) :
  {
    *(.stabstr)
  }

  .eh_frame 0 (NOLOAD) :
  {
    *(.eh_frame)
  }
}
