/* Linker script for the wflash bootloader.
 *
 * The bootloader has a 512 byte header on top of the cartridge (vectors +
 * cartridge header) and a boot sector at the bottom of the 32 Mbit
 * ROM. All the space inbetween is unused.
 *
 * Some code and read-only data from flash module must be copied to RAM
 * to run from there. This is required because these functions handle flash
 * erase and program routines. And while these routines are in progress, the
 * CPU cannot read code/data from ROM/Flash.
 *
 * The flash sector at RAM is placed at the end of the .bss section. The
 * copy from Flash to RAM can be done at startup (by the sega.s code), or
 * alternatively if you want to be able to reuse this chunk of RAM (e.g.
 * for dynamic allocation), the data can be copied dynamically when needed.
 * 
 */

OUTPUT_ARCH(m68k)
SEARCH_DIR(.)
__DYNAMIC  =  0;

MEMORY
{
	rom : ORIGIN = 0x00000000, LENGTH = 0x00400000
	ram : ORIGIN = 0x00FF0000, LENGTH = 0x00010000
}

/*
 * allocate the stack at the top of memory, since the stack
 * grows down
 */

PROVIDE (__stack = 0x01000000);


SECTIONS
{
  /* Code and read-only data */
  .text 0x000000 :
  {
	/* Make sure boot sector is not discarded by the linker */
	KEEP(*(.text.boot)) *(.text .text.*)
	. = ALIGN(4);
    *(.rodata .rodata.*)

    . = ALIGN(0x4);
    __INIT_SECTION__ = . ;
    *(.init)
    SHORT (0x4E75)	/* rts */

    __FINI_SECTION__ = . ;
    *(.fini)
    SHORT (0x4E75)	/* rts */

    _etext = .;
  } > rom
  _stext = SIZEOF(.text);

  .data 0xFF0000 :
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
  /* Code and rodata needed for the flash read/write routines */
  .flash _ebss :
  AT ( LOADADDR(.data) + _sdata )
  {
	_oflash = . ;
    *(.flash .flash.*)
    _eflash = .;
  } > ram
  _sflash = SIZEOF(.flash);
  _lflash = LOADADDR(.flash);

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
