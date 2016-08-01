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

/** \addtogroup MwApi Cmds Supported commands.
 *  \{ */
#define MW_CMD_OK			  0		///< OK command reply
#define MW_CMD_VERSION        1		///< Get firmware version
#define MW_CMD_ECHO			  2		///< Echo data
#define MW_CMD_AP_SCAN		  3		///< Scan for access points
#define MW_CMD_AP_CFG		  4		///< Configure access point
#define MW_CMD_AP_CFG_GET     5		///< Get access point configuration
#define MW_CMD_IP_CFG		  6		///< Configure IPv4
#define MW_CMD_IP_CFG_GET	  7		///< Get IPv4 configuration
#define MW_CMD_AP_JOIN		  8		///< Join access point
#define MW_CMD_AP_LEAVE		  9		///< Leave previously joined access point
#define MW_CMD_TCP_CON		 10		///< Connect TCP socket
#define MW_CMD_TCP_BIND		 11		///< Bind TCP socket to port
#define MW_CMD_TCP_ACCEPT	 12		///< Accept incomint TCP connection
#define MW_CMD_TCP_STAT		 13		///< Get TCP status
#define MW_CMD_TCP_DISC		 14		///< Disconnect and free TCP socket
#define MW_CMD_UDP_SET		 15		///< Configure UDP socket
#define MW_CMD_UDP_STAT		 16		///< Get UDP status
#define MW_CMD_UDP_CLR		 17		///< Clear and free UDP socket
#define MW_CMD_PING			 18		///< Ping host
#define MW_CMD_SNTP_CFG		 19		///< Configure SNTP service
#define MW_CMD_DATETIME		 20		///< Get date and time
#define MW_CMD_DT_SET        21		///< Set date and time
#define MW_CMD_FLASH_WRITE	 22		///< Write to WiFi module flash
#define MW_CMD_FLASH_READ	 23		///< Read from WiFi module flash
#define MW_CMD_FLASH_ERASE	 24		///< Erase sector from WiFi flash
#define MW_CMD_FLASH_ID 	 25		///< Get WiFi flash chip identifiers
#define MW_CMD_ERROR		255		///< Error command reply
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

typedef struct {
	uint32_t addr;
	uint8_t data[MW_CMD_MAX_BUFLEN - sizeof(uint32_t)];
} MwMsgFlashData;

typedef struct {
	uint32_t addr;
	uint16_t len;
} MwMsgFlashRange;

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
		MwMsgFlashData flData;
		MwMsgFlashRange flRange;
		uint16_t flSect;	// Flash sector
		uint32_t flId;		// Flash IDs
	};
} MwCmd;
/** \} */

typedef struct {
	uint8_t data[MW_MSG_MAX_BUFLEN];	///< Buffer data
	uint16_t len;						///< Length of buffer contents
	uint8_t ch;							///< Channel associated with buffer
} MwMsgBuf;

#endif //_MW_MSG_H_
