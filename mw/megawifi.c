#include "megawifi.h"
#include "lsd.h"

/****************************************************************************
 * \brief MwInit Module initialization. Must be called once before using any
 *        other function. It also initializes de UART.
 ****************************************************************************/
void MwInit(void) {
	// Initialize LSD
	LsdInit();
//	UartInit();

	// TODO Set lines to default status (keep WiFi module in reset)
	MwModuleReset();
	// Power down and Program not active (required for the module to boot)
	UartClrBits(MCR, MW__PRG | MW__PD);
	// Enable control channel
	LsdChEnable(MW_CTRL_CH);
}

/****************************************************************************
 * \brief Send a command to the WiFi module.
 *
 * \param[in] cmd Pointer to the filled MwCmd command structure.
 * \return 0 if OK. Nonzero if error.
 ****************************************************************************/
int MwCmdSend(MwCmd* cmd) {
	// Send data on control channel (0).
	return LsdSend((uint8_t*)cmd, cmd->datalen + 4, MW_CTRL_CH) < 0?-1:0;
}

/****************************************************************************
 * \brief Try obtaining a reply to a command.
 *
 * \param[out] rep Pointer to MwRep structure, containing the reply to the
 *                 command, if the call completed successfully.
 * \return The channel on which the data has been received (0 if it was on
 *         the control channel). Lower than 0 if there was a reception
 *         error.
 ****************************************************************************/
int MwCmdReplyGet(MwRep* rep) {
	uint16_t maxLen = sizeof(MwRep);

	return LsdRecv((uint8_t*)rep, &maxLen, UINT32_MAX);
}

