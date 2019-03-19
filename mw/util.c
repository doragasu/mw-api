#include "util.h"

const char *str_is_uint8(const char *str)
{
	uint8_t i;

	// Skip leading zeros
	while (str[0] == '0' && (str[1] >= '0' && str[1] <= '9')) {
		str++;
	}
	// Determine number length (up to 4 characters)
	for (i = 0; (i < 4) && (str[i] >= '0') && (str[i] <= '9'); i++);

	switch (i) {
	// If number is 3 characters, the number fits in 8 bits only if
	// lower than 255
	case 3:
		if ((str[0] > '2') || ((str[0] == '2') && ((str[1] > '5') ||
				((str[1] == '5') && (str[2] > '5'))))) {
			return NULL;
		}

		// If length is 2 or 1 characters, the number fits in 8 bits.
		// fallthrough
	case 2:
	case 1:
		return str + i;

		// If length is 4 or more, number does not fit in 8 bits.
	default:
		return NULL;
	}
}

int ip_validate(const char *str)
{
	int8_t i;

	// Evaluate if we have 4 numbers fitting in a byte, separated by '.'
	if (!(str = str_is_uint8(str))) return FALSE;

	for (i = 2; i >= 0; i--) {
		if (*str != '.') {
			return FALSE;
		}
		str++;
		if (!(str = str_is_uint8(str))) {
			return FALSE;
		}
	}

	if (*str != '\0') {
		return FALSE;	
	}
	return TRUE;
}

/// Convert an IPv4 address in string format to binary format
uint32_t ip_str_to_uint32(const char *ip)
{
	uint32_t bin_ip;
	uint8_t *byte = (uint8_t*)&bin_ip;
	int i;

	for (i = 0; i < 4; i++) {
		if ((ip = str_to_uint8(ip, &byte[i])) == NULL) {
			return 0;
		}
		ip++;
	}
	return bin_ip;
}

/// Converts an IP address in uint32_t binary representation to
int uint32_to_ip_str(uint32_t ip_u32, char *ip_str)
{
	uint8_t *byte = (uint8_t*)&ip_u32;
	int pos = 0;
	int i;

	for (i = 0; i < 3; i++) {
		pos += uint8_to_str(byte[i], ip_str + pos);
		ip_str[pos++] = '.';
	}
	pos += uint8_to_str(byte[i], ip_str + pos);
	ip_str[pos] = '\0';

	return pos;
}

uint8_t uint8_to_str(uint8_t num, char *str)
{
	uint8_t i = 0;
	uint8_t tmp;

	// Compute digits and write decimal number
	// On 3 digit numbers, first one can only be 1 or 2. Take advantage of
	// this to avoid division (TODO test if this is really faster).
	if (num > 199) {
		str[i++] = '2';
		num -= 200;
	} else if (num > 99) {
		str[i++] = '1';
		num -= 100;
	}
	
	tmp = num / 10;
	if (tmp) {
		str[i++] = '0' + tmp;
	}
	str[i++] = '0' + num % 10;
	str[i] = '\0';

	return i;
}

int8_t int8_to_str(int8_t num, char *str)
{
	int i = 0;

	if (num < 0) {
		num = -num;
		str[i++] = '-';
	}
	i += uint8_to_str(num, str + i);

	return i;
}

const char *str_to_uint8(const char *str, uint8_t *result)
{
	uint8_t i;

	*result = 0;

	// Skip leading zeros
	while (*str == '0') str++;
    // Special case: number is zero
    if (*str < '0' || *str > '9') return str;
	// Determine number length (up to 4 characters)
	for (i = 0; (i < 4) && (str[i] >= '0') && (str[i] <= '9'); i++);
	
	switch (i) {
	// If number is 3 characters, the number fits in 8 bits only if
	// lower than 256
	case 3:
		if ((str[0] > '2') || ((str[0] == '2') && ((str[1] > '5') ||
					((str[1] == '5') && (str[2] > '5')))))
			return NULL;
		else {
			*result = ((*str) - '0') * 100;
			str++;
		}
		// fallthrough
	case 2:
		*result += ((*str) - '0') * 10;
		str++;
		// fallthrough
	case 1:
		*result += (*str) - '0';
		str++;
		break;

	// If length is 4 or more, number does not fit in 8 bits.
	default:
		return NULL;
	}
	return str;
}

int long_to_str(long num, char *str, int buf_len, int pad_len, char pad_chr)
{
	int i = 0;
	int j;
	int rem;
	int len = 0;

	// Obtain string length
	for (rem = num, len = 0; rem; len++, rem /= 10);
	// if number is 0 or negative, increase length by 1
	if (len == 0) {
		len++;
		str[i++] = '0';
	}
	if (num < 0) {
		len++;
		num = -num;
		str[i++] = '-';
	}
	// Check number fits in buffer
	if (((len + 1) > buf_len) || ((pad_len + 1) > buf_len)) {
		return 0;
	}
	for (; i < (pad_len - len); i++) {
		str[i] = pad_chr;
	}

	// Perform the conversion in reverse order
	pad_len = MAX(pad_len, len);
	str[pad_len] = '\0';
	for (j = pad_len - 1;j >= i; j--) {
		str[j] = '0' + num % 10;
		num = num / 10;
	}

	return pad_len;
}

int uint32_to_hex_str(uint32_t num, char *str, int pad)
{
	const char map[] = "0123456789ABCDEF";
	int i;
	int nibble;
	int off;

	for (i = 0, nibble = 7; nibble >= 0; nibble--) {
		off = nibble<<2;
		if ((num>>off) & 0xF) {
			str[i++] = map[(num>>off) & 0xF];
		} else if ((i > 0) || (nibble < pad) || ((!i) && (!nibble))){
			str[i++] = '0';
		}
	}
	str[i] = '\0';
	return i;
}

