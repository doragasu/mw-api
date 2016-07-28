#ifndef _MW_MSG_H_
#define _MW_MSG_H_

#include <stdint.h>

/// Maximum buffer length (bytes)
#define MW_MSG_MAX_BUFLEN	512

/// Maximum data payload size of a command
#define MW_MAX_CMD_DATALEN 	512

/** \addtogroup MwMsg Cmds Supported commands.
 *  \{ */
#define MW_CMD_OK			  0
#define MW_CMD_VERSION        1
#define MW_CMD_ECHO			  2
#define MW_CMD_AP_SCAN		  3
#define MW_CMD_AP_CFG		  4
#define MW_CMD_AP_CFG_GET	  5
#define MW_CMD_IP_CFG		  6
#define MW_CMD_IP_CFG_GET	  7
#define MW_CMD_AP_JOIN		  8
#define MW_CMD_AP_LEAVE		  9
#define MW_CMD_TCP_CON		 10
#define MW_CMD_TCP_BIND		 11
#define MW_CMD_TCP_ACCEPT	 12
#define MW_CMD_TCP_STAT		 13
#define MW_CMD_TCP_DISC		 14
#define MW_CMD_UDP_SET		 15
#define MW_CMD_UDP_STAT		 16
#define MW_CMD_UDP_CLR		 17
#define MW_CMD_PING			 18
#define MW_CMD_SNTP_CFG		 19
#define MW_CMD_DATETIME		 20
#define MW_CMD_DT_SET        21
#define MW_CMD_FLASH_WRITE	 22
#define MW_CMD_FLASH_READ	 23
#define MW_CMD_ERROR		255

/** \} */

/** \addtogroup MwMsg MwCmd Command sent to system FSM
 *  \{ */
typedef struct {
	uint16_t cmd;		///< Command code
	uint16_t datalen;	///< Data length
	/// If datalen is nonzero, additional command data goes here until
	/// filling datalen bytes.
	uint8_t data[MW_MAX_CMD_DATALEN];
} MwCmd;
/** \} */

/// Reply to a command
typedef struct {
	uint16_t rep;
	uint16_t datalen;
	/// If datalen is nonzero, additional reply data goes here until
	/// filling datalen bytes.
	uint8_t data[MW_MAX_CMD_DATALEN];
} MwRep;

typedef struct {
	uint8_t data[MW_MSG_MAX_BUFLEN];	///< Buffer data
	uint16_t len;						///< Length of buffer contents
	uint8_t ch;							///< Channel associated with buffer
} MwMsgBuf;

typedef struct {
	char dst_port[6];
	char src_port[6];
	uint8_t channel;
	char data[MW_MSG_MAX_BUFLEN];
} MwMsgInAddr;

#endif //_MW_MSG_H_
