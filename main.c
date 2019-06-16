/************************************************************************//**
 * \brief 1985 Channel
 * \author Jesús Alonso (doragasu)
 * \date   2019
 * \defgroup 1985ch main
 * \{
 ****************************************************************************/
#include <string.h>
#include "vdp.h"
#include "mw/util.h"
#include "mw/mpool.h"
#include "mw/megawifi.h"
#include "mw/loop.h"

/// Length of the wflash buffer
#define MW_BUFLEN	1440

/// TCP port to use (set to Megadrive release year ;-)
#define MW_CH_PORT 	1985

/// Maximum number of loop functions
#define MW_MAX_LOOP_FUNCS	2

/// Maximun number of loop timers
#define MW_MAX_LOOP_TIMERS	4

/// Command buffer
static char cmd_buf[MW_BUFLEN];

static void println(const char *str, int color)
{
	static unsigned int line = 2;

	if (str) {
		VdpDrawText(VDP_PLANEA_ADDR, 2, line, color, 36, str, 0);
	}
	line++;
}

static void idle_cb(struct loop_func *f)
{
	UNUSED_PARAM(f);
	mw_process();
}

void udp_recv_cb(enum lsd_status stat, uint8_t ch,
		char *data, uint16_t len, void *ctx)
{
	const struct mw_reuse_payload *udp =
		(const struct mw_reuse_payload*)data;
	UNUSED_PARAM(ctx);

	if (LSD_STAT_COMPLETE == stat) {
		mw_udp_reuse_send(ch, udp, len, NULL, NULL);
	}
}

static void udp_echo(struct loop_timer *t)
{
	struct mw_reuse_payload *pkt =
		(struct mw_reuse_payload * const)cmd_buf;
	UNUSED_PARAM(t);

	mw_udp_reuse_recv(pkt, MW_BUFLEN, NULL, udp_recv_cb);
}

static void bssid_bin_to_str(const uint8_t *bssid, char *bssid_str)
{
	int i;

	for (i = 0; i < 5; i++) {
		uint8_to_hex_str(bssid[i], &bssid_str[7 + 3 * i]);
		bssid_str[9 + 3 * i] = ':';
	}
	uint8_to_hex_str(bssid[i], &bssid_str[6 + 3 * i]);
}

//static void gamertag_init(int id, const char *name, const char *security,
//		const char *tagline, struct mw_gamertag *gamertag)
//{
//	memset(gamertag, 0, sizeof(struct mw_gamertag));
//	gamertag->id = id;
//	strcpy(gamertag->nickname, name);
//	strcpy(gamertag->security, security);
//	strcpy(gamertag->tagline, tagline);
//}

static void print_gamertag(struct mw_gamertag *gamertag)
{
	char id[9];
	uint32_to_hex_str(gamertag->id, id, 0);
	println(id, VDP_TXT_COL_CYAN);
	println(gamertag->nickname, VDP_TXT_COL_WHITE);
	println(gamertag->security, VDP_TXT_COL_WHITE);
	println(gamertag->tagline, VDP_TXT_COL_WHITE);
	println("", VDP_TXT_COL_WHITE);
}

static void run_test(struct loop_timer *t)
{
	enum mw_err err;
	uint8_t *bssid;
	char bssid_str[7 + 17 + 1] = "BSSID: ";
//	struct mw_gamertag gamertag;
	struct mw_gamertag *pgt;

	// Set test gamertags
//	gamertag_init(1, "doragasu", "secure_password", "If it's not broken, "
//			"I'll fix it", &gamertag);
//	mw_gamertag_set(0, &gamertag);
//	gamertag_init(2, "Pocket_Lucho", "Me encanta git", "SEEEEEGAAAAAAA!",
//			&gamertag);
//	mw_gamertag_set(1, &gamertag);
//	gamertag_init(3, "Davidian", "Murcianete", "A topeeeeeeeeee!",
//			&gamertag);
//	mw_gamertag_set(2, &gamertag);

	// Read back and print gamertags
	for (int i = 0; i < 3; i++) {
		pgt = mw_gamertag_get(i);
		if (pgt) {
			print_gamertag(pgt);
		}
	}


	// Get BSSID
	bssid = mw_bssid_get(MW_IF_STATION);
	if (!bssid) {
		goto err;
	}
	bssid_bin_to_str(bssid, bssid_str);
	println(bssid_str, VDP_TXT_COL_WHITE);

	// Join AP
	println("Associating to AP", VDP_TXT_COL_WHITE);
	err = mw_ap_assoc(0);
	if (err != MW_ERR_NONE) {
		goto err;
	}
	err = mw_ap_assoc_wait(MS_TO_FRAMES(30000));
	if (err != MW_ERR_NONE) {
		goto err;
	}
	// Wait an additional second to ensure DNS service is up
	mw_sleep(MS_TO_FRAMES(1000));
	println("DONE!", VDP_TXT_COL_CYAN);
	println(NULL, 0);

	// Connect to www.duck.com on port 443
	println("Connecting to www.duck.com", VDP_TXT_COL_WHITE);
	err = mw_tcp_connect(1, "www.duck.com", "443", NULL);
	if (err != MW_ERR_NONE) {
		goto err;
	}
	println("DONE!", VDP_TXT_COL_CYAN);
	println(NULL, 0);

	println("Test finished, all OK!", VDP_TXT_COL_WHITE);
	
	mw_tcp_disconnect(1);

	// Start UDP echo task
	mw_udp_set(1, NULL, NULL, "7");
	t->timer_cb = udp_echo;
	goto out;

err:
	println("ERROR!", VDP_TXT_COL_MAGENTA);

out:
	mw_ap_disassoc();
}

/// MegaWiFi initialization
static void megawifi_init_cb(struct loop_timer  *t)
{
	uint8_t ver_major = 0, ver_minor = 0;
	char *variant = NULL;
	enum mw_err err;
	char line[] = "MegaWiFi version X.Y";

	// Try detecting the module
	err = mw_detect(&ver_major, &ver_minor, &variant);

	if (MW_ERR_NONE != err) {
		// Megawifi not found
		println("MegaWiFi not found!", VDP_TXT_COL_MAGENTA);
	} else {
		// Megawifi found
		line[17] = ver_major + '0';
		line[19] = ver_minor + '0';
		println(line, VDP_TXT_COL_WHITE);
		println(NULL, 0);
		// Configuration complete, run test function next frame
		t->timer_cb = run_test;
		loop_timer_start(t, 1);

	}
}

/// Loop run while idle
static void main_loop_init(void)
{
	// Run next frame, do not auto-reload
	static struct loop_timer frame_timer = {
		.timer_cb = megawifi_init_cb,
		.frames = 1
	};
	static struct loop_func megawifi_loop = {
		.func_cb = idle_cb
	};

	loop_init(MW_MAX_LOOP_FUNCS, MW_MAX_LOOP_TIMERS);
	loop_timer_add(&frame_timer);
	loop_func_add(&megawifi_loop);
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
	// Initialize MegaWiFi
	mw_init(cmd_buf, MW_BUFLEN);
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

