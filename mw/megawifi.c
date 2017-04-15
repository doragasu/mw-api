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

#include "megawifi.h"
#include <string.h>
#include "util.h"

/// Tries sending the specified command. Returns specified code if error.
#define MW_TRY_CMD_SEND(pCmd, errRet) do {if (MwCmdSend((pCmd), \
			MW_DEF_MAX_LOOP_CNT)  != 0) return (errRet);} while (0)

/// Tries receiving a command response. Returns specified code if error.
#define MW_TRY_REP_RECV(pRep, errRet) do { \
	if ((MwCmdReplyGet(pRep, MW_DEF_MAX_LOOP_CNT) < 0) || \
				(MW_CMD_OK != cmd->cmd)) return errRet;} while (0)

static MwCmd *cmd;
static uint16_t maxLen;
static uint16_t recvd;	// Number of bytes received on last frame
static uint8_t fDr;		// Data received flag
static uint8_t mwReady = FALSE;

/************************************************************************//**
 * \brief MwInit Module initialization. Must be called once before using any
 *        other function. It also initializes de UART.
 *
 * \param[in] cmdBuf Pointer to the buffer used to send and receive commands.
 * \param[in] bufLen Length of cmdBuf in bytes. 
 *
 * \return 0 if Initialization successful, lower than 0 otherwise.
 ****************************************************************************/
int MwInit(char *cmdBuf, uint16_t bufLen) {
	// Check input params
	if ((cmdBuf == NULL) || (bufLen < MW_CMD_MIN_BUFLEN)) return MW_ERROR;

	// Set command buffer
	cmd = (MwCmd*)cmdBuf;
	maxLen = bufLen;

	// Initialize LSD
	LsdInit();

	// TODO Set lines to default status (keep WiFi module in reset)
	MwModuleReset();
	// Power down and Program not active (required for the module to boot)
	UartClrBits(MCR, MW__PRG | MW__PD);
	// Try accessing UART scratch pad register to see if it is installed
	
	UART_SPR = 0x55;
	if (UART_SPR != 0x55) return MW_ERROR;
	UART_SPR = 0xAA;
	if (UART_SPR != 0xAA) return MW_ERROR;

	// Clear data received flag
	fDr = FALSE;
	recvd = 0;
	// Enable control channel
	LsdChEnable(MW_CTRL_CH);

	mwReady = TRUE;

	return MW_OK;
}

/************************************************************************//**
 * \brief Send a command to the WiFi module.
 *
 * \param[in] cmd Pointer to the filled MwCmd command structure.
 * \param[in]  maxLoopCnt Maximum number of loops trying to write command.
 *
 * \return 0 if OK. Nonzero if error.
 ****************************************************************************/
int MwCmdSend(MwCmd* cmd, uint32_t maxLoopCnt) {
	if (!mwReady) return MW_ERROR;

	// Send data on control channel (0).
	return LsdSend((uint8_t*)cmd, cmd->datalen + 4, MW_CTRL_CH,
			maxLoopCnt) < 0?-1:0;
}

/************************************************************************//**
 * \brief Try obtaining a reply to a command.
 *
 * \param[out] rep Pointer to MwRep structure, containing the reply to the
 *                 command, if the call completed successfully.
 * \param[in]  maxLoopCnt Maximum number of loops trying to read data.
 *
 * \return The channel on which the data has been received (0 if it was on
 *         the control channel). Lower than 0 if there was a reception
 *         error.
 ****************************************************************************/
int MwCmdReplyGet(MwCmd *rep, uint32_t maxLoopCnt) {
	if (!mwReady) return MW_ERROR;

	uint16_t maxLen = sizeof(MwCmd);

	return LsdRecv((uint8_t*)rep, &maxLen, maxLoopCnt);
}

/************************************************************************//**
 * \brief Obtain module version numbers and string
 *
 * \param[out] verMajor Pointer to Major version number.
 * \param[out] verMinor Pointer to Minor version number.
 * \param[out] variant  String with firmware variant ("std" for standard).
 *
 * \return MW_OK if completed successfully, MW_ERROR otherwise.
 ****************************************************************************/
int MwVersionGet(uint8_t *verMajor, uint8_t *verMinor, char *variant[]) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_VERSION;
	cmd->datalen = 0;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);
	*verMajor = cmd->data[0];
	*verMinor = cmd->data[1];
	// Version string is not NULL terminated, add proper termination
	cmd->data[MIN(maxLen - 1, cmd->datalen)] = '\0';
	*variant = (char*)(cmd->data + 2);

	return MW_OK;
}

/************************************************************************//**
 * \brief Sends an Echo request with the specified data payload, and returns
 * the echo response.
 *
 * \param[in]    data Data buffer to send.
 * \param[inout] len  Pointer to the length of the data buffer to send. On
 *                    successful function exit, holds the length of the
 *                    received echo response.
 *
 * \return Pointer to the received buffer, of NULL if command has failed.
 *
 * \note Returned data pointer is valid until cmdBuf buffer configured
 *       during initialization, is reused.
 ****************************************************************************/
char *MwEcho(char data[], int *len) {
	if (!mwReady) return NULL;

	if (*len + LSD_OVERHEAD + 4 < maxLen) return NULL;

	// Try sending and receiving echo
	cmd->cmd = MW_CMD_ECHO;
	cmd->datalen = *len;
	memcpy((char*)cmd->data, data, *len);
	MW_TRY_CMD_SEND(cmd, NULL);
	MW_TRY_REP_RECV(cmd, NULL);
	// Got response, return
	*len = cmd->datalen;

	return (char*)cmd->data;
}

/************************************************************************//**
 * \brief Set default module configuration.
 *
 * \return MW_OK if configuration successfully reset, MW_ERROR otherwise.
 *
 * \note For this command to take effect, it must be followed by a module
 *       reset.
 ****************************************************************************/
int MwDefaultCfgSet(void) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_DEF_CFG_SET;
	cmd->datalen = 4;
	cmd->dwData[0] = 0xFEAA5501;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Set access point configuration (SSID and password).
 *
 * \param[in] index Index of the configuration to set.
 * \param[in] ssid  String with the AP SSID to set.
 * \param[in] pass  String with the AP SSID to set.
 *
 * \return MW_OK if configuration successfully set, MW_ERROR otherwise.
 *
 * \note Strings must be NULL terminated. Maximum SSID length is 32 bytes,
 *       maximum pass length is 64 bytes.
 ****************************************************************************/
int MwApCfgSet(uint8_t index, const char ssid[], const char pass[]) {
	if (!mwReady) return MW_ERROR;

	if (index >= MW_NUM_AP_CFGS) return MW_ERROR;
	if (strlen(ssid) > MW_SSID_MAXLEN) return MW_ERROR;
	if (strlen(pass) > MW_PASS_MAXLEN) return MW_ERROR;

	cmd->cmd = MW_CMD_AP_CFG;
	cmd->datalen = sizeof(MwMsgApCfg);
	cmd->apCfg.cfgNum = index;
	// Note: *NOT* NULL terminated strings are allowed on cmd.apCfg.ssid and
	// cmd.apCfg.pass
	// No stupid strncpy() supported by SGDK, so workaround it
	memset(&cmd->apCfg, 0, sizeof(MwMsgApCfg));
	memcpy(cmd->apCfg.ssid, ssid, strlen(ssid));
	memcpy(cmd->apCfg.pass, pass, strlen(pass));
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Gets access point configuration (SSID and password).
 *
 * \param[in]  index Index of the configuration to get.
 * \param[out] ssid  String with the AP SSID got.
 * \param[out] pass  String with the AP SSID got.
 *
 * \return MW_OK if configuration successfully got, MW_ERROR otherwise.
 *
 * \warning ssid is zero padded up to 32 bytes, and pass is zero padded up
 *          to 64 bytes. If ssid is 32 bytes, it will NOT be NULL terminated.
 *          Also if pass is 64 bytes, it will NOT be NULL terminated.
 ****************************************************************************/
int MwApCfgGet(uint8_t index, char *ssid[], char *pass[]) {
	if (!mwReady) return MW_ERROR;

	if (index >= MW_NUM_AP_CFGS) return MW_ERROR;

	cmd->cmd = MW_CMD_AP_CFG_GET;
	cmd->datalen = 1;
	cmd->data[0] = index;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);
	*ssid = cmd->apCfg.ssid;
	*pass = cmd->apCfg.pass;

	return index;
}

/************************************************************************//**
 * \brief Set IPv4 configuration.
 *
 * \param[in] index Index of the configuration to set.
 * \param[in] ip    Pointer to the MwIpCfg structure, with IP configuration.
 *
 * \return MW_OK if configuration successfully set, MW_ERROR otherwise.
 ****************************************************************************/
int MwIpCfgSet(uint8_t index, const MwIpCfg *ip) {
	if (!mwReady) return MW_ERROR;

	if (index >= MW_NUM_AP_CFGS) return MW_ERROR;

	cmd->cmd = MW_CMD_IP_CFG;
	cmd->datalen = sizeof(MwMsgIpCfg);
	cmd->ipCfg.cfgNum = index;
	cmd->ipCfg.reserved[0] = 0;
	cmd->ipCfg.reserved[1] = 0;
	cmd->ipCfg.reserved[2] = 0;
	cmd->ipCfg.ip = *ip;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return index;
}

/************************************************************************//**
 * \brief Get IPv4 configuration.
 *
 * \param[in]  index Index of the configuration to get.
 * \param[out] ip    Double pointer to MwIpCfg structure, with IP conf.
 *
 * \return MW_OK if configuration successfully got, MW_ERROR otherwise.
 ****************************************************************************/
int MwIpCfgGet(uint8_t index, MwIpCfg **ip) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_IP_CFG_GET;
	cmd->datalen = 1;
	cmd->data[0] = index;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);
	*ip = &cmd->ipCfg.ip;

	return index;
}

/************************************************************************//**
 * \brief Scan for access points.
 *
 * \param[out] apData Data of the found access points. Each entry has the
 *             format specified on the MwApData structure.
 *
 * \return Length in bytes of the output data if operation completes
 *         successfully, or MW_ERROR if scan fails.
 ****************************************************************************/
int MwApScan(char *apData[]) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_AP_SCAN;
	cmd->datalen = 0;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);
	*apData = (char*)cmd->data;

	return cmd->datalen;
}

/************************************************************************//**
 * \brief Parses received AP data and fills information of the AP at "pos".
 *        Useful to extract AP information from the data obtained by
 *        calling MwApScan() function.
 *
 * \param[in]  apData  Access point data obtained from MwApScan().
 * \param[in]  pos     Position at which to extract data.
 * \param[out] apd     Pointer to the extracted data from an AP.
 * \param[in]  dataLen Lenght of apData.
 *
 * \return Position of the next AP entry in apData, 0 if no more APs
 *         available or MW_ERROR if apData/pos combination is not valid.
 *
 * \note This functions executes locally, does not communicate with the
 *       WiFi module.
 ****************************************************************************/
int MwApFillNext(char apData[], uint16_t pos,
		         MwApData *apd, uint16_t dataLen) {

	if (pos >= dataLen) return 0;	// End reached
	if ((pos + apData[pos + 3] + 4) > dataLen) return MW_ERROR;
	apd->auth = apData[pos++];
	apd->channel = apData[pos++];
	apd->str = apData[pos++];
	apd->ssidLen = apData[pos++];
	apd->ssid = apData + pos;

	// Return updated position
	return pos + apd->ssidLen;
}


/************************************************************************//**
 * \brief Tries joining an AP. If successful, also configures IPv4.
 *
 * \param[in] index Index of the configuration used to join the AP.
 *
 * \return MW_OK if AP joined successfully and ready to send/receive data,
 *         or MW_ERROR if AP join/IP configuration failed.
 ****************************************************************************/
int MwApJoin(uint8_t index) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_AP_JOIN;
	cmd->datalen = 1;
	cmd->data[0] = index;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Leaves a previously joined AP.
 *
 * \return MW_OK if AP successfully left, or MW_ERROR if operation failed.
 ****************************************************************************/
int MwApLeave(void) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_AP_LEAVE;
	cmd->datalen = 0;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Tries establishing a TCP connection with specified server.
 *
 * \param[in] ch Channel used for the connection.
 * \param[in] dstaddr Address (IP or DNS entry) of the server.
 * \param[in] dstport Port in which server is listening.
 * \param[in] srcport Port from which try establishing connection. Set to
 *                    0 or empty string for automatic port allocation.
 *
 * \return MW_OK if connection successfully established, or MW_ERROR if
 *         connection failed.
 ****************************************************************************/
int MwTcpConnect(uint8_t ch, char dstaddr[], char dstport[], char srcport[]) {
	if (!mwReady) return MW_ERROR;

	if (ch > MW_MAX_SOCK) return MW_ERROR;
	// TODO Check at least dstaddr length and port numbers
	// TODO Maybe we should keep track of used channels

	// Configure TCP socket
	cmd->cmd = MW_CMD_TCP_CON;
	// Length is the length of both ports, the channel and the address.
	cmd->datalen = 6 + 6 + 1 + strlen(dstaddr);
	// Zero structure data
	memset(cmd->inAddr.dst_port, 0, cmd->datalen);
	strcpy(cmd->inAddr.dst_port, dstport);
	if (srcport) strcpy(cmd->inAddr.src_port, srcport);
	cmd->inAddr.channel = ch;
	strcpy(cmd->inAddr.dstAddr, dstaddr);
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	// Enable channel
	LsdChEnable(ch);

	return MW_OK;
}

/************************************************************************//**
 * \brief Disconnects a TCP socket from specified channel.
 *
 * \param[in] ch Channel associated to the socket to disconnect.
 *
 * \return MW_OK if socket successfully disconnected, or MW_ERROR if command
 *         failed.
 ****************************************************************************/
int MwTcpDisconnect(uint8_t ch) {
	if (!mwReady) return MW_ERROR;

	// TODO Check if used channel/socket
	cmd->cmd = MW_CMD_TCP_DISC;
	cmd->datalen = 1;
	cmd->data[0] = ch;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	// Disable channel
	LsdChDisable(ch);

	return MW_OK;
}

/************************************************************************//**
 * \brief Binds a socket to a port, and listens to connections on the port.
 *        If a connection request is received, it will be automatically
 *        accepted.
 *
 * \param[in] ch   Channel associated to the socket bound t port.
 * \param[in] port Port number to which the socket will be bound.
 *
 * \return MW_OK if socket successfully bound, or MW_ERROR if command failed.
 ****************************************************************************/
int MwTcpBind(uint8_t ch, uint16_t port) {
	if (!mwReady) return MW_ERROR;

	// TODO Check if used channel/socket
	cmd->cmd = MW_CMD_TCP_BIND;
	cmd->datalen = 7;
	cmd->bind.reserved = 0;
	cmd->bind.port = port;
	cmd->bind.channel = ch;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	// TODO This should maybe be done later.
	LsdChEnable(ch);

	return MW_OK;
}

/************************************************************************//**
 * \brief Waits until data is received or loop timeout. If data is received,
 *        return the channel on which it has been.
 *
 * \param[in] maxLoopCnt Maximum number of loop tries before desisting from
 *            waiting. Set to 0 avoid waiting if no data is available.
 *
 * \return Channel in which data has been received, or MW_ERROR if an error
 *         has occurred.
 *
 * \note If data has been received on control channel, 0 will be returned.
 ****************************************************************************/
int MwDataWait(uint32_t maxLoopCnt) {
	int ch;
	uint16_t bufLen = maxLen;

	if (!mwReady) return MW_ERROR;

	ch = LsdRecv((uint8_t*)cmd, &bufLen, maxLoopCnt);

	// Check if error
	if (ch < 0) return ch;
	if (0 == ch) return 0;
	if (ch > MW_MAX_SOCK) return MW_ERROR;

	// Signal data received and return channel
	fDr = ch;
	recvd = bufLen;

	return ch;
}

/************************************************************************//**
 * \brief Receive data.
 *
 * \param[out] data       Double pointer to received data.
 * \param[out] len        Length of the received data.
 * \param[in]  maxLoopCnt Maximum number of iterations to try before giving
 *                        up. Set to 0 to avoid waiting if no data available.
 *
 * \return On success, channel on which data has been received, or MW_ERROR
 *         if no data was received.
 ****************************************************************************/
int MwRecv(uint8_t **data, uint16_t *len, uint32_t maxLoopCnt) {
	int ch;

	if (!mwReady) return MW_ERROR;

	// If data already received and pending, return immediately
	if (fDr) {
		ch = fDr;
		fDr = FALSE;
		*data = (uint8_t*)cmd;
		*len = recvd;
		recvd = 0;
		return ch;
	}

	*len = maxLen;
	ch = LsdRecv((uint8_t*)cmd, len, maxLoopCnt);

	// Check if data received on control channel
	if (0 == ch) {
		*data = NULL;
		*len = 0;
		return 0;
	}

	// Check if error	
	if (ch > MW_MAX_SOCK) {
		*data = NULL;
		*len = 0;
		return MW_ERROR;
	}

	// Assign received data and return
	*data = (uint8_t*)cmd;

	return ch;
}


/************************************************************************//**
 * \brief Get system status.
 *
 * \return Pointer to system status structure on success, or NULL on error.
 ****************************************************************************/
MwMsgSysStat *MwSysStatGet(void) {
	if (!mwReady) return NULL;

	cmd->cmd = MW_CMD_SYS_STAT;
	cmd->datalen = 0;
	MW_TRY_CMD_SEND(cmd, NULL);
	MW_TRY_REP_RECV(cmd, NULL);

	return &cmd->sysStat;
}

/************************************************************************//**
 * \brief Get socket status.
 *
 * \param[in] ch Channel associated to the socket asked for status.
 *
 * \return Socket status data on success, or MW_ERROR on error.
 ****************************************************************************/
MwSockStat MwSockStatGet(uint8_t ch) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_SOCK_STAT;
	cmd->datalen = 1;
	cmd->data[0] = ch;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return cmd->data[0];
}


/************************************************************************//**
 * \brief Configure SNTP parameters and timezone.
 *
 * \param[in] servers  Array of up to three NTP servers. If less than three
 *                     servers are desired, unused entries must be empty.
 * \param[in] upDelay  Update delay in seconds. Minimum value is 15.
 * \param[in] timezone Time zone information (from -11 to 13).
 * \param[in] dst      Daylight saving. Set to 1 to apply 1 hour offset.
 *
 * \return MW_OK on success, MW_ERROR if command fails.
 ****************************************************************************/
// TODO Check for overflows when copying server data.
int MwSntpCfgSet(char *servers[3], uint8_t upDelay, char timezone, char dst) {
	uint8_t offset;

	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_SNTP_CFG;
	cmd->sntpCfg.upDelay = upDelay;
	cmd->sntpCfg.tz = timezone;
	cmd->sntpCfg.dst = dst;
	strcpy(cmd->sntpCfg.servers, servers[0]);
	// Offset: server length + 1 ('\0')
	offset  = strlen(servers[0]) + 1;
	strcpy(cmd->sntpCfg.servers + offset, servers[1]);
	offset += strlen(servers[1]) + 1;
	strcpy(cmd->sntpCfg.servers + offset, servers[2]);
	offset += strlen(servers[2]) + 1;
	// Mark the end of the list with two adjacent '\0'
	cmd->sntpCfg.servers[offset] = '\0';
	cmd->datalen = offset + 1;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Get date and time.
 *
 * \param[out] dtBin Date and time in seconds since Epoch. If set to NULL,
 *                   this info is not filled (but return value will still
 *                   be properly set).
 *
 * \return A string with the date and time in textual format, e.g.: "Thu Mar
 *         3 12:26:51 2016”.
 ****************************************************************************/
char *MwDatetimeGet(uint32_t dtBin[2]) {
	if (!mwReady) return NULL;

	cmd->cmd = MW_CMD_DATETIME;
	cmd->datalen = 0;
	MW_TRY_CMD_SEND(cmd, NULL);
	MW_TRY_REP_RECV(cmd, NULL);
	if (dtBin) dtBin = cmd->datetime.dtBin;
	// Set NULL termination of the string
	cmd->data[cmd->datalen] = '\0';

	return cmd->datetime.dtStr;
}

int MwFlashIdGet(uint8_t id[3]) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_FLASH_ID;
	cmd->datalen = 0;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);
	id[0] = cmd->data[0];
	id[1] = cmd->data[1];
	id[2] = cmd->data[2];

	return MW_OK;
}


/************************************************************************//**
 * \brief Erase a 4 KiB Flash sector. Every byte of an erased sector can be
 *        read as 0xFF.
 *
 * \param[in] sect Sector number to erase.
 *
 * \return MW_OK if success, MW_ERROR if sector could not be erased.
 ****************************************************************************/
// sect = 0 corresponds to flash sector 0x80
int MwFlashSectorErase(uint16_t sect) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_FLASH_ERASE;
	cmd->datalen = sizeof(uint16_t);
	cmd->flSect = sect;
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Write data to specified flash address.
 *
 * \param[in] addr    Address to which data will be written.
 * \param[in] data    Data to be written to flash chip.
 * \param[in] dataLen Length in bytes of data field.
 *
 * \return MW_OK if success, MW_ERROR if data could not be written.
 ****************************************************************************/
// Address 0 corresponds to flash address 0x80000
int MwFlashWrite(uint32_t addr, uint8_t data[], uint16_t dataLen) {
	if (!mwReady) return MW_ERROR;

	cmd->cmd = MW_CMD_FLASH_WRITE;
	cmd->datalen = dataLen + sizeof(uint32_t);
	cmd->flData.addr = addr;
	memcpy(cmd->flData.data, data, dataLen);
	MW_TRY_CMD_SEND(cmd, MW_ERROR);
	MW_TRY_REP_RECV(cmd, MW_ERROR);

	return MW_OK;
}

/************************************************************************//**
 * \brief Read data from specified flash address.
 *
 * \param[in] addr    Address from which data will be read.
 * \param[in] dataLen Number of bytes to read from addr.
 *
 * \return Pointer to read data on success, or NULL if command failed.
 ****************************************************************************/
// Address 0 corresponds to flash address 0x80000
uint8_t *MwFlashRead(uint32_t addr, uint16_t dataLen) {
	if (!mwReady) return NULL;

	cmd->cmd = MW_CMD_FLASH_READ;
	cmd->flRange.addr = addr;
	cmd->flRange.len = dataLen;
	cmd->datalen = sizeof(MwMsgFlashRange);
	MW_TRY_CMD_SEND(cmd, NULL);
	MW_TRY_REP_RECV(cmd, NULL);

	return cmd->data;
}

uint8_t *MwHrngGet(uint16_t rndLen) {
	if (!mwReady) return NULL;

	cmd->cmd = MW_CMD_HRNG_GET;
	cmd->datalen = sizeof(uint16_t);
	cmd->rndLen = rndLen;
	MW_TRY_CMD_SEND(cmd, NULL);
	MW_TRY_REP_RECV(cmd, NULL);

	return cmd->data;
}

