/************************************************************************//**
 * \brief MeGaWiFi API implementation.
 *
 * \author Jesus Alonso (doragasu)
 * \date 2015
 * \defgroup megawifi MeGaWiFi API implementation.
 * \{
 ****************************************************************************/

#ifndef _MEGAWIFI_H_
#define _MEGAWIFI_H_

#include "16c550.h"
#include "mw-msg.h"

/** \addtogroup megawifi mw_ctrl_pins Pins used to control WiFi module.
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
#define MW_NUM_AP_CFGS		3
/// Number of DSN servers supported per AP configuration.
#define MW_NUM_DNS_SERVERS	2
/// Length of the FSM queue
#define MW_FSM_QUEUE_LEN	8
/// Maximum number of simultaneous TCP connections
#define MW_MAX_SOCK			3
/// Control channel used for LSD protocol
#define MW_CTRL_CH			0

/****************************************************************************
 * \brief MwInit Module initialization. Must be called once before using any
 *        other function. It also initializes de UART.
 ****************************************************************************/
void MwInit(void);

/****************************************************************************
 * \brief Send a command to the WiFi module.
 *
 * \param[in] cmd Pointer to the filled MwCmd command structure.
 * \return 0 if OK. Nonzero if error.
 ****************************************************************************/
int MwCmdSend(MwCmd* cmd);

/****************************************************************************
 * \brief Try obtaining a reply to a command.
 *
 * \param[out] rep Pointer to MwRep structure, containing the reply to the
 *                 command, if the call completed successfully.
 * \return The channel on which the data has been received (0 if it was on
 *         the control channel). Lower than 0 if there was a reception
 *         error.
 ****************************************************************************/
int MwCmdReplyGet(MwCmd *rep);

/****************************************************************************
 * \brief Puts the WiFi module in reset state.
 ****************************************************************************/
#define MwModuleReset()		do{UartSetBits(MCR, MW__RESET);}while(0)

/****************************************************************************
 * \brief Releases the module from reset state.
 ****************************************************************************/
#define MwModuleStart()		do{UartClrBits(MCR, MW__RESET);}while(0)

#endif /*_MEGAWIFI_H_*/

/** \} */

