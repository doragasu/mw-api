/************************************************************************//**
 * \file
 *
 * \brief MegaWiFi API implementation.
 *
 * \defgroup megawifi megawifi
 * \{
 *
 * \brief MegaWiFi API implementation.
 *
 * API to communicate with the wifi module and the Internet. API calls are
 * documented and most of them are self explanatory. Mostly the only weird
 * thing about the API is UDP reuse mode. If you enable reuse mode (setting
 * the dst_addr and/or dst_port to NULL in the mw_udp_set() call), received
 * data will prepend the IP and port of the peer (using mw_reuse_payload data
 * structure), and data to be sent also requires the IP and port to be
 * prepended to the payload.
 *
 * \author Jesus Alonso (doragasu)
 * \date 2015
 *
 * \note This module uses a loop_timer from the loop module.
 *
 * \todo Missing a lot of integrity checks, also module should track used
 *       channels, and is not currently doing it
 ****************************************************************************/

#ifndef _MEGAWIFI_H_
#define _MEGAWIFI_H_

#include "16c550.h"
#include "mw-msg.h"
#include "lsd.h"

/// Timeout for standard commands in milliseconds
#define MW_COMMAND_TOUT_MS	1000
/// Timeout for the AP scan command in milliseconds
#define MW_SCAN_TOUT_MS		10000
/// Timeout for the AP associate command in milliseconds
#define MW_ASSOC_TOUT_MS	20000
/// Milliseconds between status polls while in wm_ap_assoc_wait()
#define MW_STAT_POLL_MS		250

/// Error codes for MegaWiFi API functions
enum mw_err {
	MW_ERR_NONE = 0,		///< No error (success)
	MW_ERR,				///< General error
	MW_ERR_NOT_READY,		///< Not ready to run command
	MW_ERR_BUFFER_TOO_SHORT,	///< Command buffer is too small
	MW_ERR_PARAM,			///< Input parameter out of range
	MW_ERR_SEND,			///< Error sending data
	MW_ERR_RECV			///< Error receiving data
};

/** \addtogroup mw_ctrl_pins mw_ctrl_pins
 *  \brief Pins used to control WiFi module.
 *  \{ */
#define MW__RESET	UART_MCR__OUT1	///< Reset out.
#define MW__PRG		UART_MCR__OUT2	///< Program out.
#define MW__PD		UART_MCR__DTR	///< Power Down out.
#define MW__DAT		UART_MSR__DSR	///< Data request in.
/** \} */

/// Maximum SSID length (including '\0').
#define MW_SSID_MAXLEN		32
/// Maximum password length (including '\0').
#define MW_PASS_MAXLEN		64
/// Maximum length of an NTP pool URI (including '\0').
#define MW_NTP_POOL_MAXLEN	80
/// Number of AP configurations stored to nvflash.
#define MW_NUM_CFG_SLOTS	3
/// Number of DSN servers supported per AP configuration.
#define MW_NUM_DNS_SERVERS	2
/// Length of the FSM queue
#define MW_FSM_QUEUE_LEN	8
/// Maximum number of simultaneous TCP connections
#define MW_MAX_SOCK			3
/// Control channel used for LSD protocol
#define MW_CTRL_CH			0

/// Minimum command buffer length to be able to send all available commands
/// with minimum data payload. This length might not guarantee that commands
/// like mw_sntp_cfg_set() can be sent if payload length is big enough).
#define MW_CMD_MIN_BUFLEN	104

/// Access Point data.
struct mw_ap_data {
	enum mw_security auth;	///< Security type
	uint8_t channel;	///< WiFi channel.
	int8_t rssi;		///< Signal strength.
	uint8_t ssid_len;	///< Length of ssid field.
	char *ssid;		///< SSID string (not NULL terminated).
};

/// Interface type for the mw_bssid_get() function.
enum mw_if_type {
	MW_IF_STATION = 0,	///< Station interface
	MW_IF_SOFTAP,		///< Access Point interface
	MW_IF_MAX		///< Number of supported interface types
};

/************************************************************************//**
 * \brief Module initialization. Must be called once before using any
 *        other function. It also initializes de UART.
 *
 * \param[in] cmd_buf Pointer to the buffer used to send and receive commands.
 * \param[in] buf_len Length of cmdBuf in bytes. 
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
int mw_init(char *cmd_buf, uint16_t buf_len);

/************************************************************************//**
 * \brief Processes sends/receives pending data.
 *
 * Call this function as much as possible to process incoming/outgoing data.
 *
 * \warning No data will be sent/received if this function is not frequently
 * invoked.
 ****************************************************************************/
static inline void mw_process(void)	{lsd_process();}

/************************************************************************//**
 * \brief Sets the callback function to be run when network data is received
 * while waiting for a command reply.
 *
 * \param[in] cmd_recv_cb Callback to be run when data is received while
 *                        waiting for a command reply.
 *
 * \warning If this callback is not set, data received while waiting for a
 * command reply will be silently discarded.
 ****************************************************************************/
void mw_cmd_data_cb_set(lsd_recv_cb cmd_recv_cb);

/************************************************************************//**
 * \brief Performs the startup sequence for the WiFi module, and tries
 * detecting it by requesting the version data.
 *
 * \param[out] major   Pointer to Major version number.
 * \param[out] minor   Pointer to Minor version number.
 * \param[out] variant String with firmware variant ("std" for standard).
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_detect(uint8_t *major, uint8_t *minor, char **variant);

/************************************************************************//**
 * \brief Obtain module version numbers and string
 *
 * \param[out] major   Pointer to Major version number.
 * \param[out] minor   Pointer to Minor version number.
 * \param[out] variant String with firmware variant ("std" for standard).
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_version_get(uint8_t *major, uint8_t *minor, char **variant);

/************************************************************************//**
 * \brief Gets the module BSSID (the MAC address) for the specified interface.
 *
 * \param[in] interface_type Type of the interface to obtain BSSID from.
 *
 * \return The requested BSSID (6 byte binary data), or NULL on error.
 ****************************************************************************/
uint8_t *mw_bssid_get(enum mw_if_type interface_type);

/************************************************************************//**
 * \brief Set default module configuration.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 *
 * \note For this command to take effect, it must be followed by a module
 *       reset.
 ****************************************************************************/
enum mw_err mw_default_cfg_set(void);

/************************************************************************//**
 * \brief Set access point configuration (SSID and password).
 *
 * \param[in] slot Configuration slot to use.
 * \param[in] ssid String with the AP SSID to set.
 * \param[in] pass String with the AP SSID to set.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 *
 * \note Strings must be NULL terminated. Maximum SSID length is 32 bytes,
 *       maximum pass length is 64 bytes.
 ****************************************************************************/
enum mw_err mw_ap_cfg_set(uint8_t slot, const char *ssid, const char *pass);

/************************************************************************//**
 * \brief Gets access point configuration (SSID and password).
 *
 * \param[in]  slot Configuration slot to use.
 * \param[out] ssid String with the AP SSID got.
 * \param[out] pass String with the AP SSID got.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 *
 * \warning ssid is zero padded up to 32 bytes, and pass is zero padded up
 *          to 64 bytes. If ssid is 32 bytes, it will NOT be NULL terminated.
 *          Also if pass is 64 bytes, it will NOT be NULL terminated.
 ****************************************************************************/
enum mw_err mw_ap_cfg_get(uint8_t slot, char **ssid, char **pass);

/************************************************************************//**
 * \brief Set IPv4 configuration.
 *
 * \param[in] slot Configuration slot to use.
 * \param[in] ip   Pointer to the mw_ip_cfg structure, with IP configuration.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 *
 * \note Strings must be NULL terminated. Maximum SSID length is 32 bytes,
 *       maximum pass length is 64 bytes.
 ****************************************************************************/
enum mw_err mw_ip_cfg_set(uint8_t slot, const struct mw_ip_cfg *ip);

/************************************************************************//**
 * \brief Get IPv4 configuration.
 *
 * \param[in]  slot Configuration slot to use.
 * \param[out] ip   Double pointer to mw_ip_cfg structure, with IP conf.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_ip_cfg_get(uint8_t slot, struct mw_ip_cfg **ip);

/************************************************************************//**
 * \brief Get current IP configuration, of the joined AP.
 *
 * \param[out] ip Double pointer to mw_ip_cfg structure, with IP conf.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_ip_current(struct mw_ip_cfg **ip);

/************************************************************************//**
 * \brief Scan for access points.
 *
 * \param[out] ap_data Data of the found access points. Each entry has the
 *             format specified on the mw_ap_data structure.
 * \param[out] aps     Number of found access points.
 *
 * \return Length in bytes of the output data if operation completes
 *         successfully, or -1 if scan fails.
 ****************************************************************************/
int mw_ap_scan(char **ap_data, uint8_t *aps);

/************************************************************************//**
 * \brief Parses received AP data and fills information of the AP at "pos".
 *        Useful to extract AP information from the data obtained by
 *        calling mw_ap_scan() function.
 *
 * \param[in]  ap_data  Access point data obtained from mw_ap_scan().
 * \param[in]  pos      Position at which to extract data.
 * \param[out] apd      Pointer to the extracted data from an AP.
 * \param[in]  data_len Lenght of apData.
 *
 * \return Position of the next AP entry in apData, 0 if no more APs
 *         available or MW_ERROR if ap data/pos combination is not valid.
 *
 * \note This functions executes locally, does not communicate with the
 *       WiFi module.
 ****************************************************************************/
int mw_ap_fill_next(const char *ap_data, uint16_t pos,
		struct mw_ap_data *apd, uint16_t data_len);

/************************************************************************//**
 * \brief Tries associating to an AP. If successful, also configures IPv4.
 *
 * \param[in] slot Configuration slot to use.
 *
 * \return MW_ERR_NONE if AP join operation has been successfully started,
 ****************************************************************************/
enum mw_err mw_ap_assoc(uint8_t slot);

/************************************************************************//**
 * \brief Polls the module status until it reports device is associated to
 * AP or timeout occurs.
 *
 * \param[in] tout_frames Maximun number of frames to wait for association.
 *            Set to 0 for an infinite wait.
 *
 * \return MW_ERR_NONE if device is associated to AP. MW_ERR_NOT_READY if
 * the timeout has expired.
 ****************************************************************************/
enum mw_err mw_ap_assoc_wait(int tout_frames);

/************************************************************************//**
 * Sets default AP/IP configuration.
 *
 * \param[in] slot Configuration slot to use.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_def_ap_cfg(uint8_t slot);

/************************************************************************//**
 * \brief Dissasociates from a previously associated AP.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_ap_disassoc(void);

/************************************************************************//**
 * Gets default AP/IP configuration slot.
 *
 * \return The default configuration slot, of -1 on error.
 ****************************************************************************/
int mw_def_ap_cfg_get(void);

/************************************************************************//**
 * \brief Tries establishing a TCP connection with specified server.
 *
 * \param[in] ch       Channel used for the connection.
 * \param[in] dst_addr Address (IP or DNS entry) of the server.
 * \param[in] dst_port Port in which server is listening.
 * \param[in] src_port Port from which try establishing connection. Set to
 *                     0 or empty string for automatic port allocation.
 *
 * \return MW_ERR_NONE on success, other code if connection failed.
 ****************************************************************************/
enum mw_err mw_tcp_connect(uint8_t ch, const char *dst_addr,
		const char *dst_port, const char *src_port);

/************************************************************************//**
 * \brief Closes and disconnects a socket from specified channel.
 *
 * This function can be used to free the channel associated to both TCP and
 * UDP sockets.
 *
 * \param[in] ch Channel associated to the socket to disconnect.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_close(uint8_t ch);

/// Closes a TCP socket. This is an alias of mw_close().
#define mw_tcp_disconnect(ch)	mw_close(ch)

/************************************************************************//**
 * \brief Configures a UDP socket to send/receive data.
 *
 * \param[in] ch       Channel used for the connection.
 * \param[in] dst_addr Address (IP or DNS entry) to send data to.
 * \param[in] dst_port Port to send data to.
 * \param[in] src_port Local port to listen message on.
 *
 * \return MW_ERR_NONE on success, other code if connection failed.
 *
 * \note Setting to NULL dst_addr and/or dst_port, enables reuse mode.
 ****************************************************************************/
enum mw_err mw_udp_set(uint8_t ch, const char *dst_addr, const char *dst_port,
		const char *src_port);

/// Frees a UDP socket. This is an alias of mw_close().
#define mw_udp_unset(ch)	mw_close(ch)

/************************************************************************//**
 * \brief Binds a socket to a port, and listens to connections on the port.
 *        If a connection request is received, it will be automatically
 *        accepted.
 *
 * \param[in] ch   Channel associated to the socket bound t port.
 * \param[in] port Port number to which the socket will be bound.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_tcp_bind(uint8_t ch, uint16_t port);

/************************************************************************//**
 * \brief Polls a socket until it is ready to transfer data. Typical use of
 * this function is after a successful mw_tcp_bind().
 *
 * \param[in] ch          Channel associated to the socket to monitor.
 * \param[in] tout_frames Maximum number of frames to wait for connection.
 *            Set to 0 for an infinite wait.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_sock_conn_wait(uint8_t ch, int tout_frames);

/************************************************************************//**
 * \brief Receive data, asyncrhonous interface.
 *
 * \param[in] buf     Reception buffer.
 * \param[in] len     Length of the receive buffer.
 * \param[in] ctx     Context pointer to pass to the reception callbak.
 * \param[in] recv_cb Callback to run when reception is complete or errors.
 *
 * \return Status of the receive procedure.
 ****************************************************************************/
static inline enum lsd_status mw_recv(char *buf, int16_t len, void *ctx,
		lsd_recv_cb recv_cb)
{
	return lsd_recv(buf, len, ctx, recv_cb);
}

/************************************************************************//**
 * \brief Receive data using an UDP socket in reuse mode.
 *
 * \param[in] data    Receive buffer including the remote address and the
 *                    data payload.
 * \param[in] len     Length of the receive buffer.
 * \param[in] ctx     Context pointer to pass to the reception callbak.
 * \param[in] recv_cb Callback to run when reception is complete or errors.
 *
 * \return Status of the receive procedure.
 ****************************************************************************/
static inline enum lsd_status mw_udp_reuse_recv(struct mw_reuse_payload *data,
		int16_t len, void *ctx, lsd_recv_cb recv_cb)
{
	return lsd_recv((char*)data, len, ctx, recv_cb);
}

/************************************************************************//**
 * \brief Send data using a UDP socket in reuse mode.
 *
 * \param[in] data    Send buffer including the remote address and the
 *                    data payload.
 * \param[in] len     Length of the receive buffer.
 * \param[in] ctx     Context pointer to pass to the reception callbak.
 * \param[in] recv_cb Callback to run when reception is complete or errors.
 *
 * \return Status of the receive procedure.
 ****************************************************************************/
static inline enum lsd_status mw_udp_reuse_send(uint8_t ch,
		const struct mw_reuse_payload *data, int16_t len, void *ctx,
		lsd_send_cb send_cb)
{
	return lsd_send(ch, (const char*)data, len, ctx, send_cb);
}

/************************************************************************//**
 * \brief Sends data through a socket, using a previously allocated channel.
 * Asynchronous interface.
 *
 * \param[in] ch      Channel used to send the data.
 * \param[in] data    Buffer to send.
 * \param[in] len     Length of the data to send.
 * \param[in] ctx     Context for the send callback function.
 * \param[in] send_cb Callback to run when send completes or errors.
 *
 * \return Status of the send procedure. Usually LSD_STAT_BUSY is returned,
 * and the send procedure is then performed in background.
 * \note Calling this function while there is a send procedure in progress,
 * will cause the function call to fail with LSD_STAT_SEND_ERR_IN_PROGRESS.
 * \warning For very short data frames, it is possible that the send callback
 * is run before this function returns. In this case, the function returns
 * LSD_STAT_COMPLETE.
 ****************************************************************************/
static inline enum lsd_status mw_send(uint8_t ch, const char *data, int16_t len,
		void *ctx, lsd_send_cb send_cb)
{
	return lsd_send(ch, data, len, ctx, send_cb);
}

/************************************************************************//**
 * \brief Receive data, syncrhonous interface.
 *
 * \param[out] ch         Channel on which data was received.
 * \param[out] buf        Reception buffer.
 * \param[inout] buf_len  On input, length of the buffer.
 *                        On output, received data length in bytes.
 * \param[in] tout_frames Reception timeout in frames. Set to 0 for infinite
 *                        wait (dangerous!).
 *
 * \return Status of the receive procedure.
 * \warning Do not use more than one syncrhonous call at once. You must wait
 * until a syncrhonous call ends to issue another one.
 ****************************************************************************/
enum mw_err mw_recv_sync(uint8_t *ch, char *buf, int16_t *buf_len,
		uint16_t tout_frames);

/************************************************************************//**
 * \brief Sends data through a socket, using a previously allocated channel.
 * Synchronous interface.
 *
 * \param[in] ch          Channel used to send the data.
 * \param[in] data        Buffer to send.
 * \param[in] len         Length of the data to send.
 * \param[in] tout_frames Timeout for send operation in frames. Set to 0 for
 *                        infinite wait (dangerous!).
 *
 * \return Status of the send procedure. Usually LSD_STAT_BUSY is returned,
 * and the send procedure is then performed in background.
 * \warning Do not use more than one syncrhonous call at once. You must wait
 * until a syncrhonous call ends to issue another one.
 ****************************************************************************/
enum mw_err mw_send_sync(uint8_t ch, const char *data, int16_t len,
		uint16_t tout_frames);

/************************************************************************//**
 * \brief Get system status.
 *
 * \return Pointer to system status structure on success, or NULL on error.
 ****************************************************************************/
union mw_msg_sys_stat *mw_sys_stat_get(void);

/************************************************************************//**
 * \brief Get socket status.
 *
 * \param[in] ch Channel associated to the socket asked for status.
 *
 * \return Socket status data on success, or -1 on error.
 ****************************************************************************/
enum mw_sock_stat mw_sock_stat_get(uint8_t ch);

/************************************************************************//**
 * \brief Configure SNTP parameters and timezone.
 *
 * \param[in] server   Array of up to three NTP servers. If less than three
 *                     servers are desired, unused entries must be empty.
 * \param[in] up_delay Update delay in seconds. Minimum value is 15.
 * \param[in] timezone Time zone information (from -11 to 13).
 * \param[in] dst      Daylight saving. Set to 1 to apply 1 hour offset.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_sntp_cfg_set(const char *server[3], uint16_t up_delay,
		int8_t timezone, int8_t dst);

/************************************************************************//**
 * \brief Get SNTP parameters and timezone configuration.
 *
 * \param[out] server   Array of three NTP server pointers. If less than 3
 *                      servers are configured, unused ones will be NULL.
 * \param[out] up_delay Update delay in seconds.
 * \param[out] timezone Time zone information (from -11 to 13).
 * \param[out] dst      Daylight saving. When 1, 1 hour offset is applied.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_sntp_cfg_get(char *server[3], uint16_t *up_delay,
		int8_t *timezone, int8_t *dst);

/************************************************************************//**
 * \brief Get date and time.
 *
 * \param[out] dt_bin Date and time in seconds since Epoch. If set to NULL,
 *                    this info is not filled (but return value will still
 *                    be properly set).
 *
 * \return A string with the date and time in textual format, e.g.: "Thu Mar
 *         3 12:26:51 2016”, or NULL if error.
 ****************************************************************************/
char *mw_date_time_get(uint32_t dt_bin[2]);

/************************************************************************//**
 * \brief Get date and time.
 *
 * \param[out] id Identifiers of the flash chip installed in the WiFi module.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_flash_id_get(uint8_t id[3]);

/************************************************************************//**
 * \brief Erase a 4 KiB Flash sector. Every byte of an erased sector will be
 *        read as 0xFF.
 *
 * \param[in] sect Sector number to erase.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_flash_sector_erase(uint16_t sect);

/************************************************************************//**
 * \brief Write data to specified flash address.
 *
 * \param[in] addr     Address to which data will be written.
 * \param[in] data     Data to be written to flash chip.
 * \param[in] data_len Length in bytes of data field.
 *
 * \return MW_ERR_NONE on success, other code on failure.
 ****************************************************************************/
enum mw_err mw_flash_write(uint32_t addr, uint8_t *data, uint16_t data_len);

/************************************************************************//**
 * \brief Read data from specified flash address.
 *
 * \param[in] addr     Address from which data will be read.
 * \param[in] data_len Number of bytes to read from addr.
 *
 * \return Pointer to read data on success, or NULL if command failed.
 ****************************************************************************/
uint8_t *mw_flash_read(uint32_t addr, uint16_t data_len);

/************************************************************************//**
 * \brief Puts the WiFi module in reset state.
 ****************************************************************************/
#define mw_module_reset()	do{uart_set_bits(MCR, MW__RESET);}while(0)

/************************************************************************//**
 * \brief Releases the module from reset state.
 ****************************************************************************/
#define mw_module_start()	do{uart_clr_bits(MCR, MW__RESET);}while(0)

/************************************************************************//**
 * \brief Sleep the specified amount of frames
 *
 * \param[in] frames Number of frames to sleep.
 ****************************************************************************/
void mw_sleep(uint16_t frames);

/****** THE FOLLOWING COMMANDS ARE LOWER LEVEL AND USUALLY NOT NEEDED ******/

/************************************************************************//**
 * \brief Send a command to the WiFi module.
 *
 * \param[in] cmd     Pointer to the filled mw_cmd command structure.
 * \param[in] ctx     Context for callback function.
 * \param[in] send_cb Callback for the send operation completion.
 *
 * \return Status of the send procedure.
 ****************************************************************************/
static inline enum lsd_status mw_cmd_send(mw_cmd *cmd, void *ctx,
		lsd_send_cb send_cb)
{
	// Send data on control channel (0).
	return lsd_send(MW_CTRL_CH, cmd->packet, cmd->data_len + 4,
			ctx, send_cb);
}

/************************************************************************//**
 * \brief Try obtaining a reply to a command.
 *
 * \param[in] rep     Buffer to hold the command reply.
 * \param[in] ctx     Context for the reception callback.
 * \param[in] recv_cb Callback for data reception completion.
 *
 * \return Status of the reception procedure.
 ****************************************************************************/
static inline enum lsd_status mw_cmd_recv(mw_cmd *rep, void *ctx,
		lsd_recv_cb recv_cb) {
	return lsd_recv(rep->packet, sizeof(mw_cmd), ctx, recv_cb);
}

enum mw_err mw_gamertag_set(uint8_t slot, struct mw_gamertag *gamertag);

struct mw_gamertag *mw_gamertag_get(uint8_t slot);

enum mw_err mw_log(const char *msg);

#endif /*_MEGAWIFI_H_*/

/** \} */

