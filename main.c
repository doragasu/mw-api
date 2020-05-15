/************************************************************************//**
 * \brief 1985 Channel
 * \author Jesús Alonso (doragasu)
 * \date   2019
 * \defgroup 1985ch main
 * \{
 ****************************************************************************/
#include "vdp.h"
#include "mw/util.h"
#include "mw/mpool.h"
#include "mw/megawifi.h"
#include "mw/loop.h"

/// Length of the wflash buffer
#define MW_BUFLEN	1460

/// TCP port to use (set to Megadrive release year ;-)
#define MW_CH_PORT 	1985

/// Maximum number of loop functions
#define MW_MAX_LOOP_FUNCS	2

/// Maximun number of loop timers
#define MW_MAX_LOOP_TIMERS	4

/// Command buffer
static char cmd_buf[MW_BUFLEN];

static void udp_recv_cb(enum lsd_status stat, uint8_t ch,
		char *data, uint16_t len, void *ctx);

/// Certificate for www.example.org
static const char cert[] = "-----BEGIN CERTIFICATE-----\n"
"MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8+8kTANBgkqhkiG9w0BAQsFADBh"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD"
"QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVT"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg"
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB"
"ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83"
"nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd"
"KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f"
"/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX"
"kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0"
"/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8C"
"AQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYY"
"aHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6"
"Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1"
"oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD"
"QS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8v"
"d3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA+AYRyCMWHVLyjnjUY4tCzh"
"xtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEB"
"CwUAA4IBAQAjPt9L0jFCpbZ+QlwaRMxp0Wi0XUvgBCFsS+JtzLHgl4+mUwnNqipl"
"5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq/sELfeNqzqPlt/yGFUzZgTHbO7Djc1lGA"
"8MXW5dRNJ2Srm8c+cftIl7gzbckTB+6WohsYFfZcTEDts8Ls/3HB40f/1LkAtDdC"
"2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPit"
"c+LJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0"
"j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz\n"
"-----END CERTIFICATE-----";
static const uint16_t cert_len = sizeof(cert) - 1;
/// Certificate hash, obtained with command:
/// openssl x509 -hash in <cert_file_name> -noout
static const uint32_t cert_hash = 0x85cf5865;

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

static void udp_send_complete_cb(enum lsd_status stat, void *ctx)
{
	struct mw_reuse_payload *pkt =
		(struct mw_reuse_payload * const)cmd_buf;
	UNUSED_PARAM(ctx);
	UNUSED_PARAM(stat);

	// Trigger reception of another UDP packet
	mw_udp_reuse_recv(pkt, MW_BUFLEN, NULL, udp_recv_cb);
}

static void udp_recv_cb(enum lsd_status stat, uint8_t ch,
		char *data, uint16_t len, void *ctx)
{
	const struct mw_reuse_payload *udp =
		(const struct mw_reuse_payload*)data;
	UNUSED_PARAM(ctx);

	// Ignore frame if not from channel 2
	if (LSD_STAT_COMPLETE == stat && 2 == ch) {
		mw_udp_reuse_send(2, udp, len, NULL, udp_send_complete_cb);
	} else {
		mw_udp_reuse_recv((struct mw_reuse_payload*)cmd_buf,
				MW_BUFLEN, NULL, udp_recv_cb);
	}
}

static void udp_normal_test(void)
{
	char line[40];
	int16_t len = sizeof(line);
	uint8_t ch = 1;

	// Make sure you are listening on the target address, e.g. with:
	// nc -lu 12345
	println("Send to UDP 12345, waiting for reply",
			VDP_TXT_COL_CYAN);
	// Send UDP data to peer and wait for reply. Localhost works only when
	// using emulators, so change IP as needed when using the real thing.
	if (mw_udp_set(ch, "127.0.0.1", "12345", NULL)) {
		goto err;
	}
	mw_send_sync(ch, "MegaWiFi UDP test!\n", 20, 0);
	mw_recv_sync(&ch, line, &len, 0);
	line[39] = '\0';
	if (1 == ch) {
		println("Got UDP reply:", VDP_TXT_COL_CYAN);
		println(line, VDP_TXT_COL_WHITE);
	}
	mw_close(ch);

	return;

err:
	println("UDP test failed!", VDP_TXT_COL_MAGENTA);
	mw_close(1);
}

static void udp_reuse_test(void)
{
	struct mw_reuse_payload *pkt =
		(struct mw_reuse_payload * const)cmd_buf;

	// You can send text and get the echo e.g. by:
	// nc -u <dest_ip> 8007
	println("Doing echo on UDP port 8007", VDP_TXT_COL_CYAN);
	// Start UDP echo task
	mw_udp_set(2, NULL, NULL, "8007");
	mw_udp_reuse_recv(pkt, MW_BUFLEN, NULL, udp_recv_cb);
}

static int http_recv(uint32_t len)
{
	int16_t recv_last;
	int err = FALSE;
	uint32_t recvd = 0;
	uint8_t ch = MW_HTTP_CH;

	// For the test, just read and discard the data
	while (recvd < len && !err) {
		recv_last = MW_BUFLEN;
		err = mw_recv_sync(&ch, cmd_buf, &recv_last, 0) != MW_ERR_NONE;
		recvd += recv_last;
	}

	return err;
}

static void http_cert_set(void)
{
	uint32_t hash = mw_http_cert_query();
	if (hash != cert_hash) {
		mw_http_cert_set(cert_hash, cert, cert_len);
	}
}

static void http_test(void)
{
	uint32_t len = 0;

	http_cert_set();

	if (mw_http_url_set("https://www.example.com") ||
			mw_http_method_set(MW_HTTP_METHOD_GET) ||
			mw_http_open(0) ||
			mw_http_finish(&len, MS_TO_FRAMES(20000)) < 100) {
		goto err_out;
	}
	if (len) {
		if (http_recv(len)) goto err_out;
	}

	println("HTTP test SUCCESS", VDP_TXT_COL_WHITE);
	return;

err_out:
	println("HTTP test FAILED", VDP_TXT_COL_WHITE);
	return;
}

static void tcp_test(void)
{
	enum mw_err err;

	// Connect to www.example.com on port 80
	println("Connecting to www.example.com", VDP_TXT_COL_WHITE);
	err = mw_tcp_connect(1, "www.example.com", "80", NULL);
	if (err != MW_ERR_NONE) {
		println("TCP test FAILED", VDP_TXT_COL_CYAN);
		goto out;
	}
	println("DONE!", VDP_TXT_COL_CYAN);
	println(NULL, 0);

	println("TCP test SUCCESS", VDP_TXT_COL_WHITE);

out:
	mw_close(1);
}

static void datetime_test(void)
{
	const char *datetime;
	uint32_t dt_bin[2] = {};
	union mw_msg_sys_stat *stat;

	// Wait until date/time is set
	do {
		mw_sleep(60);
		stat = mw_sys_stat_get();
		if (!stat) {
			println("Failed to get date/time", VDP_TXT_COL_CYAN);
			return;
		}
	} while (!stat->dt_ok);
	datetime = mw_date_time_get(dt_bin);
	println(datetime, VDP_TXT_COL_WHITE);
}

static void flash_test(void) {
	uint16_t *p_runs = (uint16_t*)mw_flash_read(0, 2);
	uint16_t runs = *p_runs;
	char msg[] = "ID:        , runs:      ";
	uint16_t dev_id = 0;
	uint8_t man_id = 0;

	mw_flash_id_get(&man_id, &dev_id);
	if (0xFFFF == runs) {
		runs = 0;
	}
	runs++;
	mw_flash_sector_erase(0);
	mw_flash_write(0, (uint8_t*)&runs, sizeof(uint16_t));

	uint8_to_hex_str(man_id, &msg[4]);
	msg[6] = ' ';
	uint8_to_hex_str(dev_id>>8, &msg[7]);
	uint8_to_hex_str(dev_id, &msg[9]);
	msg[11] = ',';
	uint16_to_str(runs, &msg[19]);
	println(msg, VDP_TXT_COL_WHITE);
}

static void run_test(struct loop_timer *t)
{
	enum mw_err err;

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
	mw_sleep(3 * 60);
	println("DONE!", VDP_TXT_COL_CYAN);
	println(NULL, 0);

	flash_test();
	tcp_test();
	http_test();
	datetime_test();

	// Test UDP in normal mode
	udp_normal_test();

	// Test UDP in reuse mode
	udp_reuse_test();

	goto out;

err:
	println("ERROR!", VDP_TXT_COL_MAGENTA);
	mw_ap_disassoc();

out:
	loop_timer_del(t);
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

