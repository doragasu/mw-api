/************************************************************************//**
 * \brief MeGaWiFi API implementation.
 *
 * \author Jesus Alonso (doragasu)
 * \date 2015
 *
 * \note Module is not reentrant.
 *
 * \todo Missing a lot of integrity checks, also module should track used
 *       channels, and is not currently doing it
 ****************************************************************************/

#include <string.h>
#include "megawifi.h"
#include "util.h"
#include "loop.h"

#define MW_COMMAND_TOUT		MS_TO_FRAMES(MW_COMMAND_TOUT_MS)
#define MW_SCAN_TOUT		MS_TO_FRAMES(MW_SCAN_TOUT_MS)
#define MW_ASSOC_TOUT		MS_TO_FRAMES(MW_ASSOC_TOUT_MS)
#define MW_STAT_POLL_TOUT	MS_TO_FRAMES(MW_STAT_POLL_MS)
#define MW_HTTP_OPEN_TOUT	MS_TO_FRAMES(MW_HTTP_OPEN_TOUT_MS)
/*
 * The module assumes that once started, sending always succeeds, but uses
 * timers (when defined) for data reception.
 */

enum cmd_stat {
	CMD_ERR_PROTO = -2,
	CMD_ERR_TIMEOUT = -1,
	CMD_OK = 1
};

struct recv_metadata {
	uint16_t len;
	uint8_t ch;
};

struct mw_data {
	mw_cmd *cmd;
	lsd_recv_cb cmd_data_cb;
	struct loop_timer timer;
	uint16_t buf_len;
	int16_t tout_frames;
	union {
		uint8_t flags;
		struct {
			uint8_t mw_ready:1;
			uint8_t stat_poll:1;
			uint8_t monitor_ch:4;
		};
	};
};

/// Data required by the module
static struct mw_data d = {};


void cmd_tout_cb(struct loop_timer *t);

int mw_init(char *cmd_buf, uint16_t buf_len)
{
	if (!cmd_buf || buf_len < MW_CMD_MIN_BUFLEN) {
		return MW_ERR_BUFFER_TOO_SHORT;
	}

	memset(&d, 0, sizeof(struct mw_data));
	d.cmd = (mw_cmd*)cmd_buf;
	d.buf_len = buf_len;
	d.timer.timer_cb = cmd_tout_cb;
	loop_timer_add(&d.timer);

	lsd_init();

	// Keep WiFi module in reset
	mw_module_reset();
	// Power down and Program not active (required for the module to boot)
	uart_clr_bits(MCR, MW__PRG | MW__PD);

	// Try accessing UART scratch pad register to see if it is installed
	UART_SPR = 0x55;
	if (UART_SPR != 0x55) return MW_ERR;
	UART_SPR = 0xAA;
	if (UART_SPR != 0xAA) return MW_ERR;

	// Enable control channel
	lsd_ch_enable(MW_CTRL_CH);

	d.mw_ready = TRUE;

	return MW_ERR_NONE;
}

static void cmd_send_cb(enum lsd_status err, void *ctx)
{
	UNUSED_PARAM(ctx);

	loop_timer_stop(&d.timer);
	if (!err) {
		loop_post(CMD_OK);
	} else {
		loop_post(CMD_ERR_PROTO);
	}
}

static void cmd_recv_cb(enum lsd_status err, uint8_t ch,
		char *data, uint16_t len, void *ctx)
{
	UNUSED_PARAM(data);
	struct recv_metadata *md = (struct recv_metadata*)ctx;

	loop_timer_stop(&d.timer);
	if (!err) {
		md->ch = ch;
		md->len = len;
		loop_post(CMD_OK);
	} else {
		loop_post(CMD_ERR_PROTO);
	}
}

void cmd_tout_cb(struct loop_timer *t)
{
	UNUSED_PARAM(t);

	loop_post(CMD_ERR_TIMEOUT);
}

static enum mw_err mw_command(int timeout_frames)
{
	struct recv_metadata md;
	int stat;
	int done = FALSE;

	mw_cmd_send(d.cmd, NULL, cmd_send_cb);
	/// \todo Optimization: maybe we do not need to wait for the send
	/// process to complete, just jump to reception.
	loop_timer_start(&d.timer, timeout_frames);
	stat = loop_pend();
	if (CMD_OK != stat) {
		return MW_ERR_SEND;
	}

	while (!done) {
		mw_cmd_recv(d.cmd, &md, cmd_recv_cb);
		loop_timer_start(&d.timer, timeout_frames);
		stat = loop_pend();
		if (CMD_OK != stat) {
			return MW_ERR_RECV;
		}
		// We might receive network data while waiting
		// for a command reply
		if (MW_CTRL_CH == md.ch) {
			done = TRUE;
		} else if (d.cmd_data_cb) {
			d.cmd_data_cb(LSD_STAT_COMPLETE, md.ch,
					(char*)d.cmd, md.len, NULL);
		}
	}

	return MW_ERR_NONE;
}

enum mw_err mw_recv_sync(uint8_t *ch, char *buf, int16_t *buf_len,
		uint16_t tout_frames)
{
	struct recv_metadata md;
	int stat;

	lsd_recv(buf, *buf_len, &md, cmd_recv_cb);
	if (tout_frames) {
		loop_timer_start(&d.timer, tout_frames);
	}
	stat = loop_pend();
	if (CMD_OK != stat) {
		return MW_ERR_RECV;
	}

	*ch = md.ch;
	*buf_len = md.len;

	return MW_ERR_NONE;
}

enum mw_err mw_send_sync(uint8_t ch, const char *data, uint16_t len,
		uint16_t tout_frames)
{
	uint16_t to_send;
	int stat;
	uint16_t sent = 0;

	while (sent < len) {
		to_send = MIN(len - sent, d.buf_len);
		lsd_send(ch, data + sent, to_send, NULL, cmd_send_cb);
		if (tout_frames) {
			loop_timer_start(&d.timer, tout_frames);
		}
		stat = loop_pend();
		if (CMD_OK != stat) {
			return MW_ERR_SEND;
		}
		sent += to_send;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_detect(uint8_t *major, uint8_t *minor, char **variant)
{
	int retries = 5;
	enum mw_err err;

	// Wait a bit and take module out of resest
	loop_timer_start(&d.timer, MS_TO_FRAMES(30));
	loop_pend();
	mw_module_start();
	loop_timer_start(&d.timer, MS_TO_FRAMES(1000));
	loop_pend();

	do {
		retries--;
		uart_reset_fifos();
		err = mw_version_get(major, minor, variant);
	} while (err != MW_ERR_NONE && retries);

	return err;
}

enum mw_err mw_version_get(uint8_t *major, uint8_t *minor, char **variant)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_VERSION;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return err;
	}
	if (major) {
		*major = d.cmd->data[0];
	}
	if (minor) {
		*minor = d.cmd->data[1];
	}
	if (variant) {
		/// \todo check this
		// Version string is not NULL terminated, add proper termination
		d.cmd->data[MIN(d.buf_len - 1, d.cmd->data_len)] = '\0';
		*variant = (char*)(d.cmd->data + 2);
	}

	return MW_ERR_NONE;
}

char *mw_echo(const char *data, int *len)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return NULL;
	}
	if (*len + LSD_OVERHEAD + 4 < d.buf_len) {
		return NULL;
	}

	// Try sending and receiving echo
	d.cmd->cmd = MW_CMD_ECHO;
	d.cmd->data_len = *len;
	memcpy(d.cmd->data, data, *len);
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}
	*len = d.cmd->data_len;

	return (char*)d.cmd->data;
}

enum mw_err mw_default_cfg_set(void)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_DEF_CFG_SET;
	d.cmd->data_len = 4;
	d.cmd->dw_data[0] = 0xFEAA5501;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_ap_cfg_set(uint8_t slot, const char *ssid, const char *pass)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}
	if (!ssid) {
		// At least SSID is required.
		return MW_ERR_PARAM;
	}
	if (slot >= MW_NUM_CFG_SLOTS) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_AP_CFG;
	d.cmd->data_len = sizeof(struct mw_msg_ap_cfg);

	memset(&d.cmd->ap_cfg, 0, sizeof(struct mw_msg_ap_cfg));
	d.cmd->ap_cfg.cfg_num = slot;
	// Note: *NOT* NULL terminated strings are allowed on cmd.ap_cfg.ssid
	// and cmd.ap_cfg.pass
	memcpy(d.cmd->ap_cfg.ssid, ssid, strnlen(ssid, MW_SSID_MAXLEN));
	if (pass) {
		memcpy(d.cmd->ap_cfg.pass, pass, strnlen(pass, MW_PASS_MAXLEN));
	}

	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_ap_cfg_get(uint8_t slot, char **ssid, char **pass)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}
	if (slot >= MW_NUM_CFG_SLOTS) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_AP_CFG_GET;
	d.cmd->data_len = 1;
	d.cmd->data[0] = slot;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	if (ssid) {
		*ssid = d.cmd->ap_cfg.ssid;
	}
	if (pass) {
		*pass = d.cmd->ap_cfg.pass;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_ip_cfg_set(uint8_t slot, const struct mw_ip_cfg *ip)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}
	if (slot >= MW_NUM_CFG_SLOTS) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_IP_CFG;
	d.cmd->data_len = sizeof(struct mw_msg_ip_cfg);
	d.cmd->ip_cfg.cfg_slot = slot;
	d.cmd->ip_cfg.reserved[0] = 0;
	d.cmd->ip_cfg.reserved[1] = 0;
	d.cmd->ip_cfg.reserved[2] = 0;
	d.cmd->ip_cfg.ip = *ip;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_ip_cfg_get(uint8_t slot, struct mw_ip_cfg **ip)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_IP_CFG_GET;
	d.cmd->data_len = 1;
	d.cmd->data[0] = slot;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	*ip = &d.cmd->ip_cfg.ip;

	return MW_ERR_NONE;
}

enum mw_err mw_ip_current(struct mw_ip_cfg **ip)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_IP_CURRENT;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	*ip = &d.cmd->ip_cfg.ip;

	return MW_ERR_NONE;
}

int mw_ap_scan(char **ap_data, uint8_t *aps)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return -1;
	}

	d.cmd->cmd = MW_CMD_AP_SCAN;
	d.cmd->data_len = 0;
	err = mw_command(MW_SCAN_TOUT);
	if (err) {
		return -1;
	}

	// Fill number of APs and skip it for the apData array
	*aps = *d.cmd->data;
	*ap_data = ((char*)d.cmd->data) + 1;

	return d.cmd->data_len - 1;
}

int mw_ap_fill_next(const char *ap_data, uint16_t pos,
		struct mw_ap_data *apd, uint16_t data_len)
{
	if (pos >= data_len) {
		return 0;	// End reached
	}
	if ((pos + ap_data[pos + 3] + 4) > data_len) {
		return -1;
	}
	apd->auth = ap_data[pos++];
	apd->channel = ap_data[pos++];
	apd->rssi = ap_data[pos++];
	apd->ssid_len = ap_data[pos++];
	apd->ssid = (char*)ap_data + pos;

	// Return updated position
	return pos + apd->ssid_len;
}

enum mw_err mw_ap_assoc(uint8_t slot)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_AP_JOIN;
	d.cmd->data_len = 1;
	d.cmd->data[0] = slot;
	err = mw_command(MW_ASSOC_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

static void stat_reply_cb(enum lsd_status err, uint8_t ch, char *data,
		uint16_t len, void *ctx)
{
	UNUSED_PARAM(ctx);
	UNUSED_PARAM(len);
	UNUSED_PARAM(data);

	if (!err && MW_CTRL_CH == ch && d.stat_poll &&
			d.cmd->sys_stat.sys_stat >= MW_ST_READY) {
		// We are associated!
		loop_timer_stop(&d.timer);
		d.stat_poll = FALSE;
		loop_post(1);
	} else {
		// Query the system status again
		d.cmd->cmd = MW_CMD_SYS_STAT;
		d.cmd->data_len = 0;
		mw_cmd_send(d.cmd, NULL, NULL);
	}
}

static void assoc_poll_timer_cb(struct loop_timer *t)
{
	if (d.tout_frames) {
		d.tout_frames -= MW_STAT_POLL_TOUT;
		if (d.tout_frames <= 0) {
			d.stat_poll = FALSE;
			loop_timer_stop(t);
			loop_post(-1);
			return;
		}
	}
	mw_cmd_recv(d.cmd, NULL, stat_reply_cb);
}

enum mw_err mw_ap_assoc_wait(int tout_frames)
{
	int ret;

	// Send command and do not look back
	d.cmd->cmd = MW_CMD_SYS_STAT;
	d.cmd->data_len = 0;
	mw_cmd_send(d.cmd, NULL, NULL);

	// Carefully reuse the command timer
	d.tout_frames = tout_frames;
	d.stat_poll = TRUE;
	d.timer.timer_cb = assoc_poll_timer_cb;
	d.timer.auto_reload = TRUE;
	loop_timer_start(&d.timer, MW_STAT_POLL_TOUT);
	ret = loop_pend();

	// Restore default timer values
	d.timer.timer_cb = cmd_tout_cb;
	d.timer.auto_reload = FALSE;

	return ret < 0?MW_ERR_NOT_READY:MW_ERR_NONE;
}

enum mw_err mw_ap_disassoc(void)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_AP_LEAVE;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_def_ap_cfg(uint8_t slot)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->data_len = 1;
	d.cmd->cmd = MW_CMD_DEF_AP_CFG;
	d.cmd->data[0] = slot;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

int mw_def_ap_cfg_get(void)
{
	enum mw_err err;

	d.cmd->data_len = 0;
	d.cmd->cmd = MW_CMD_DEF_AP_CFG_GET;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return -1;
	}

	return d.cmd->data[0];
}

static int fill_addr(const char *dst_addr, const char *dst_port,
		const char *src_port, struct mw_msg_in_addr *in_addr)
{
	// Zero structure data
	memset(in_addr, 0, sizeof(struct mw_msg_in_addr));
	in_addr->dst_addr[0] = '\0';
	strcpy(in_addr->dst_port, dst_port);
	if (src_port) {
		strcpy(in_addr->src_port, src_port);
	}
	if (dst_addr && dst_port) {
		strcpy(in_addr->dst_addr, dst_addr);
		strcpy(in_addr->dst_port, dst_port);
	}

	// Length is the length of both ports, the channel and the address.
	return 6 + 6 + 1 + strlen(in_addr->dst_addr) + 1;
}

enum mw_err mw_tcp_connect(uint8_t ch, const char *dst_addr,
		const char *dst_port, const char *src_port)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}
	if (ch > MW_MAX_SOCK) {
		return MW_ERR_PARAM;
	}

	// Configure TCP socket
	d.cmd->cmd = MW_CMD_TCP_CON;
	d.cmd->data_len = fill_addr(dst_addr, dst_port, src_port,
			&d.cmd->in_addr);
	d.cmd->in_addr.channel = ch;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	// Enable channel
	lsd_ch_enable(ch);

	return MW_ERR_NONE;
}

enum mw_err mw_close(uint8_t ch)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	// TODO Check if used channel/socket
	d.cmd->cmd = MW_CMD_CLOSE;
	d.cmd->data_len = 1;
	d.cmd->data[0] = ch;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	// Disable channel
	lsd_ch_disable(ch);

	return MW_ERR_NONE;
}

enum mw_err mw_tcp_bind(uint8_t ch, uint16_t port)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	// TODO Check if used channel/socket
	d.cmd->cmd = MW_CMD_TCP_BIND;
	d.cmd->data_len = 7;
	d.cmd->bind.reserved = 0;
	d.cmd->bind.port = port;
	d.cmd->bind.channel = ch;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	// TODO This should maybe be done later.
	lsd_ch_enable(ch);

	return MW_ERR_NONE;
}

enum mw_err mw_udp_set(uint8_t ch, const char *dst_addr, const char *dst_port,
		const char *src_port)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}
	if (ch > MW_MAX_SOCK) {
		return MW_ERR_PARAM;
	}

	// Configure UDP socket
	d.cmd->cmd = MW_CMD_UDP_SET;
	d.cmd->data_len = fill_addr(dst_addr, dst_port, src_port,
			&d.cmd->in_addr);
	d.cmd->in_addr.channel = ch;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	// Enable channel
	lsd_ch_enable(ch);

	return MW_ERR_NONE;
}

static void sock_stat_reply_cb(enum lsd_status err, uint8_t ch, char *data,
		uint16_t len, void *ctx)
{
	UNUSED_PARAM(ctx);
	UNUSED_PARAM(len);
	UNUSED_PARAM(data);

	if (!err && MW_CTRL_CH == ch && d.stat_poll &&
			d.cmd->data[0] >= MW_SOCK_TCP_EST) {
		// Ready to send/receive data
		loop_timer_stop(&d.timer);
		d.stat_poll = FALSE;
		loop_post(1);
	} else {
		// Query the system status again
		d.cmd->cmd = MW_CMD_SOCK_STAT;
		d.cmd->data_len = 1;
		d.cmd->data[0] = d.monitor_ch;
		mw_cmd_send(d.cmd, NULL, NULL);
	}
}

static void sock_poll_timer_cb(struct loop_timer *t)
{
	if (d.tout_frames) {
		d.tout_frames -= MW_STAT_POLL_TOUT;
		if (d.tout_frames <= 0) {
			d.stat_poll = FALSE;
			loop_timer_stop(t);
			loop_post(-1);
			return;
		}
	}
	mw_cmd_recv(d.cmd, NULL, sock_stat_reply_cb);
}

enum mw_err mw_sock_conn_wait(uint8_t ch, int tout_frames)
{
	int ret;

	// Send command and do not look back
	d.monitor_ch = ch;
	d.cmd->cmd = MW_CMD_SOCK_STAT;
	d.cmd->data_len = 1;
	d.cmd->data[0] = ch;
	mw_cmd_send(d.cmd, NULL, NULL);

	// Carefully reuse the command timer
	d.tout_frames = tout_frames;
	d.stat_poll = TRUE;
	d.timer.timer_cb = sock_poll_timer_cb;
	d.timer.auto_reload = TRUE;
	loop_timer_start(&d.timer, MW_STAT_POLL_TOUT);
	ret = loop_pend();

	// Restore default timer values
	d.timer.timer_cb = cmd_tout_cb;
	d.timer.auto_reload = FALSE;

	return ret < 0?MW_ERR_NOT_READY:MW_ERR_NONE;
}

union mw_msg_sys_stat *mw_sys_stat_get(void)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return NULL;
	}

	d.cmd->cmd = MW_CMD_SYS_STAT;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}

	return &d.cmd->sys_stat;
}

enum mw_sock_stat mw_sock_stat_get(uint8_t ch)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return -1;
	}

	d.cmd->cmd = MW_CMD_SOCK_STAT;
	d.cmd->data_len = 1;
	d.cmd->data[0] = ch;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return -1;
	}

	return d.cmd->data[0];
}

// TODO Check for overflows when copying server data.
enum mw_err mw_sntp_cfg_set(const char *server[3], uint16_t up_delay,
		int8_t timezone, int8_t dst)
{
	enum mw_err err;
	int offset;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_SNTP_CFG;
	d.cmd->sntp_cfg.up_delay = up_delay;
	d.cmd->sntp_cfg.tz = timezone;
	d.cmd->sntp_cfg.dst = dst;
	strcpy(d.cmd->sntp_cfg.servers, server[0]);
	// Offset: server length + 1 ('\0')
	offset  = strlen(server[0]) + 1;
	strcpy(d.cmd->sntp_cfg.servers + offset, server[1]);
	offset += strlen(server[1]) + 1;
	strcpy(d.cmd->sntp_cfg.servers + offset, server[2]);
	offset += strlen(server[2]) + 1;
	// Mark the end of the list with two adjacent '\0'
	d.cmd->sntp_cfg.servers[offset] = '\0';
	d.cmd->data_len = offset + 1 + 4;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_sntp_cfg_get(char *server[3], uint16_t *up_delay,
		int8_t *timezone, int8_t *dst)
{
	enum mw_err err;
	int offset;
	int i;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_SNTP_CFG_GET;
	d.cmd->data_len = 0;

	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	if (up_delay) {
		*up_delay = d.cmd->sntp_cfg.up_delay;
	}
	if (timezone) {
		*timezone = d.cmd->sntp_cfg.tz;
	}
	if (dst) {
		*dst = d.cmd->sntp_cfg.dst;
	}

	server[0] = server[1] = server[2] = NULL;

	for (i = 0, offset = 0; i < 3; i++) {
		if (!d.cmd->sntp_cfg.servers[offset]) {
			goto out;
		}
		server[i] = d.cmd->sntp_cfg.servers + offset;
		offset += strlen(server[i]) + 1;
	}
out:
	return MW_ERR_NONE;
}

char *mw_date_time_get(uint32_t dt_bin[2])
{
	enum mw_err err;

	if (!d.mw_ready) {
		return NULL;
	}

	d.cmd->cmd = MW_CMD_DATETIME;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}

	if (dt_bin) {
		dt_bin = d.cmd->date_time.dt_bin;
	}
	// Set NULL termination of the string
	d.cmd->data[d.cmd->data_len] = '\0';

	return d.cmd->date_time.dt_str;
}

enum mw_err mw_flash_id_get(uint8_t id[3])
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_FLASH_ID;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	id[0] = d.cmd->data[0];
	id[1] = d.cmd->data[1];
	id[2] = d.cmd->data[2];

	return MW_ERR_NONE;
}

// sect = 0 corresponds to flash sector 0x80
enum mw_err mw_flash_sector_erase(uint16_t sect)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_FLASH_ERASE;
	d.cmd->data_len = sizeof(uint16_t);
	d.cmd->fl_sect = sect;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

// Address 0 corresponds to flash address 0x80000
enum mw_err mw_flash_write(uint32_t addr, uint8_t *data, uint16_t data_len)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_FLASH_WRITE;
	d.cmd->data_len = data_len + sizeof(uint32_t);
	d.cmd->fl_data.addr = addr;
	memcpy(d.cmd->fl_data.data, data, data_len);
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

// Address 0 corresponds to flash address 0x80000
uint8_t *mw_flash_read(uint32_t addr, uint16_t data_len)
{
	enum mw_err err;

	if (!d.mw_ready || data_len > d.buf_len) {
		return NULL;
	}

	d.cmd->cmd = MW_CMD_FLASH_READ;
	d.cmd->fl_range.addr = addr;
	d.cmd->fl_range.len = data_len;
	d.cmd->data_len = sizeof(struct mw_msg_flash_range);
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}

	return d.cmd->data;
}

uint8_t *mw_hrng_get(uint16_t rnd_len) {
	enum mw_err err;

	if (!d.mw_ready) {
		return NULL;
	}

	d.cmd->cmd = MW_CMD_HRNG_GET;
	d.cmd->data_len = sizeof(uint16_t);
	d.cmd->rnd_len = rnd_len;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}

	return d.cmd->data;
}

uint8_t *mw_bssid_get(enum mw_if_type interface_type)
{
	enum mw_err err;

	if (!d.mw_ready || interface_type >= MW_IF_MAX) {
		return NULL;
	}

	d.cmd->cmd = MW_CMD_BSSID_GET;
	d.cmd->data_len = 1;
	d.cmd->data[0] = interface_type;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}

	return d.cmd->data;
}

enum mw_err mw_gamertag_set(uint8_t slot, const struct mw_gamertag *gamertag)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_GAMERTAG_SET;
	d.cmd->gamertag_set.slot = slot;
	d.cmd->gamertag_set.reserved[0] = 0;
	d.cmd->gamertag_set.reserved[1] = 0;
	d.cmd->gamertag_set.reserved[2] = 0;
	d.cmd->data_len = sizeof(struct mw_gamertag_set_msg);
	memcpy(&d.cmd->gamertag_set.gamertag, gamertag,
			sizeof(struct mw_gamertag));
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

struct mw_gamertag *mw_gamertag_get(uint8_t slot)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return NULL;
	}

	d.cmd->cmd = MW_CMD_GAMERTAG_GET;
	d.cmd->data_len = 1;
	d.cmd->data[0] = slot;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return NULL;
	}

	return &d.cmd->gamertag_get;
}

enum mw_err mw_http_url_set(const char *url)
{
	enum mw_err err;
	size_t len;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	if (!url || !(len = strlen(url))) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_HTTP_URL_SET;
	d.cmd->data_len = len + 1;
	memcpy(d.cmd->data, url, len + 1);
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_http_method_set(enum mw_http_method method)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	if (method >= MW_HTTP_METHOD_MAX) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_HTTP_METHOD_SET;
	d.cmd->data_len = 1;
	d.cmd->data[0] = method;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_http_header_add(const char *key, const char *value)
{
	size_t key_len;
	size_t value_len;

	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	if (!key || !value || !(key_len = strlen(key)) ||
			!(value_len = strlen(value))) {
		return MW_ERR_PARAM;
	}

	key_len++;
	value_len++;
	d.cmd->cmd = MW_CMD_HTTP_HDR_ADD;
	d.cmd->data_len = key_len + value_len;
	memcpy(d.cmd->data, key, key_len);
	memcpy(d.cmd->data + key_len, value, value_len);
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_http_header_del(const char *key)
{
	enum mw_err err;
	size_t len;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	if (!key || !(len = strlen(key))) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_HTTP_HDR_DEL;
	d.cmd->data_len = len + 1;
	memcpy(d.cmd->data, key, len + 1);
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_http_open(uint32_t content_len)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_HTTP_OPEN;
	d.cmd->data_len = 4;
	d.cmd->dw_data[0] = content_len;
	err = mw_command(MW_HTTP_OPEN_TOUT);
	if (err) {
		return MW_ERR;
	}

	lsd_ch_enable(MW_HTTP_CH);
	return MW_ERR_NONE;
}

int mw_http_finish(uint32_t *content_len, int tout_frames)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	if (!content_len) {
		return MW_ERR_PARAM;
	}

	d.cmd->cmd = MW_CMD_HTTP_FINISH;
	d.cmd->data_len = 0;
	err = mw_command(tout_frames);
	if (err) {
		return MW_ERR;
	}

	*content_len = d.cmd->dw_data[0];
	return d.cmd->w_data[2];
}

uint32_t mw_http_cert_query(void)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return 0xFFFFFFFF;
	}

	d.cmd->cmd = MW_CMD_HTTP_CERT_QUERY;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return 0xFFFFFFF;
	}

	return d.cmd->dw_data[0];
}

enum mw_err mw_http_cert_set(uint32_t cert_hash, const char *cert,
		uint16_t cert_len)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	if (cert_len && !cert) {
		return MW_ERR_PARAM;
	}
	d.cmd->cmd = MW_CMD_HTTP_CERT_SET;
	d.cmd->data_len = 6;
	d.cmd->dw_data[0] = cert_hash;
	d.cmd->w_data[2] = cert_len;
	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	lsd_ch_enable(MW_HTTP_CH);
	// Command succeeded, now send the certificate using MW_CH_HTTP
	err = mw_send_sync(MW_HTTP_CH, cert, cert_len, 0);
	lsd_ch_disable(MW_HTTP_CH);

	return MW_ERR_NONE;
}

int mw_http_cleanup(void)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_HTTP_FINISH;
	d.cmd->data_len = 0;
	err = mw_command(MW_COMMAND_TOUT);
	lsd_ch_disable(MW_HTTP_CH);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_log(const char *msg)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_LOG;
	d.cmd->data_len = strlen(msg) + 1;
	memcpy(d.cmd->data, msg, d.cmd->data_len);

	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

enum mw_err mw_factory_settings(void)
{
	enum mw_err err;

	if (!d.mw_ready) {
		return MW_ERR_NOT_READY;
	}

	d.cmd->cmd = MW_CMD_FACTORY_RESET;
	d.cmd->data_len = 0;

	err = mw_command(MW_COMMAND_TOUT);
	if (err) {
		return MW_ERR;
	}

	return MW_ERR_NONE;
}

void mw_power_off(void)
{
	d.cmd->cmd = MW_CMD_SLEEP;
	d.cmd->data_len = 0;

	mw_cmd_send(d.cmd, NULL, NULL);
}

void mw_sleep(uint16_t frames)
{
	loop_timer_start(&d.timer, frames);
	loop_pend();
}

