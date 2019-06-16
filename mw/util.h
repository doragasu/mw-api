/************************************************************************//**
 * \file
 *
 * \brief General purpose utilities.
 *
 * \defgroup util util
 * \{
 *
 * \brief General purpose utilities.
 ****************************************************************************/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include <stddef.h>

#ifndef TRUE
/// TRUE value for boolean comparisons
#define TRUE 1
#endif
#ifndef FALSE
/// TRUE value for boolean comparisons
#define FALSE 0
#endif

#ifndef NULL
/// NULL Pointer
#define NULL ((void*)0)
#endif

/// Returns TRUE if number is in the specified range
#define IN_RANGE(num, lower, upper)					\
	(((number) >= (lower)) && ((number) <= (upper)))

/// The infamous container_of() macro directly the Linux kernel
#define container_of(ptr, type, member) ({				\
		const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) );})

/// Macro for packing structures and enumerates
#define PACKED		__attribute__((__packed__))

/// Section attribute definition for variables and functions. Examples:
/// - int a SECTION(data);
/// - void foo(void) SECTION(text);
#define SECTION(name)	__attribute__((section(#name)))

/// Get number of rows of a 2D array
#define ARRAY_ROWS(array_2d)		(sizeof(array_2d) / sizeof(array_2d[0]) / sizeof(array_2d[0][0]))
/// Get number of columns of a 2D array
#define ARRAY_COLS(array_2d)		(sizeof(array_2d[0]) / sizeof(array_2d[0][0]))

/// Remove compiler warnings when not using a function parameter
#define UNUSED_PARAM(x)		(void)x

#if !defined(MAX)
/// Returns the maximum of two numbers
#define MAX(a, b)	((a)>(b)?(a):(b))
#endif
#if !defined(MIN)
/// Returns the minimum of two numbers
#define MIN(a, b)	((a)<(b)?(a):(b))
#endif

/// Swaps bytes from a word (16 bit)
#define ByteSwapWord(w)	(uint16_t)((((uint16_t)(w))>>8) | (((uint16_t)(w))<<8))

/// Swaps bytes from a dword (32 bits)
#define ByteSwapDWord(dw)	(uint32_t)((((uint32_t)(dw))>>24) |               \
		((((uint32_t)(dw))>>8) & 0xFF00) | ((((uint32_t)(dw)) & 0xFF00)<<8) | \
	  	(((uint32_t)(dw))<<24))

/************************************************************************//**
 * \brief Converts input string to uppercase.
 *
 * \param[inout] str String to be converted to uppercase.
 ****************************************************************************/
static inline void to_upper(char *str) {
	uint16_t i;
	for (i = 0; str[i] != '\0'; i++) {
		if ((str[i] >= 'a') && (str[i] <= 'z'))
			str[i] -= 'a' - 'A';
	}
}

/************************************************************************//**
 * \brief Evaluates if a string points to a number that can be stored in a
 * uint8_t type variable.
 *
 * \param[in] str String to be evaluated as a number.
 *
 * \return The pointer to the character following the last digit of the
 *         number, if the string represents a number fittint in a uint_8.
 *         NULL if the string does not represent an uint8_t number.
 ****************************************************************************/
const char *str_is_uint8(const char *str);

/************************************************************************//**
 * \brief This function evaluates the data entered on the input Menu
 * structure, to guess if it corresponds to a valid IPv4.
 *
 * \param[in] str string to evaluate against an IPv4 pattern.
 *
 * \return TRUE if the evaluated string corresponds to a valid IPv4. False
 *         otherwise.
 ****************************************************************************/
int ip_validate(const char *str);

/************************************************************************//**
 * \brief Writes the corresponding string representing an IPv4 stored in the
 * input DWORD (32-bit) integer.
 *
 * \param[in]  ip_u32 Input DWORD to translate into string.
 * \param[out] ip_str Resulting string matching the input DWORD.
 *
 * \return Length of the resulting IP string.
 ****************************************************************************/
int uint32_to_ip_str(uint32_t ip_u32, char *ip_str);

/************************************************************************//**
 * \brief Returns the binary IP addres (uint32) corresponding to the input
 * string.
 *
 * \param[in]  ip String representing an IPv4 address.
 *
 * \return Binary representation of the input IPv4 string.
 ****************************************************************************/
uint32_t ip_str_to_uint32(const char *ip);

/************************************************************************//**
 * \brief Converts an unsigned 8-bit number to its character
 * string representation.
 *
 * \param[in]  num Input number to convert.
 * \param[out] str String representing the input number.
 *
 * \return Resulting str length (not including null termination).
 * \note str buffer length shall be at least 4 bytes.
 ****************************************************************************/
uint8_t uint8_to_str(uint8_t num, char *str);

/************************************************************************//**
 * \brief Converts an signed 8-bit number to its character
 * string representation.
 *
 * \param[in]  num Input number to convert.
 * \param[out] str String representing the input number.
 *
 * \return Resulting str length (not including null termination).
 * \note str buffer length shall be at least 5 bytes.
 ****************************************************************************/
int8_t int8_to_str(int8_t num, char *str);

/************************************************************************//**
 * \brief Converts a character string representing an 8-bit unsigned number,
 * to its binary (uint8_t) representation.
 *
 * \param[in]  strIn  Input string with the number to convert.
 * \param[out] result Converted number will be left here.
 *
 * \return Pointer to the end of the number received in strIn parameter, or
 * NULL if the strIn does not contain a valid string representation of an
 * uint8_t type.
 ****************************************************************************/
const char *str_to_uint8(const char *strIn, uint8_t *result);

/************************************************************************//**
 * \brief Converts an integer to a character string.
 *
 * \param[in]  num  Number to convert to string.
 * \param[out] str  String that will hold the converted number.
 * \param[in]  buf_len Length of str buffer.
 * \param[in]  pad_len Length of the padding to introduce. 0 for no padding.
 * \param[in]  pad_chr Character used for padding (typically '0' or ' ').
 *
 * \return Number of characters written to str buffer, not including the
 * null termination. 0 if string does not fin in the buffer and has not
 * been converted.
 *
 * \warning Function uses lots of divisions. Maybe it is not the best of the
 * ideas using it in a game loop.
 ****************************************************************************/
int long_to_str(long num, char *str, int buf_len, int pad_len, char pad_chr);

/************************************************************************//**
 * \brief Converts a 8-bit number to its hexadecimal string representation.
 *
 * \param[in]  num Number to convert.
 * \param[out] str Converted equivalent string. Must have room for at least
 *             3 characters to guarantee an overrun will not accur.
 ****************************************************************************/
void uint8_to_hex_str(uint8_t num, char *str);

/************************************************************************//**
 * \brief Converts a 32-bit number to its hexadecimal string representation.
 *
 * \param[in]  num Number to convert.
 * \param[out] str Converted equivalent string. Must have room for at least
 *             9 characters to guarantee an overrun will not accur.
 * \param[in]  pad Padding. If greater than 0, left part of resulting number
 *             will be zero-padded to the specified length.
 *
 * \return Number of characters of the resulting converted string, not
 *         including the null termination.
 ****************************************************************************/
int uint32_to_hex_str(uint32_t num, char *str, int pad);

#endif //_UTIL_H_

/** \} */

