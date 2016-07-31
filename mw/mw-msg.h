#ifndef _MW_MSG_H_
#define _MW_MSG_H_

#include <stdint.h>

/// Maximum buffer length (bytes)
#define MW_MSG_MAX_BUFLEN	512

#define MW_CMD_MAX_BUFLEN	(MW_MSG_MAX_BUFLEN - 4)

/// Maximum SSID length (including '\0').
#define MW_SSID_MAXLEN		32
/// Maximum password length (including '\0').
#define MW_PASS_MAXLEN		64

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

/// TCP/UDP address message
typedef struct {
	char dst_port[6];
	char src_port[6];
	uint8_t channel;
	char data[MW_CMD_MAX_BUFLEN - 6 - 6 - 1];
} MwMsgInAddr;

/// AP configuration message
typedef struct {
	uint8_t cfgNum;
	char ssid[MW_SSID_MAXLEN];
	char pass[MW_PASS_MAXLEN];
} MwMsgApCfg;

/// IP configuration message
typedef struct {
	uint8_t cfgNum;
	uint8_t reserved[3];
	uint8_t ip_addr[4];
	uint8_t mask[4];
	uint8_t gateway[4];
	uint8_t dns1[4];
	uint8_t dns2[4];
} MwMsgIpCfg;

/// Date and time message
typedef struct {
	uint32_t dtBin[2];
	char dtStr[MW_CMD_MAX_BUFLEN - sizeof(uint64_t)];
} MwMsgDateTime;

/** \addtogroup MwApi MwCmd Command sent to system FSM
 *  \{ */
typedef struct {
	uint16_t cmd;		///< Command code
	uint16_t datalen;	///< Data length
	// If datalen is nonzero, additional command data goes here until
	// filling datalen bytes.
	union {
		uint8_t ch;		// Channel number for channel related requests
		uint8_t data[MW_CMD_MAX_BUFLEN];// Might need adjusting data length!
		MwMsgInAddr inAddr;
		MwMsgApCfg apCfg;
		MwMsgIpCfg ipCfg;
		MwMsgDateTime datetime;
	};
} MwCmd;
/** \} */

typedef struct {
	uint8_t data[MW_MSG_MAX_BUFLEN];	///< Buffer data
	uint16_t len;						///< Length of buffer contents
	uint8_t ch;							///< Channel associated with buffer
} MwMsgBuf;

#endif //_MW_MSG_H_
