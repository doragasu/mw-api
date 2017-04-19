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
 *
 * \defgroup MegaWiFi megawifi
 * \{
 ****************************************************************************/

#ifndef _MEGAWIFI_H_
#define _MEGAWIFI_H_

#include <stdint.h>
#include "genesis.h"
#include "16c550.h"
#include "mw-msg.h"
#include "lsd.h"

/** \addtogroup MwCtrlPins MwCtrlPins
 *  \brief Pins used to control WiFi module.
 *  \{ */
#define MW__RESET	UART_MCR__OUT1	///< Reset out.
#define MW__PRG		UART_MCR__OUT2	///< Program out.
#define MW__PD		UART_MCR__DTR	///< Power Down out.
#define MW__DAT		UART_MSR__DSR	///< Data request in.
/** \} */

/** \addtogroup MwRetVals MwRetVals
 *  \brief Function return values.
 *  \{ */
/// The function completed successfully
#define MW_OK		 0
/// The function completed with error
#define MW_ERROR	-1
/** \} */

/// Maximum SSID length (including '\0').
#define MW_SSID_MAXLEN		32
/// Maximum password length (including '\0').
#define MW_PASS_MAXLEN		64
/// Maximum length of an NTP pool URI (including '\0').
#define MW_NTP_POOL_MAXLEN	80
/// Number of AP configurations stored to nvflash.
#define MW_NUM_AP_CFGS		3
/// Number of DSN servers supported per AP configuration.
#define MW_NUM_DNS_SERVERS	2
/// Length of the FSM queue
#define MW_FSM_QUEUE_LEN	8
/// Maximum number of simultaneous TCP connections
#define MW_MAX_SOCK			3
/// Control channel used for LSD protocol
#define MW_CTRL_CH			0
/// Default value of maximum times to try completing a command before
/// desisting
#define MW_DEF_MAX_LOOP_CNT	UINT32_MAX

/// Minimum command buffer length to be able to send all available commands
/// with minimum data payload. This length might not guarantee that commands
/// like MwSntpCfgSet() can be sent if payload length is big enough).
#define MW_CMD_MIN_BUFLEN	104

/// Access Point data.
typedef struct {
	char auth;		///< Authentication type.
	char channel;	///< WiFi channel.
	char str;		///< Signal strength.
	char ssidLen;	///< Length of ssid field.
	char *ssid;		///< SSID string (not NULL terminated).
} MwApData;

/************************************************************************//**
 * \brief MwInit Module initialization. Must be called once before using any
 *        other function. It also initializes de UART.
 *
 * \param[in] cmdBuf Pointer to the buffer used to send and receive commands.
 * \param[in] bufLen Length of cmdBuf in bytes. 
 *
 * \return 0 if Initialization successful, lower than 0 otherwise.
 ****************************************************************************/
int MwInit(char *cmdBuf, uint16_t bufLen);

/************************************************************************//**
 * \brief Obtain module version numbers and string
 *
 * \param[out] verMajor Pointer to Major version number.
 * \param[out] verMinor Pointer to Minor version number.
 * \param[out] variant  String with firmware variant ("std" for standard).
 *
 * \return MW_OK if completed successfully, MW_ERROR otherwise.
 ****************************************************************************/
int MwVersionGet(uint8_t *verMajor, uint8_t *verMinor, char *variant[]);

/************************************************************************//**
 * \brief Set default module configuration.
 *
 * \return MW_OK if configuration successfully reset, MW_ERROR otherwise.
 *
 * \note For this command to take effect, it must be followed by a module
 *       reset.
 ****************************************************************************/
int MwDefaultCfgSet(void);

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
int MwApCfgSet(uint8_t index, const char ssid[], const char pass[]);

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
int MwApCfgGet(uint8_t index, char *ssid[], char *pass[]);

/************************************************************************//**
 * \brief Set IPv4 configuration.
 *
 * \param[in] index Index of the configuration to set.
 * \param[in] ip    Pointer to the MwIpCfg structure, with IP configuration.
 *
 * \return MW_OK if configuration successfully set, MW_ERROR otherwise.
 *
 * \note Strings must be NULL terminated. Maximum SSID length is 32 bytes,
 *       maximum pass length is 64 bytes.
 ****************************************************************************/
int MwIpCfgSet(uint8_t index, const MwIpCfg *ip);

/************************************************************************//**
 * \brief Get IPv4 configuration.
 *
 * \param[in]  index Index of the configuration to get.
 * \param[out] ip    Double pointer to MwIpCfg structure, with IP conf.
 *
 * \return MW_OK if configuration successfully got, MW_ERROR otherwise.
 ****************************************************************************/
int MwIpCfgGet(uint8_t index, MwIpCfg **ip);

/************************************************************************//**
 * \brief Scan for access points.
 *
 * \param[out] apData Data of the found access points. Each entry has the
 *             format specified on the MwApData structure.
 *
 * \return Length in bytes of the output data if operation completes
 *         successfully, or MW_ERROR if scan fails.
 ****************************************************************************/
int MwApScan(char *apData[]);

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
		         MwApData *apd, uint16_t dataLen);

/************************************************************************//**
 * \brief Tries joining an AP. If successful, also configures IPv4.
 *
 * \param[in] index Index of the configuration used to join the AP.
 *
 * \return MW_OK if AP joined successfully and ready to send/receive data,
 *         or MW_ERROR if AP join/IP configuration failed.
 ****************************************************************************/
int MwApJoin(uint8_t index);

/************************************************************************//**
 * \brief Leaves a previously joined AP.
 *
 * \return MW_OK if AP successfully left, or MW_ERROR if operation failed.
 ****************************************************************************/
int MwApLeave(void);

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
int MwTcpConnect(uint8_t ch, char dstaddr[], char dstport[], char srcport[]);

/************************************************************************//**
 * \brief Disconnects a TCP socket from specified channel.
 *
 * \param[in] ch Channel associated to the socket to disconnect.
 *
 * \return MW_OK if socket successfully disconnected, or MW_ERROR if command
 *         failed.
 ****************************************************************************/
int MwTcpDisconnect(uint8_t ch);

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
int MwTcpBind(uint8_t ch, uint16_t port);

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
int MwDataWait(uint32_t maxLoopCnt);

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
int MwRecv(uint8_t **data, uint16_t *len, uint32_t maxLoopCnt);

/************************************************************************//**
 * \brief Sends data through a socket, using a previously allocated channel.
 *
 * \param[in] ch     Channel used to send the data.
 * \param[in] data   Data to send through channel.
 * \param[in] length Length in bytes of the data field.
 ****************************************************************************/
#define MwSend(ch, data, length) LsdSend(data, length, ch, \
		                                     MW_DEF_MAX_LOOP_CNT)

/************************************************************************//**
 * \brief Get system status.
 *
 * \return Pointer to system status structure on success, or NULL on error.
 ****************************************************************************/
MwMsgSysStat *MwSysStatGet(void);

/************************************************************************//**
 * \brief Get socket status.
 *
 * \param[in] ch Channel associated to the socket asked for status.
 *
 * \return Socket status data on success, or MW_ERROR on error.
 ****************************************************************************/
MwSockStat MwSockStatGet(uint8_t ch);

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
int MwSntpCfgSet(char *servers[3], uint8_t upDelay, char timezone, char dst);

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
char *MwDatetimeGet(uint32_t dtBin[2]);

/************************************************************************//**
 * \brief Erase a 4 KiB Flash sector. Every byte of an erased sector can be
 *        read as 0xFF.
 *
 * \param[in] sect Sector number to erase.
 *
 * \return MW_OK if success, MW_ERROR if sector could not be erased.
 ****************************************************************************/
int MwFlashSectorErase(uint16_t sect);

/************************************************************************//**
 * \brief Write data to specified flash address.
 *
 * \param[in] addr    Address to which data will be written.
 * \param[in] data    Data to be written to flash chip.
 * \param[in] dataLen Length in bytes of data field.
 *
 * \return MW_OK if success, MW_ERROR if data could not be written.
 ****************************************************************************/
int MwFlashWrite(uint32_t addr, uint8_t data[], uint16_t dataLen);

/************************************************************************//**
 * \brief Read data from specified flash address.
 *
 * \param[in] addr    Address from which data will be read.
 * \param[in] dataLen Number of bytes to read from addr.
 *
 * \return Pointer to read data on success, or NULL if command failed.
 ****************************************************************************/
uint8_t *MwFlashRead(uint32_t addr, uint16_t dataLen);

/************************************************************************//**
 * \brief Puts the WiFi module in reset state.
 ****************************************************************************/
#define MwModuleReset()		do{UartSetBits(MCR, MW__RESET);}while(0)

/************************************************************************//**
 * \brief Releases the module from reset state.
 ****************************************************************************/
#define MwModuleStart()		do{UartClrBits(MCR, MW__RESET);}while(0)

/****** THE FOLLOWING COMMANDS ARE LOWER LEVEL AND USUALLY NOT NEEDED ******/

/************************************************************************//**
 * \brief Send a command to the WiFi module.
 *
 * \param[in] cmd Pointer to the filled MwCmd command structure.
 * \param[in]  maxLoopCnt Maximum number of loops trying to write command.
 *
 * \return 0 if OK. Nonzero if error.
 ****************************************************************************/
int MwCmdSend(MwCmd* cmd, uint32_t maxLoopCnt);

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
int MwCmdReplyGet(MwCmd *rep, uint32_t maxLoopCnt);

#endif /*_MEGAWIFI_H_*/

/** \} */

