/*
 * Simple linker script for the tasking examples. Grabbed and modified from
 * MegaWiFi repositories, that was grabbed I do not remember from where
 * (maybe from SGDK?).
 *
 * For the tasking experiment, a user stack is needed in addition to the
 * system stack. So we define __ustack in addition to __stack. These will be
 * used in the sega.s startup code to configure the stacks.
 */

OUTPUT_ARCH(m68k)
SEARCH_DIR(.)
__DYNAMIC  =  0;

PROVIDE (_rom_start = 0x00000000);
PROVIDE (_ram_start = 0x00FF0000);

MEMORY
{
    rom : ORIGIN = _rom_start, LENGTH = 0x00400000
    ram : ORIGIN = _ram_start, LENGTH = 0x00010000
}

/*
 * allocate the stacks at the top of memory, since the stack
 * grows down
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
    *(.data.dirty)
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
