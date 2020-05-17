/************************************************************************//**
 * \brief GameJolt Game API implementation for MegaWiFi.
 *
 * \author Jesus Alonso (doragasu)
 * \date 2020
 ****************************************************************************/
#include <string.h>
#include "megawifi.h"
#include "gamejolt.h"

static const char * const trophy_strings[] = {
	"Bronze", "Silver", "Gold", "Platinum", "Unknown"
};

enum boolean {
	BOOL_ERROR = -1,
	BOOL_FALSE =  0,
	BOOL_TRUE  =  1
};

struct {
	char *buf;
	uint16_t buf_len;
} gj = {};

static char *line_next(const char *data)
{
	char *next = strchr(data, '\n');

	if (next) {
		next++;
	}

	return next;
}

// If there is a match, *value is set to the match, and the pointer to the next
// line is returned. If the match is on last line, NULL is returned. If there
// is no match, or there is an error, *value is set to NULL and NULL is
// returned.
static char *val_get(char *line, const char *key, char **value)
{
	int len = strlen(key);
	char *val = NULL;
	char *endval = NULL;

	if (!line) {
		goto out;
	}
	if (memcmp(line, key, len) || ':' != line[len] ||
			'"' != line[len + 1]) {
		goto out;
	}

	val = line + len + 2;
	endval = strchr(val, '"');
	if (!endval) {
		val = NULL;
		goto out;
	}
	*endval = '\0';
	endval++;
	if ('\r' == endval[0] && '\n' == endval[1]) {
		endval += 2;
	} else {
		endval = NULL;
	}

out:
	*value = val;
	return endval;
}

static char *key_val_check(char *line, const char *key,
		const char *value, bool *match)
{
	char *next;
	char *val;

	*match = false;
	next = val_get(line, key, &val);

	if (!val) {
		goto out;
	}

	if (!strcmp(value, val)) {
		*match = true;
	}

out:
	return next;
}

static char *key_bool_get(char *line, const char *key, enum boolean *result)
{
	char *next;
	char *val;

	*result = BOOL_ERROR;
	next = val_get(line, key, &val);

	if (!val) {
		goto out;
	}

	if (!strcmp(val, "false")) {
		*result = BOOL_FALSE;
	} else if (!strcmp(val, "true")) {
		*result = BOOL_TRUE;
	}

out:
	return next;
}

bool gj_init(const char *endpoint, const char *game_id, const char *private_key,
		const char *username, const char *user_token, char *reply_buf,
		uint16_t buf_len)
{
	const char *key[4] = {"game_id", "username", "user_token", "format"};
	const char *value[4] = {game_id,  username,   user_token,  "keypair"};

	gj.buf = reply_buf;
	gj.buf_len = buf_len;

	if (mw_ga_endpoint_set(endpoint, private_key) ||
			mw_ga_key_value_add(key, value, 4)) {
		return true;
	}

	return false;
}

void gj_flush(uint32_t remaining, uint16_t tout_frames)
{
	int16_t to_recv;
	uint8_t ch = MW_HTTP_CH;

	while(remaining) {
		to_recv = MIN(gj.buf_len, remaining);
		if (MW_ERR_NONE != mw_recv_sync(&ch, gj.buf, &to_recv,
					tout_frames)) {
			return;
		}
		remaining -= to_recv;
	}
}

char *gj_recv(uint32_t *len, uint16_t tout_frames)
{
	uint16_t pos = 0;
	int16_t buf_len;
	uint8_t ch = MW_HTTP_CH;

	// Note:
	// Reception from GameJolt is a bit problematic on embedded devices,
	// because data is sent using chunked transfers, and esp_http_client
	// does not allow retrieving in advance the length of each data chunk.
	if (*len > (uint32_t)(gj.buf_len - 1)) {
		return NULL;
	}

	while (pos < *len) {
		buf_len = gj.buf_len - pos;
		if (MW_ERR_NONE != mw_recv_sync(&ch, gj.buf + pos,
					&buf_len, tout_frames)) {
			return NULL;
		}
		if (0 == buf_len) {
			// Server closed the connection
			break;
		}
		pos += buf_len;
	}

	gj.buf[pos++] = '\0';
	*len = pos;

	return gj.buf;
}

char *gj_request(const char **path, uint8_t num_paths, const char **key,
		const char **value, uint8_t num_kv_pairs, uint16_t tout_frames,
		uint32_t *out_len)
{
	char *reply;
	int status = mw_ga_request(MW_HTTP_METHOD_GET, path, num_paths, key,
			value, num_kv_pairs, out_len, tout_frames);


	if (status < 100) {
		return NULL;
	}
	if (status < 200 || status >= 300) {
		gj_flush(*out_len, tout_frames);
		return NULL;
	}
	if (INT32_MAX == *out_len) {
		// Chunked response, limit to the buffer length minus 1
		// character for proper string termination
		*out_len = gj.buf_len - 1;
	}

	reply = gj_recv(out_len, tout_frames);
	if (reply) {
		enum boolean success;
		char *aux = key_bool_get(reply, "success", &success);
		if (success != BOOL_TRUE) {
			return NULL;
		}
		*out_len -= aux - reply;
		reply = aux;
	}
	return reply;
}

char *gj_trophies_fetch(bool achieved, const char *trophy_id,
		uint16_t tout_frames)
{
	const char *path = "trophies";
	const char *key[2] = {NULL, NULL};
	const char *val[2] = {NULL, NULL};
	uint8_t kv_idx = 0;
	uint32_t reply_len;

	if (achieved) {
		key[kv_idx] = "achieved";
		val[kv_idx] = "true";
		kv_idx++;
	}

	if (trophy_id) {
		key[kv_idx] = "trophy_id";
		val[kv_idx] = trophy_id;
		kv_idx++;
	}

	return gj_request(&path, 1, key, val, kv_idx, tout_frames, &reply_len);
}

static enum gj_trophy_difficulty get_trophy(const char *difficulty_str)
{
	enum gj_trophy_difficulty difficulty = GJ_TROPHY_TYPE_UNKNOWN;

	for (int i = 0; i < GJ_TROPHY_TYPE_UNKNOWN; i++) {
		if (!strcmp(difficulty_str, trophy_strings[i])) {
			difficulty = i;
			break;
		}
	}

	return difficulty;
}

static char *decode_string(char *data, const char *key, char **output)
{
	data = val_get(data, key, output);
	if (!*output) {
		return NULL;
	}

	return data;
}

static char *decode_trophy_difficulty(char *data, const char *key,
		enum gj_trophy_difficulty *output)
{
	char *value;

	data = decode_string(data, key, &value);
	if (!value) {
		return NULL;
	}
	*output = get_trophy(value);
	if (GJ_TROPHY_TYPE_UNKNOWN == *output) {
		return NULL;
	}

	return data;
}

static char *decode_boolean(char *data, const char *key, bool *output)
{
	char *value;

	data = decode_string(data, key, &value);
	if (!data) {
		return NULL;
	}

	if (!strcmp(value, "false")) {
		*output = BOOL_FALSE;
	} else if (!strcmp(value, "true")) {
		*output = BOOL_TRUE;
	} else {
		*output = BOOL_ERROR;
		return NULL;
	}

	return data;
}

#define X_AS_DECODER(field, decoder, type) \
	pos = decode_ ## decoder(pos, #field, &trophy->field); \
	if (!pos) { return NULL; }

char *gj_trophy_get_next(char *data, struct gj_trophy *trophy)
{
	char *pos = data;

	if (!data || !data[0]) {
		NULL;
	}

	memset(trophy, 0, sizeof(struct gj_trophy));

	// Expand decoders
	GJ_TROPHY_RESPONSE_TABLE(X_AS_DECODER);

	if ('\0' == trophy->description[0]) {
		trophy->secret = true;
	};

	return pos;
}

const char *gj_trophy_difficulty_str(enum gj_trophy_difficulty difficulty)
{
	if (difficulty < 0 || difficulty > GJ_TROPHY_TYPE_UNKNOWN) {
		difficulty = GJ_TROPHY_TYPE_UNKNOWN;
	}

	return trophy_strings[difficulty];
}

bool gj_trophy_add_achieved(const char *trophy_id, uint16_t tout_frames)
{
	const char *path[2] = {"trophies", "add-achieved"};
	const char *key = "trophy_id";
	uint32_t reply_len;

	return !gj_request(path, 2, &key, &trophy_id, 1,
			tout_frames, &reply_len);
}

bool gj_trophy_remove_achieved(const char *trophy_id, uint16_t tout_frames)
{
	const char *path[2] = {"trophies", "remove-achieved"};
	const char *key = "trophy_id";
	uint32_t reply_len;

	return !gj_request(path, 2, &key, &trophy_id, 1,
			tout_frames, &reply_len);
}

