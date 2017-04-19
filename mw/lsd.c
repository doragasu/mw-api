/************************************************************************//**
 * \brief  Local Symmetric Data-link. Implements an extremely simple
 *         protocol to link two full-duplex devices, multiplexing the
 *         data link.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 * \todo   Implement UART RTS/CTS handshaking.
 * \todo   Current implementation uses polling. Unfortunately as the Genesis/
 *         Megadrive does not have an interrupt pin on the cart, implementing
 *         more efficient data transmission techniques will be tricky.
 * \todo   Proper implementation of error handling.
 ****************************************************************************/
#include "lsd.h"
#include "util.h" 
/// Number of buffer frames available
#define LSD_BUF_FRAMES			2

/// Start of data in the buffer (skips STX and LEN fields).
#define LSD_BUF_DATA_START 		3

/// LsdState Allowed states for reception state machine.
typedef enum {
	LSD_ST_IDLE = 0,		///< Currently inactive
	LSD_ST_STX_WAIT,		///< Waiting for STX
	LSD_ST_CH_LENH_RECV,	///< Receiving channel and length (high bits)
	LSD_ST_LEN_RECV,		///< Receiving frame length
	LSD_ST_DATA_RECV,		///< Receiving data length
	LSD_ST_ETX_RECV,		///< Receiving ETX
	LSD_ST_MAX				///< Number of states
} LsdState;

/// Local data required by the module.
typedef struct {
	//MwMsgBuf rx[LSD_BUF_FRAMES];	///< Reception buffers
	LsdState rxs;					///< Reception state
	LsdState txs;					///< Send state
	uint8_t en[LSD_MAX_CH];			///< Channel enable
	uint16_t pos;					///< Position in current buffer
	uint8_t current;				///< Current buffer in use
} LsdData;

/// Module global data
static LsdData d;

/// \note Loop count is reset each time a data segment is successfully copied
///       to the TX FIFO.
static inline int LsdPollSend(uint8_t data[], uint16_t len,
							   uint32_t maxLoopCnt) {
	int16_t i;
	uint8_t n;
	uint32_t loopCnt = maxLoopCnt;

	// Buffer is sent in chunks of up to UART_TX_FIFO_LEN bytes.
	// Only the last chunck gets some advantage of the FIFOs :(
	for (i = 0; i < len;) {
		n = MIN(UART_TX_FIFO_LEN, len - i);
		while (!UartTxReady() && loopCnt) loopCnt--;
		if (!maxLoopCnt) return -1;
		while (n--) UartPutc(data[i++]);
		loopCnt = maxLoopCnt;
	}
	return len;
}

/************************************************************************//**
 * Module initialization. Call this function before any other one in this
 * module.
 ****************************************************************************/
void LsdInit(void) {
	uint8_t i;

	d.rxs = d.txs = LSD_ST_IDLE;
	d.pos = d.current = 0;
	for (i = 0; i < LSD_MAX_CH; i++) {
		d.en[i] = FALSE;
	}
	UartInit();
}

/************************************************************************//**
 * Enables a channel to start reception and be able to send data.
 *
 * \param[in] ch Channel number.
 *
 * \return A pointer to an empty TX buffer, or NULL if no buffer is
 *         available.
 ****************************************************************************/
int LsdChEnable(uint8_t ch) {
	if (ch >= LSD_MAX_CH) return LSD_ERROR;

	d.en[ch] = TRUE;
	return LSD_OK;
}

/************************************************************************//**
 * Disables a channel to stop reception and prohibit sending data.
 *
 * \param[in] ch Channel number.
 *
 * \return A pointer to an empty TX buffer, or NULL if no buffer is
 *         available.
 ****************************************************************************/
int LsdChDisable(uint8_t ch) {
	if (ch >= LSD_MAX_CH) return LSD_ERROR;

	d.en[ch] = FALSE;

	return LSD_OK;
}


/************************************************************************//**
 * Sends data through a previously enabled channel.
 *
 * \param[in] data Buffer to send.
 * \param[in] len  Length of the buffer to send.
 * \param[in] ch   Channel number to use.
 * \param[in] maxLoopCnt Maximum number of loops trying to write data.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise. Note returned value might be 0 if no characters were
 * 		   sent due to maxLoopCnt value reached (timeout).
 *
 * \note   maxLoopCnt value is only used for the wait before starting
 *         sending the frame header. For sending the data payload and the
 *         ETX, UINT32_MAX value is used for loop counts. If tighter control
 *         of the timing is necessary, frame must be sent using split
 *         functions.
 ****************************************************************************/
int LsdSend(uint8_t *data, uint16_t len, uint8_t ch, uint32_t maxLoopCnt) {
	if (ch >= LSD_MAX_CH) {
		return -1;
	}
	if (len > LSD_MAX_LEN) {
		return -1;
	}
	if (!d.en[ch]) {
		return -1;
	}

	while ((!UartTxReady()) && maxLoopCnt) maxLoopCnt--;
	if (!maxLoopCnt) return 0;
	// Send STX
	UartPutc(LSD_STX_ETX);
	// Send ch and high nibble of length
	UartPutc((ch<<4) | (len>>8));
	// Send low byte of length
	UartPutc(len & 0xFF);
	// Send data payload
	if (LsdPollSend(data, len, UINT32_MAX) != len) return -1;
	// Send ETX
	// TODO: It's stupid using FIFOs to end up doing this:
	for (maxLoopCnt = UINT32_MAX; (!UartTxReady()) && maxLoopCnt; maxLoopCnt--);
	if (!maxLoopCnt) return -1;
	UartPutc(LSD_STX_ETX);
	
	return len;
}

/************************************************************************//**
 * Starts sending data through a previously enabled channel. Once started,
 * you can send more additional data inside of the frame by issuing as
 * many LsdSplitNext() calls as needed, and end the frame by calling
 * LsdSplitEnd().
 *
 * \param[in] data  Buffer to send.
 * \param[in] len   Length of the data buffer to send.
 * \param[in] total Total length of the data to send using a split frame.
 * \param[in] ch    Channel number to use for sending.
 * \param[in] maxLoopCnt Maximum number of loops trying to write data.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 *
 * \note     maxLoopCnt is only used for the wait before starting sending
 *           the frame header. Optional data field is sent using UINT32_MAX
 *           as loop count.
 ****************************************************************************/
int LsdSplitStart(uint8_t *data, uint16_t len,
	              uint16_t total, uint8_t ch, uint32_t maxLoopCnt) {
	if (ch >= LSD_MAX_CH) return -1;
	if (total > LSD_MAX_LEN) return -1;
	if (!d.en[ch]) return -1;

	while (!UartTxReady() && maxLoopCnt) maxLoopCnt--;
	if (!maxLoopCnt) return -1;
	// Send STX
	UartPutc(LSD_STX_ETX);
	// Send ch and high nibble of length
	UartPutc((ch<<4) | (len>>8));
	// Send low byte of length
	UartPutc(len & 0xFF);
	// Send data payload
	if (len) return LsdPollSend(data, len, UINT32_MAX) != len?-1:len;
	return 0;
}

/************************************************************************//**
 * Appends (sends) additional data to a frame previously started by an
 * LsdSplitStart() call.
 *
 * \param[in] data  Buffer to send.
 * \param[in] len   Length of the data buffer to send.
 * \param[in] maxLoopCnt Maximum number of loops trying to write data.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 ****************************************************************************/
int LsdSplitNext(uint8_t *data, uint16_t len, uint32_t maxLoopCnt) {
	return LsdPollSend(data, len, maxLoopCnt);

	return len;
}

/************************************************************************//**
 * Appends (sends) additional data to a frame previously started by an
 * LsdSplitStart() call, and finally ends the frame.
 *
 * \param[in] data  Buffer to send.
 * \param[in] len   Length of the data buffer to send.
 * \param[in] maxLoopCnt Maximum number of loops trying to write data.
 *
 * \return -1 if there was an error, or the number of characterse sent
 * 		   otherwise.
 ****************************************************************************/
int LsdSplitEnd(uint8_t *data, uint16_t len, uint32_t maxLoopCnt) {
	if (len) {
		if (LsdPollSend(data, len, maxLoopCnt) != len) return -1;
	}
	// Send ETX
	// TODO: It's stupid using FIFOs to end up doing this:
	while (!UartTxReady() && maxLoopCnt) maxLoopCnt--;
	if (!maxLoopCnt) return -1;
	UartPutc(LSD_STX_ETX);

	return len;
}


/************************************************************************//**
 * Receives a frame using LSD protocol.
 *
 * \param[out]   buf Buffer that will hold the received data.
 * \param[inout] maxLen When calling the function, the variable pointed by
 *               maxLen, must hold the maximum number of bytes buf can
 *               store. On return, the variable is updated to the number
 *               of bytes received.
 * \param[in]    maxLoopCnt Maximum number of loops trying to read data.
 *
 * \return On success, the number of the channel in which data has been
 * 		   received. On failure, a negative number.
 ****************************************************************************/
// Returns the channel number
//int LsdRecv(MwMsgBuf* buf, uint16_t maxLen, uint32_t maxLoopCnt) {
int LsdRecv(uint8_t* buf, uint16_t* maxLen, uint32_t maxLoopCnt) {
	LsdState rxs = LSD_ST_STX_WAIT;
	uint32_t loops;
	uint16_t pos = 0;
	uint16_t len = 0;
	uint8_t recv;
	int8_t ch = -1;

	while (1) {
		// Receive a character
		loops = maxLoopCnt;
		while (!UartRxReady()) {
			loops--;
			if (!loops) return -1;
		}
		recv = UartGetc();
		switch (rxs) {
			case LSD_ST_IDLE:			// Do nothing!
				return -1;
	
			case LSD_ST_STX_WAIT:		// Wait for STX to arrive
				if (LSD_STX_ETX == recv) rxs = LSD_ST_CH_LENH_RECV;
				break;
	
			case LSD_ST_CH_LENH_RECV:	// Receive CH and len high
				// Check special case: if we receive STX and pos == 0,
				// then this is the real STX (previous one was ETX from
				// previous frame!).
				if (!(LSD_STX_ETX == recv && 0 == pos)) {
					ch = recv>>4;
					len = (recv & 0x0F)<<8;
					// Sanity check (not exceding number of channels)
					if (ch >= LSD_MAX_CH) {
						return -1;
					}
					else rxs = LSD_ST_LEN_RECV;
				}
				break;
	
			case LSD_ST_LEN_RECV:		// Receive len low
				len |= recv;
				// Sanity check (not exceeding maximum buffer length)
				if (len > *maxLen) {
					return -1;
				}
				// If there's payload, receive it. Else wait for ETX
				if (len) {
					pos = 0;
					rxs = LSD_ST_DATA_RECV;
				} else {
					rxs = LSD_ST_ETX_RECV;
				}
				break;
	
			case LSD_ST_DATA_RECV:		// Receive payload
				buf[pos++] = recv;
				if (pos >= len) rxs = LSD_ST_ETX_RECV;
				break;
	
			case LSD_ST_ETX_RECV:		// ETX should come here
				if (LSD_STX_ETX == recv) {
					*maxLen = pos;
					return ch;
				}
				// Error, ETX not received.
				return -1;
	
			default:
				// Code should never reach here!
				return -1;
		} // switch(d.rxs)
	} // while (1)
}
