/****************************************************************************
 * Startup code. We put it in its own .text.boot to make sure the linker
 * garbage collection routines do not discard it (see the KEEP() directive
 * in the linker script).
 ****************************************************************************/
        .section .text.boot

        .org    0

        dc.l    __stack                 /* Stack address */
        dc.l    _entry_point            /* Program start address */
        dc.l    _bus_error
        dc.l    _address_error
        dc.l    _illegal_instruction
        dc.l    _zero_divide
        dc.l    _chk_instruction
        dc.l    _trapv_instruction
        dc.l    _privilege_violation
        dc.l    _trace
        dc.l    _line_1010_emulation
        dc.l    _line_1111_emulation
        dc.l    _error_exception, _error_exception, _error_exception, _error_exception
        dc.l    _error_exception, _error_exception, _error_exception, _error_exception
        dc.l    _error_exception, _error_exception, _error_exception, _error_exception
        dc.l    _error_exception /* Spurious interrupt */
        # Auto vectors 1 to 7
        dc.l    _int
        dc.l    _extint
        dc.l    _int
        dc.l    _hint
        dc.l    _int
        dc.l    _vint
        dc.l    _int
        # Traps 0 to 15
        dc.l    _unlock,_int,_int,_int,_int,_int,_int,_int
        dc.l    _int,_int,_int,_int,_int,_int,_int,_int
        # FP, MMU and undefined stuff
        dc.l    _int,_int,_int,_int,_int,_int,_int,_int
        dc.l    _int,_int,_int,_int,_int,_int,_int,_int

        # Add the Megadrive ROM header
        .incbin "boot/rom_head.bin", 0, 0x100

table:
        dc.w    _sdata, 0x3FFF, 0x0100
        dc.l    0xA11100, 0xA11200, 0xC00011, __ustack, _ram_start, _rom_end
        dc.b    0x9F, 0xBF, 0xDF, 0xFF

_entry_point:
        move    #0x2700, sr
        lea     table.w, a6
        movem.w (a6)+, d5-d7
        movem.l (a6)+, a0-a5
        # Check Version Number
        move.b  -0x10ff(a0), d0
        andi.b  #0x0f, d0
        beq.s   wrong_version
        # Sega Security Code (SEGA)
        move.l  #0x53454741, 0x2f00(a0)
wrong_version:
        # Put Z80 and YM2612 in reset (note: might not be enough cycles)
        moveq   #0x00, d0
        move.b  d0, (a1)

        # Pause Z80
        move.w  d7, (a0)
        move.w  d7, (a1)

        # Mute PSG
        moveq   #3, d1
psg_mute:
        move.b  (a6)+, (a2)
        dbra    d1, psg_mute

        # Set user stack pointer
        move.l  a3, usp

        # clear Genesis RAM. We already have 0x3FFF on d6
        # TODO: do not clear what will be overwritten by data below
        move.l    a4, a3
clear_ram:
        move.l  d0, (a3)+
        dbra    d6, clear_ram

        # Copy initialized variables from ROM to Work RAM. We have:
        # _sdata on d5, _rom_end on a5 and _ram_start on a4
        lsr.l   #1, d5
        beq     2f

        subq.w  #1, d5
1:
        move.w  (a5)+, (a4)+
        dbra    d5, 1b

2:
        # Jump to user entry point
        jmp     _start_entry


/****************************************************************************
 * Other than _vint, we are not using interrupts on this example. If any of
 * them fires, machine will be halted.
 ****************************************************************************/
_bus_error:
_address_error:
_illegal_instruction:
_zero_divide:
_chk_instruction:
_trapv_instruction:
_privilege_violation:
_trace:
_line_1010_emulation:
_line_1111_emulation:
_error_exception:
_int:
_extint:
_hint:
        jmp _int

        .data
/****************************************************************************
 * Variables needed for the task context switches
 ****************************************************************************/
# User task status register. Initial value of second nibble must be 5 or
# lower in order for VINT interrupts to fire. First nibble must be 0 in
# order for the user task to run.
utsk_sr: .word 0x0400

# User task program counter
utsk_pc: .long 0

# User task registers saved on context switch
        .equ UTSK_REGS_LEN, 15 * 4
utsk_regs: .fill UTSK_REGS_LEN, 1, 0

# Supervisor task lock
lock:   .word 0

# VINT routine callback pointer
vint_cb: .long except_return


        .text

/************************************************************************//**
 * Configure the VBLANK interrupt attention callback. Note that the function
 * is run in exception context.
 *
 * Receives a parameter with the pointer to interrupt attention callback.
 ****************************************************************************/
        .globl vint_cb_set
vint_cb_set:
        move.l  4(sp), vint_cb
        rts

/************************************************************************//**
 * Configure the task used as user task. Must be invoked once before calling
 * tsk_user_yield().
 *
 * Receives a parameter with the pointer to the user task.
 ****************************************************************************/
        .globl tsk_user_set
tsk_user_set:
        move.l  4(sp), utsk_pc
        rts

/************************************************************************//**
 * Resume a blocked supervisor task. Must be called from user task.
 ****************************************************************************/
        .globl tsk_super_post
tsk_super_post:
        clr (lock)
        # Call supervisor for the context switch
        trap #0
        rts

/************************************************************************//**
 * Block supervisor task and resume user task. Supervisor task will not
 * resume execution until tsk_super_post() is called from user task.
 ****************************************************************************/
        .globl tsk_super_pend
tsk_super_pend:
        # Set lock and fallthrough to tsk_user_yield to switch to user task
        move.w  6(sp), (lock)

/************************************************************************//**
 * tsk_user_yield: yield from supervisor task to user task
 ****************************************************************************/
        .globl tsk_user_yield
tsk_user_yield:
        # Push sr onto the stack. We already have there the pc, so we can
        # use rte in the _vint code to go back to the supervisor task.
        move.w  sr, d0
        move.w  #0x2700, sr
        move.w  d0, -(sp)

        # Push non clobberable registers. Since user task can modify them,
        # they need to be restored before resuming the supervisor task.
        # a7 (the sp) should not need to be saved because when this function
        # exits, the usp will be used instead, and if any interrupt fires
        # (so sp is used again), on return it should leave it where it was.
        movem.l d2-d7/a2-a6, -(sp)

        # Restore bg task registers. a7 does not need to be restored,
        # since it is the usp and is restored when we go to user mode.
        lea     (utsk_regs + 4), a0
        movem.l (a0)+, d0-d7/a1-a6
        move.l  (utsk_regs), a0

        # Enter the user task by pushing its pc and sr, and executing an rte.
        # This will change context to user mode.
        move.l  (utsk_pc), -(sp)
        move.w  (utsk_sr), -(sp)
        rte

/************************************************************************//**
 * Vertical blanking interrupt: if we are running user task, switch back to
 * supervisor task.
 *
 * Also embedded in this routine is the trap #0 that unlocks the supervisor
 * task.
 ****************************************************************************/
_vint:
        # If we are already at the supervisor task, skip context switch
        btst    #5, (sp)
        bne.s   no_ctx_switch

        # Check if supervisor task is locked
        tst.w   (lock)          | 16
        # If lock is 0, we are not locked, unlock foreground task
        beq.s    _unlock        | 8/10
        # If lock is negative, we are locked and wait is infinite
        bcs.s    no_ctx_switch  | 8/10
        subq.w  #1, (lock)      | 20

        # Jump to VBLANK routine callback
        move.l  vint_cb, -(sp)
        rts

_unlock:
        # Save bg task registers (excepting a7, that is stored in usp)
        move.l  a0, (utsk_regs)
        lea     (utsk_regs + UTSK_REGS_LEN), a0
        movem.l d0-d7/a1-a6, -(a0)

        # Pop bg task sr and pc, and save them, so they can be restored later
        move.w  (sp)+, (utsk_sr)
        move.l  (sp)+, (utsk_pc)

        # Restore fg task non clobberable registers
        movem.l (sp)+, d2-d7/a2-a6

        # Resume supervisor task (its pc and sr are in the stack)
no_ctx_switch:
        move.l  vint_cb, -(sp)
        rts

# Vector used as default value for vint_cb
except_return:
        rte

