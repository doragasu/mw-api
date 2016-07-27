#include "16c550.h"

UartShadow sh;

/************************************************************************//**
 * \brief Initializes the driver. The baud rate is set to UART_BR, and the
 *        UART FIFOs are enabled. This function must be called before using
 *        any other API call.
 ****************************************************************************/
void UartInit(void) {
	// Set line to BR,8N1. LCR[7] must be set to access DLX registers
	UART_LCR = 0x83;
	UART_DLM = UART_DLM_VAL;
	UART_DLL = UART_DLL_VAL;
	UartSet(LCR, 0x03);

	// Disable flow control
	// TODO: Use auto flow control and auto #RTS/#CTS
	
	// Enable FIFOs
	UART_FCR = 0x01;
	// Reset FIFOs
	UartSet(FCR, 0x07);

	// Set IER and MCR to their default values, for the shadow registers
	// to be initialized.
	UartSet(MCR, 0x00);
	UartSet(IER, 0x00);

	// Ready to go! Interrupt and DMA modes were not configured since the
	// Megadrive console lacks interrupt/DMA control pins on cart connector.
}

