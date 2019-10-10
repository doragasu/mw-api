/************************************************************************//**
 * \brief 1985 Channel
 * \author Jesús Alonso (doragasu)
 * \date   2019
 * \defgroup 1985ch main
 * \{
 ****************************************************************************/
#include <stdio.h>
#include "vdp.h"
#include "mw/util.h"
#include "mw/mpool.h"
#include "mw/loop.h"
#include "mw/megawifi.h"

/// Maximum number of loop functions
#define MW_MAX_LOOP_FUNCS	2

/// Maximun number of loop timers
#define MW_MAX_LOOP_TIMERS	2

static uint32_t bg = 0;
static uint32_t timed = 0;

char buf[1440];

static void test_cb(struct loop_timer *t)
{
	UNUSED_PARAM(t);
	char num[9];

	uint32_to_hex_str(bg, num, 0);
	VdpDrawText(VDP_PLANEA_ADDR, 2, 3, VDP_TXT_COL_WHITE, 12, num, 0);
	uint32_to_hex_str(timed++, num, 0);
	VdpDrawText(VDP_PLANEA_ADDR, 2, 2, VDP_TXT_COL_WHITE, 12, num, 0);
}

static void background_func_cb(struct loop_func *f)
{
	UNUSED_PARAM(f);
	mw_sleep(1);
	bg++;
}

/// Loop run while idle
static void main_loop_init(void)
{
	// Run next frame, do not auto-reload
	static struct loop_timer frame_timer = {
		.timer_cb = test_cb,
		.frames = 1,
		.auto_reload = 1
	};

	static struct loop_func background_func = {
		.func_cb = background_func_cb
	};

	loop_init(MW_MAX_LOOP_FUNCS, MW_MAX_LOOP_TIMERS);
	loop_timer_add(&frame_timer);
	loop_func_add(&background_func);
}

/// Global initialization
static void init(void)
{
	// Initialize memory pool
	mp_init(0);
	// Initialize VDP
	VdpInit();
	// Initialize game loop
	main_loop_init();
	mw_init(buf, 1440);
}

/// Entry point
int main(void)
{
	init();

	// Enter game loop (should never return)
	loop();

	return 0;
}

/** \} */

