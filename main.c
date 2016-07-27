/************************************************************************//**
 * \brief MeGaWiFi API test program. Uses SGDK to ease Genesis/Megadrive
 *        console programming.
 *
 * \author Jesus Alonso (doragasu)
 ****************************************************************************/

#include "mw/megawifi.h"
// Remove this when 16c550 driver is complete, it should be included only
// by megawifi module.
#include "mw/16c550.h"
#include "mw/lsd.h"
#include "mw/util.h"
// SGDK includes must be after mw ones, or they will conflict with stdint.h
#include <genesis.h>

#define TCP_TEST_CH		1

#define dtext(str, col)	do{VDP_drawText(str, col, line++);\
                           if ((line) > 28)line = 0;}while(0)

static inline void DelayFrames(unsigned int fr) {
	while (fr--) VDP_waitVSync();
}

static const char echoTestStr[] = "OLAKASE, ECO O KASE!";

static const char spinner[] = "|/-\\";
static const char hexTable[] = "0123456789ABCDEF";
static unsigned char line;

// Command to send
static MwCmd cmd;
// Command reply
static MwRep rep;

static inline void ByteToHexStr(uint8_t byte, char hexStr[]){
	hexStr[0] = hexTable[byte>>4];
	hexStr[1] = hexTable[byte & 0x0F];
	hexStr[2] = '\0';
}

static inline void CheckUartIsr(uint8_t val) {
	uint8_t c;
	char hexByte[3];

	if (val != (c = UART_ISR)) {
		dtext("WARNING: ISR = 0x", 1);
		ByteToHexStr(c, hexByte);
		dtext(hexByte, 18);
	}
}

static inline void SpinTick(void) {
	static uint8_t beat = 0;
	static char spin[] = {'|', '\0'};

	if (!(beat++ & 0x3F)) {
		dtext(spin, 1);
		spin[0] = spinner[(beat>>6) & 3];
	}
}

// Function for testing UART TX
void UartTxLoop(void) {
	uint8_t c;
	char hex[3];
	// Test: write to scratchpad register and read the value back.
	UART_SPR = 0x55;
	c = UART_SPR;
	if (0x55 == c) {
		dtext("0x55 TEST SUCCEEDED!", 1);
	} else {
		dtext("0x55 TEST FAILED!:", 1);
		ByteToHexStr(c, hex);
		dtext(hex, 20);
	}
	UART_SPR = 0xAA;
	c = UART_SPR;
	if (0xAA == c) {
		dtext("0xAA TEST SUCCEEDED!", 1);
	} else {
		dtext("0xAA TEST FAILED!:", 1);
		ByteToHexStr(c, hex);
		dtext(hex, 20);
	}


	CheckUartIsr(0xC1);
	dtext("TRANSMIT START!", 1);

	while(1) {
		// UART transmission test
		SpinTick();
		while (!UartTxReady());
		// Fill FIFO (write 16 characters).
		for (c = '0'; c < '@'; c++) UartPutc(c);
		// Wait for VBLANK interval
		//VDP_waitVSync();
	}
}

// Uses loopback to echo characters
void UartLoopbackLoop(void) {
	uint8_t sent;
	volatile uint32_t i;

	for (i = 0; i < 100000; i++);
	MwModuleStart();
	for (i = 0; i < 100000; i++);

	CheckUartIsr(0xC1);

	UartResetFifos();
	dtext("LOOPBACK TEST START!", 1);
	sent = '0';
	UartPutc(sent);	// Tx first byte, that will be continuously echoed
	while (1) {
		if (UartRxReady()) {
			if (UartGetc() != sent) break;
			sent = '~' == sent?'0':sent + 1;
			UartPutc(sent);
			SpinTick();
		}
	}
	// Exit with error!
	dtext("LOOPBACK TEST ERROR!", 1);
}

// Echoes characters received
void UartEchoLoop(void) {
	uint8_t lsr;

	CheckUartIsr(0xC1);

	dtext("ECHO TEST START!", 1);

	while (1) {

		if ((lsr = UART_LSR) & 1) {
			UartPutc(UartGetc());
			SpinTick();
		}
		else if (lsr & 0x97) UartPutc('E');
	}
}

void MwModuleRun() {
	// Hold reset some time and then release it.
	dtext("Resetting WiFi...", 1);
	DelayFrames(30);
	dtext("Connecting to router...", 1);
	MwModuleStart();
}

void MwEchoTest(void) {
	int i;

	// Try sending and receiving echo
	cmd.cmd = MW_CMD_ECHO;
	cmd.datalen = sizeof(echoTestStr) - 1;
	strcpy((char*)cmd.data, echoTestStr);
	dtext("Sending echo string...\n", 1);
	UartResetFifos();
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Echo recv failed!\n", 1);
		return;
	}
	dtext("Got response!\n", 1);
	for (i = 0; i < cmd.datalen; i++) {
		if (cmd.data[i] != rep.data[i]) break;
	}
	if (i != cmd.datalen) {
		dtext("Echo reply differs!", 1);
	}
	else {
		dtext("Echo test OK", 1);
	}
}

void MwTcpHelloTest(void) {
	MwMsgInAddr* addr = (MwMsgInAddr*)cmd.data;
	const char dstport[] = "1234";
	const char dstaddr[] = "192.168.1.10";
	const char helloStr[] = "Hello world, this is a MEGADRIVE!\n";
	char echoBuff[80];
	uint16_t len = 80 - 1;

	// Configure TCP socket
	cmd.cmd = MW_CMD_TCP_CON;
	// Length is the length of both ports, the channel and the address.
	cmd.datalen = 6 + 6 + 1 + sizeof(dstaddr);
	memset(addr->dst_port, 0, cmd.datalen);
	strcpy(addr->dst_port, dstport);
	strcpy(addr->data, dstaddr);
	addr->channel = TCP_TEST_CH;

	// Try to establish connection
	dtext("Connecting to host...", 1);
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Connection failed!", 1);
		return;
	}
	// TODO check returned code
	dtext("Connecton established", 1);

	// Enable channel 1
	LsdChEnable(TCP_TEST_CH);
	// Send hello string on channel 1
	LsdSend((uint8_t*)helloStr, sizeof(helloStr) - 1, TCP_TEST_CH);
	// Try receiving the echoed string
	if (TCP_TEST_CH == LsdRecv((uint8_t*)echoBuff, &len, UINT32_MAX)) {
		echoBuff[len] = '\0';
		VDP_drawText("Rx:", 1, line);
		dtext(echoBuff, 5);
	} else {
		dtext("Error waiting for data", 1);
	}
	// Disconnect from host
	cmd.cmd = MW_CMD_TCP_DISC;
	cmd.datalen = 1;
	cmd.data[0] = TCP_TEST_CH;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("Disconnect failed!", 1);
	} else {
		dtext("Disconnected from host.", 1);
	}
}

#define AUTH_MAX 5
void MwApScanPrint(MwRep *rep) {
	// Character strings related to supported authentication modes
	const char *authStr[AUTH_MAX + 1] = {
		"OPEN", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK", "???"
	};
	uint16_t i,  x;
	char hex[3];
	char ssid[33];

	i = 0;
	while (i < rep->datalen) {
		x = 1;
		// Print auth mode
		VDP_drawText(authStr[MIN(rep->data[i], AUTH_MAX)], x, line);
		x += strlen(authStr[MIN(rep->data[i++], AUTH_MAX)]);
		VDP_drawText(", ", x, line);
		x += 2;
		// Print channel
		ByteToHexStr(rep->data[i++], hex);
		VDP_drawText(hex, x, line);
		x += 2;
		VDP_drawText(", ", x, line);
		x += 2;
		// Print strength
		ByteToHexStr(rep->data[i++], hex);
		VDP_drawText(hex, x, line);
		x += 2;
		VDP_drawText(", ", x, line);
		x += 2;
		// Print SSID
		memcpy(ssid, rep->data + i + 1, rep->data[i]);
		i += rep->data[i] + 1;
		VDP_drawText(ssid, x, line++);
	}
}

void MwScanTest(void) {
	// Leave current AP
	dtext("Leaving AP...", 1);
	cmd.cmd = MW_CMD_AP_LEAVE;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if (MwCmdReplyGet(&rep) < 0) {
		dtext("AP leave failed!", 1);
		return;
	}
	// Start scan and get scan result
	dtext("AP left, starting scan", 1);
	cmd.cmd = MW_CMD_AP_SCAN;
	cmd.datalen = 0;
	MwCmdSend(&cmd);
	if ((MwCmdReplyGet(&rep) < 0) || (MW_CMD_OK != rep.rep)) {
		dtext("AP Scan failed!", 1);
		return;
	}
	MwApScanPrint(&rep);
}

int main(void) {
	line = 0;
	dtext("MeGaWiFi TEST PROGRAM", 1);

	// MegaWifi module initialization
	MwInit();

	//UartTxLoop();
	//UartEchoLoop();
	//UartSetBits(MCR, 0x10);	// Loopback enable
	//UartLoopbackLoop();

	MwModuleRun();
	// Wait 8 additional seconds for the module to get ready
	DelayFrames(8 * 60);
//	MwEchoTest();
	MwTcpHelloTest();
	MwScanTest();

	while(1);

	return 0;	
}

