.section .text.boot

*-------------------------------------------------------
*
*       Sega startup code for the GNU Assembler
*       Translated from:
*       Sega startup code for the Sozobon C compiler
*       Written by Paul W. Lee
*       Modified by Charles Coty
*       Modified by Stephane Dallongeville
*       Massaged and trimmed by Jesus Alonso (@doragasu)
*
*-------------------------------------------------------

        .org    0x00000000

_Start_Of_Rom:
_Vecteurs_68K:
        dc.l    0x00FFFE00              /* Stack address */
        dc.l    _Entry_Point            /* Program start address */
        dc.l    _Bus_Error
        dc.l    _Address_Error
        dc.l    _Illegal_Instruction
        dc.l    _Zero_Divide
        dc.l    _Chk_Instruction
        dc.l    _Trapv_Instruction
        dc.l    _Privilege_Violation
        dc.l    _Trace
        dc.l    _Line_1010_Emulation
        dc.l    _Line_1111_Emulation
        dc.l     _Error_Exception, _Error_Exception, _Error_Exception, _Error_Exception
        dc.l     _Error_Exception, _Error_Exception, _Error_Exception, _Error_Exception
        dc.l     _Error_Exception, _Error_Exception, _Error_Exception, _Error_Exception
        dc.l    _Error_Exception, _INT, _EXTINT, _INT
        dc.l    _HINT
        dc.l    _INT
        dc.l    _VINT
        dc.l    _INT
        dc.l    _INT,_INT,_INT,_INT,_INT,_INT,_INT,_INT
        dc.l    _INT,_INT,_INT,_INT,_INT,_INT,_INT,_INT
        dc.l    _INT,_INT,_INT,_INT,_INT,_INT,_INT,_INT
        dc.l    _INT,_INT,_INT,_INT,_INT,_INT,_INT,_INT

        .incbin "boot/rom_head.bin", 0, 0x100

_Entry_Point:
        move    #0x2700,%sr
        lea     Table,%a5
        movem.w (%a5)+,%d5-%d7
        movem.l (%a5)+,%a0-%a4
* Check Version Number
        move.b  -0x10ff(%a1),%d0
        andi.b  #0x0f,%d0
        beq.s   WrongVersion
* Sega Security Code (SEGA)
        move.l  #0x53454741,0x2f00(%a1)
WrongVersion:
*        move.w  (%a4),%d0
* Put Z80 and YM2612 in reset (note: might not be enough cycles)
        moveq   #0x00,%d0
		move.b	%d0,(%a2)
		jmp		Continue

Table:
        dc.w    0x8000,0x3fff,0x0100
        dc.l    0xA00000,0xA11100,0xA11200,0xC00000,0xC00004
		dc.b	0x9F,0xBF,0xDF,0xFF

Continue:
* Set stack pointer, release Z80 from reset and request bus
		move	%d0,%sp
        move.w  %d7,(%a1)
        move.w  %d7,(%a2)

* Mute PSG
		moveq	#3,%d1
PsgMute:
		move.b	(%a5)+,0x11(%a3)
		dbf		%d1, PsgMute

* clear Genesis RAM
        lea     0xff0000,%a0
        move.w  #0x3FFF,%d1

ClearRam:
        move.l  %d0,(%a0)+
        dbra    %d1,ClearRam

* Copy initialized variables from ROM to Work RAM
        lea     _etext,%a0
        lea     0xFF0000,%a1
        move.l  #_sdata,%d0
        lsr.l   #1,%d0
        beq     2f

        subq.w  #1,%d0
1:
        move.w  (%a0)+,(%a1)+
        dbra    %d0,1b

* Copy flash routines to RAM
2:
		* a0 already contains the load address of the routines
*		lea		_lflash,%a0
		lea		_oflash,%a1
		move.l	#_sflash,%d0
		lsr.l	#1,%d0
		beq 	3f

		subq.w	#1,%d0
1:
		move.w	(%a0)+,(%a1)+
		dbra	%d0,1b

* Jump to initialisation process...
3:
        move.w  #0,%a7
        jmp     _start_entry

* If main returns, soft-reboot machine
	jmp 	_Entry_Point


*------------------------------------------------
*
*       interrupt functions
*
*------------------------------------------------

* Interrupts are not supported. So provide a minimal interrupt handler
* that just loops forever if the interrupt occurs.
_Bus_Error:
_Address_Error:
_Illegal_Instruction:
_Zero_Divide:
_Chk_Instruction:
_Trapv_Instruction:
_Privilege_Violation:
_Trace:
_Line_1010_Emulation:
_Line_1111_Emulation:
_Error_Exception:
_INT:
_EXTINT:
_HINT:
_VINT:
	jmp _Bus_Error

*------------------------------------------------
*
* Copyright (c) 1988 by Sozobon, Limited.  Author: Johann Ruegg
*
* Permission is granted to anyone to use this software for any purpose
* on any computer system, and to redistribute it freely, with the
* following restrictions:
* 1) No charge may be made other than reasonable charges for reproduction.
* 2) Modified versions must be clearly marked as such.
* 3) The authors are not responsible for any harmful consequences
*    of using this software, even if they result from defects in it.
*
*------------------------------------------------

*ldiv:
*        move.l  4(%a7),%d0
*        bpl     ld1
*        neg.l   %d0
*ld1:
*        move.l  8(%a7),%d1
*        bpl     ld2
*        neg.l   %d1
*        eor.b   #0x80,4(%a7)
*ld2:
*        bsr     i_ldiv          /* d0 = d0/d1 */
*        tst.b   4(%a7)
*        bpl     ld3
*        neg.l   %d0
*ld3:
*        rts
*
*lmul:
*        move.l  4(%a7),%d0
*        bpl     lm1
*        neg.l   %d0
*lm1:
*        move.l  8(%a7),%d1
*        bpl     lm2
*        neg.l   %d1
*        eor.b   #0x80,4(%a7)
*lm2:
*        bsr     i_lmul          /* d0 = d0*d1 */
*        tst.b   4(%a7)
*        bpl     lm3
*        neg.l   %d0
*lm3:
*        rts
*
*lrem:
*        move.l  4(%a7),%d0
*        bpl     lr1
*        neg.l   %d0
*lr1:
*        move.l  8(%a7),%d1
*        bpl     lr2
*        neg.l   %d1
*lr2:
*        bsr     i_ldiv          /* d1 = d0%d1 */
*        move.l  %d1,%d0
*        tst.b   4(%a7)
*        bpl     lr3
*        neg.l   %d0
*lr3:
*        rts
*
*ldivu:
*        move.l  4(%a7),%d0
*        move.l  8(%a7),%d1
*        bsr     i_ldiv
*        rts
*
*lmulu:
*        move.l  4(%a7),%d0
*        move.l  8(%a7),%d1
*        bsr     i_lmul
*        rts
*
*lremu:
*        move.l  4(%a7),%d0
*        move.l  8(%a7),%d1
*        bsr     i_ldiv
*        move.l  %d1,%d0
*        rts
**
** A in d0, B in d1, return A*B in d0
**
*i_lmul:
*        move.l  %d3,%a2           /* save d3 */
*        move.w  %d1,%d2
*        mulu    %d0,%d2           /* d2 = Al * Bl */
*
*        move.l  %d1,%d3
*        swap    %d3
*        mulu    %d0,%d3           /* d3 = Al * Bh */
*
*        swap    %d0
*        mulu    %d1,%d0           /* d0 = Ah * Bl */
*
*        add.l   %d3,%d0           /* d0 = (Ah*Bl + Al*Bh) */
*        swap    %d0
*        clr.w   %d0               /* d0 = (Ah*Bl + Al*Bh) << 16 */
*
*        add.l   %d2,%d0           /* d0 = A*B */
*        move.l  %a2,%d3           /* restore d3 */
*        rts
**
**A in d0, B in d1, return A/B in d0, A%B in d1
**
*i_ldiv:
*        tst.l   %d1
*        bne     nz1
*
**       divide by zero
**       divu    #0,%d0            /* cause trap */
*        move.l  #0x80000000,%d0
*        move.l  %d0,%d1
*        rts
*nz1:
*        move.l  %d3,%a2           /* save d3 */
*        cmp.l   %d1,%d0
*        bhi     norm
*        beq     is1
**       A<B, so ret 0, rem A
*        move.l  %d0,%d1
*        clr.l   %d0
*        move.l  %a2,%d3           /* restore d3 */
*        rts
**       A==B, so ret 1, rem 0
*is1:
*        moveq.l #1,%d0
*        clr.l   %d1
*        move.l  %a2,%d3           /* restore d3 */
*        rts
**       A>B and B is not 0
*norm:
*        cmp.l   #1,%d1
*        bne     not1
**       B==1, so ret A, rem 0
*        clr.l   %d1
*        move.l  %a2,%d3           /* restore d3 */
*        rts
**  check for A short (implies B short also)
*not1:
*        cmp.l   #0xffff,%d0
*        bhi     slow
**  A short and B short -- use 'divu'
*        divu    %d1,%d0           /* d0 = REM:ANS */
*        swap    %d0               /* d0 = ANS:REM */
*        clr.l   %d1
*        move.w  %d0,%d1           /* d1 = REM */
*        clr.w   %d0
*        swap    %d0
*        move.l  %a2,%d3           /* restore d3 */
*        rts
** check for B short
*slow:
*        cmp.l   #0xffff,%d1
*        bhi     slower
** A long and B short -- use special stuff from gnu
*        move.l  %d0,%d2
*        clr.w   %d2
*        swap    %d2
*        divu    %d1,%d2           /* d2 = REM:ANS of Ahi/B */
*        clr.l   %d3
*        move.w  %d2,%d3           /* d3 = Ahi/B */
*        swap    %d3
*
*        move.w  %d0,%d2           /* d2 = REM << 16 + Alo */
*        divu    %d1,%d2           /* d2 = REM:ANS of stuff/B */
*
*        move.l  %d2,%d1
*        clr.w   %d1
*        swap    %d1               /* d1 = REM */
*
*        clr.l   %d0
*        move.w  %d2,%d0
*        add.l   %d3,%d0           /* d0 = ANS */
*        move.l  %a2,%d3           /* restore d3 */
*        rts
**       A>B, B > 1
*slower:
*        move.l  #1,%d2
*        clr.l   %d3
*moreadj:
*        cmp.l   %d0,%d1
*        bhs     adj
*        add.l   %d2,%d2
*        add.l   %d1,%d1
*        bpl     moreadj
** we shifted B until its >A or sign bit set
** we shifted #1 (d2) along with it
*adj:
*        cmp.l   %d0,%d1
*        bhi     ltuns
*        or.l    %d2,%d3
*        sub.l   %d1,%d0
*ltuns:
*        lsr.l   #1,%d1
*        lsr.l   #1,%d2
*        bne     adj
** d3=answer, d0=rem
*        move.l  %d0,%d1
*        move.l  %d3,%d0
*        move.l  %a2,%d3           /* restore d3 */
*        rts

	.align 2
	.globl BootAddr
	.type	BootAddr, @function
BootAddr:
    move    #0x2700,sr              /* disable interrupts */
* Get boot address
	move.l 4(%sp),%d2

* De-initialize pads. This is necessary for some games like uwol,
* that skip initialization if they detect pads initialized.
	moveq	#0x00,%d0
	lea 	0xA10008,%a0
	move.w	%d0,(%a0)

* Release Z80 from reset
	moveq #1, %d1
	lea 0xA11100, %a1
	move.b %d1, 0x100(%a1)
* Request Z80 bus
	move.b %d1, (%a1)
* Wait until bus is granted
1:
	move.b (%a1), %d1
	beq 1b

* Clear Z80 RAM
	lea 0xA00000, %a2
	move #0x1FFF, %d3
2:	
	move.b %d0, (a2)+
	dbra %d3, 2b

* Set Z80 reset
	move.b %d0, 0x100(%a1)

* Release Z80 bus
	move.b %d0, (%a1)

* Clear WRAM
	move.w  #0x3FFF,%d1
	lea     0xff0000,%a0
WRamClear:
	move.l  %d0,(a0)+
	dbra    %d1,WRamClear

* Set default stack pointer
	move.l	%d0,%a0
	move.l	(%a0),%sp

* Boot from entry point
	move.l %d2,%a0

*	move.l	%d0,%d1
*	move.l	%d0,%d2
*	move.l	%d0,%d3
*	move.l	%d0,%d4
*	move.l	%d0,%d5
*	move.l	%d0,%d6
*	move.l	%d0,%d7
*
*	move.l	%d0,%a1
*	move.l	%d0,%a2
*	move.l	%d0,%a3
*	move.l	%d0,%a4
*	move.l	%d0,%a5
*	move.l	%d0,%a6

	jmp (%a0)
	.size	BootAddr, .-BootAddr
