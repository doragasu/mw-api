/************************************************************************//**
 * Tasking routines related functions.
 *
 * These functions are implemented in boot/sega.s
 ****************************************************************************/

#ifndef __TSK_H__
#define __TSK_H__

#include <stdint.h>
#include <stdbool.h>

/// Frames per second (60 on NTSC consoles, 50 on PAL machines)
#define FPS	60

/// Converts milliseconds to frames, rounding to the nearest.
#define MS_TO_FRAMES(ms)	(((ms)*FPS/500 + 1)/2)

#define TSK_PEND_FOREVER -1

/************************************************************************//**
 * Configure the VBLANK interrupt attention callback. Note that the function
 * is run in exception context.
 *
 * \param[in] Function pointer to the VBLANK callback.
 *
 * \warning Callback is run in exception context. Use rte (instead of rts)
 * if function is coded in assembly language, or __attribute__((interrupt))
 * for C functions.
 ****************************************************************************/
void vint_cb_set(void (*vint_cb)(void));

/************************************************************************//**
 * Configure the task used as user task. Must be invoked once before calling
 * tsk_user_yield().
 *
 * \param[in] user_tsk A function pointer to the user task to configure.
 ****************************************************************************/
void tsk_user_set(void (*user_tsk)(void));

/************************************************************************//**
 * Yield from supervisor task to user task. The user task will resume and
 * will use all the available CPU time until the next vertical blanking
 * interrupt, that will resume the supervisor task.
 ****************************************************************************/
void tsk_user_yield(void);

/************************************************************************//**
 * Block supervisor task and resume user task. Supervisor task will not
 * resume execution until super_tsk_post() is called from user task or a
 * timeout happens..
 *
 * \param[in] wait_tout Maximum number of frames to wait while blocking. Use
 *            TSK_PEND_FOREVER for an infinite wait, or a positive number
 *            (greater than 0) for a specific number of frames.
 *
 * \return false if task was awakened from user task, or true if timeout
 * occurred.
 ****************************************************************************/
bool tsk_super_pend(int16_t wait_tout);

/************************************************************************//**
 * Resume a blocked supervisor task. Must be called from user task.
 *
 * \param[in] force_ctx_sw If true, immediately causes a context switch to
 *            supervisor task. If false, context switch will not occur until
 *            the VBLANK interrupt.
 ****************************************************************************/
void tsk_super_post(bool force_ctx_sw);

#endif /*__TSK_H__*/
