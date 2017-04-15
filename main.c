/************************************************************************//**
 * \brief MeGaWiFi API test program. Uses SGDK to ease Genesis/Megadrive
 *        console programming.
 *
 * \author Jesus Alonso (doragasu)
 ****************************************************************************/

#include <stdint.h>
#include <genesis.h>
#include <string.h>
#include "mw/megawifi.h"
// Remove this when 16c550 driver is complete, it should be included only
// by megawifi module.
#include "mw/util.h"
#include "ssid_config.h"
#include "mw/16c550.h"
// SGDK includes must be after mw ones, or they will conflict with stdint.h

#define TCP_TEST_CH		1

#define CMD_BUFLEN		1024

#define dtext(str, col)	do{VDP_drawText(str, col, line++);\
                           if ((line) > 28)line = 0;}while(0)

#define IPV4_BUILD(a, b, c, d)	(((a)<<24) | ((b)<<16) | ((c)<<8) | (d))

static inline void DelayFrames(unsigned int fr) {
	while (fr--) VDP_waitVSync();
}

// Command buffer
static char cmdBuf[CMD_BUFLEN];

static const char echoTestStr[] = "ECHO TEST STRING!";

static const char spinner[] = "|/-\\";
static const char hexTable[] = "0123456789ABCDEF";
static unsigned char line;

static inline void ByteToHexStr(uint8_t byte, char hexStr[]){
	hexStr[0] = hexTable[byte>>4];
	hexStr[1] = hexTable[byte & 0x0F];
	hexStr[2] = '\0';
}

static inline void DWordToHexStr(uint32_t dword, char hexStr[]) {
	ByteToHexStr(dword>>24, hexStr);
	ByteToHexStr(dword>>16, hexStr + 2);
	ByteToHexStr(dword>>8, hexStr + 4);
	ByteToHexStr(dword, hexStr + 6);
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
	MwModuleStart();
}
void MwTcpHelloTest(void) {
	const char dstport[] = "1234";
	const char dstaddr[] = "192.168.1.10";
	const char helloStr[] = "Hello world, this is a MEGADRIVE!\n";
	char echoBuff[80];
	uint16_t len = 80 - 1;

	dtext("Connecting to host...", 1);
	if (MwTcpConnect(TCP_TEST_CH, (char*)dstaddr, (char*)dstport, NULL) < 0) {
		dtext("Connection failed!", 1);
		return;
	}
	dtext("Connecton established", 1);

	// Send hello string on channel 1
	MwSend(TCP_TEST_CH, (uint8_t*)helloStr, sizeof(helloStr) - 1);
	// Try receiving the echoed string
	if (MwRecv((uint8_t**)&echoBuff, &len, UINT32_MAX) == TCP_TEST_CH) {
		echoBuff[len] = '\0';
		VDP_drawText("Rx:", 1, line);
		dtext(echoBuff, 5);
	} else {
		dtext("Error waiting for data", 1);
	}
	// Disconnect from host
	MwTcpDisconnect(TCP_TEST_CH);
	dtext("Disconnected from host.", 1);
}

#define AUTH_MAX 5
void MwApPrint(MwApData *ap) {
	// Character strings related to supported authentication modes
	const char *authStr[AUTH_MAX + 1] = {
		"OPEN", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK", "???"
	};
	const uint8_t authStrLen[] = {
		4, 3, 7, 8, 12, 3
	};
	uint16_t x;
	char hex[3];
	char ssid[33];

	x = 1;
	// Print auth mode
	VDP_drawText(authStr[MIN(ap->auth, AUTH_MAX)], x, line);
	x += authStrLen[MIN(ap->auth, AUTH_MAX)];
	VDP_drawText(", ", x, line);
	x += 2;
	// Print channel
	ByteToHexStr(ap->channel, hex);
	VDP_drawText(hex, x, line);
	x += 2;
	VDP_drawText(", ", x, line);
	x += 2;
	// Print strength
	ByteToHexStr(ap->str, hex);
	VDP_drawText(hex, x, line);
	x += 2;
	VDP_drawText(", ", x, line);
	x += 2;
	// Print SSID
	memcpy(ssid, ap->ssid, ap->ssidLen);
	ssid[(uint8_t)ap->ssidLen] = '\0';
	VDP_drawText(ssid, x, line++);
}

void MwApConfig(void) {
	if (MwApCfgSet(0, WIFI_SSID, WIFI_PASS) < 0) {
		dtext("AP configuration failed!", 1);
		return;
	}
	dtext("AP configuration OK!", 1);
}

void MwConfigGet(uint8_t num) {
	char hex[9];
	char *ssid;
	char *pass;

	MwIpCfg *ip;

	if (MwApCfgGet(num, &ssid, &pass) < 0) {
		dtext("AP CFG GET failed!", 1);
		return;
	}
	VDP_drawText("CFG ", 1, line);
	ByteToHexStr(num, hex);
	dtext(hex, 5);
	VDP_drawText("SSID: ", 1, line);
	dtext(ssid, 7);
	VDP_drawText("PASS: ", 1, line);
	dtext(pass, 7);
	
	if (MwIpCfgGet(num, &ip) < 0) {
		dtext("IP CFG GET failed!", 1);
		return;
	}
	VDP_drawText("IP:   ", 1, line);
	DWordToHexStr(ip->addr, hex);
	dtext(hex, 7);
	VDP_drawText("MASK: ", 1, line);
	DWordToHexStr(ip->mask, hex);
	dtext(hex, 7);
	VDP_drawText("GW:   ", 1, line);
	DWordToHexStr(ip->gateway, hex);
	dtext(hex, 7);
	VDP_drawText("DNS1: ", 1, line);
	DWordToHexStr(ip->dns1, hex);
	dtext(hex, 7);
	VDP_drawText("DNS2: ", 1, line);
	DWordToHexStr(ip->dns2, hex);
	dtext(hex, 7);
}

void MwIpConfig(void) {
	MwIpCfg ip;
	
	ip.addr = IPV4_BUILD(192, 168, 1, 60);
	ip.mask    = IPV4_BUILD(255, 255, 255, 0);
	ip.gateway = IPV4_BUILD(192, 168, 1, 5);
	ip.dns1 = IPV4_BUILD(87, 216, 1, 65);
	ip.dns2 = IPV4_BUILD(87, 216, 1, 66);

	if (MwIpCfgSet(0, &ip) < 0) {
		dtext("IP configuration failed!", 1);
		return;
	}
	dtext("Configured static IP", 1);
}

void MwScanTest(void) {
	char *apData;
	int pos = 0;
	int dataLen;
	MwApData ap;

	// Leave current AP
	dtext("Leaving AP...", 1);
	if (MwApLeave() < 0) {
		dtext("AP leave failed!", 1);
		return;
	}
	// Start scan and get scan result
	dtext("AP left, starting scan", 1);
	if ((dataLen = MwApScan(&apData)) < 0) {
		dtext("AP Scan failed!", 1);
		return;
	}
	while ((pos = MwApFillNext(apData, pos, &ap, dataLen)) <= 0)
		MwApPrint(&ap);
}

void MwSntpConfSet(void) {
	const char *servers[3] = {"0.es.pool.ntp.org", "1.europe.pool.ntp.org",
	"3.europe.pool.ntp.org"};

	if (MwSntpCfgSet((char**)servers, 60, 1, 1) < 0) {
		dtext("SNTP configuration failed!", 1);
		return;
	}
	dtext("SNTP configuration set!", 1);
}

// Get date and time
void MwDatetime(void) {
	char *datetime; 

	if ((datetime = MwDatetimeGet(NULL)) == NULL) {
		dtext("Date and time query failed!", 1);
		return;
	}
	dtext(datetime, 1);
}

// Query and print MegaWiFi version
void MwVersionGetTest(void) {
	char *variant;
	uint8_t verMajor, verMinor;
	char hex[3];

	if (MwVersionGet(&verMajor, &verMinor, &variant) < 0) {
		dtext("Version query failed!", 1);
		return;
	}
	VDP_drawText("MegaWiFi cart version ", 1, line);
	ByteToHexStr(verMajor, hex);
	VDP_drawText(hex, 23, line);
	VDP_drawText(".", 25, line);
	ByteToHexStr(verMinor, hex);
	VDP_drawText(hex, 26, line);
	VDP_drawText("-", 28, line);
	VDP_drawText(variant, 29, line++);
}

void MwFlashTest(void) {
	uint8_t id[3];
	char hex[3];
	const char str[] = "MegaWiFi flash API test string!";
	char *readed;

	// Obtain and print Flash manufacturer and device IDs
	if (MwFlashIdGet(id) < 0) {
		dtext("FlashID query failed!", 1);
		return;
	}
	VDP_drawText("FlashIDs: ", 1, line);
	ByteToHexStr(id[0], hex);
	VDP_drawText(hex, 11, line);
	ByteToHexStr(id[1], hex);
	VDP_drawText(hex, 14, line);
	ByteToHexStr(id[2], hex);
	VDP_drawText(hex, 17, line++);

	// Try reading some data
	if ((readed = (char*)MwFlashRead(0, 80)) == NULL) {
		dtext("Flash read failed!\n", 1);
		return;
	}
	dtext("Flash read OK:", 1);
	dtext(readed, 1);

	// Erase sector
	if (MwFlashSectorErase(0) < 0) {
		dtext("Sector erase failed!", 1);
		return;
	}
	dtext("Sector erase OK!", 1);

	// Write some data
	if (MwFlashWrite(0, (uint8_t*)str, sizeof(str)) < 0) {
		dtext("Flash write failed!", 1);
		return;
	}
	dtext("Flash write OK!", 1);
}

void MwApJoinTest(uint8_t num) {
	if (MwApJoin(num) < 0) {
		dtext("AP join failed!", 1);
		return;
	}
	dtext("Joining AP...", 1);
}

// Get a bunch of numbers from the hardware random number generator
void MwHrng(void) {
	uint32_t *data;
	char hex[9];
	uint8_t i;

	if ((data = (uint32_t*)MwHrngGet(4 * 4 * 16)) == NULL) {
		dtext("HRNG get failed!", 1);
		return;
	}
	dtext("Dice roll:", 1);
	for (i = 0; i < 16; i++) {
		DWordToHexStr(data[4 * i], hex);
		VDP_drawText(hex, 1, line);
		DWordToHexStr(data[4 * i + 1], hex);
		VDP_drawText(hex, 10, line);
		DWordToHexStr(data[4 * i + 2], hex);
		VDP_drawText(hex, 19, line);
		DWordToHexStr(data[4 * i + 3], hex);
		dtext(hex, 28);
	}
}

void MwCfgDefaultSet(void) {
	if (MwDefaultCfgSet() < 0) {
		dtext("Factory reset failed!", 1);
		return;
	}
	dtext("Configuration reset to default.", 1);
}

int main(void) {
	line = 0;
	dtext("MeGaWiFi TEST PROGRAM", 1);

	// MegaWifi module initialization
	MwInit(cmdBuf, CMD_BUFLEN);

	//UartTxLoop();
	//UartEchoLoop();
	//UartSetBits(MCR, 0x10);	// Loopback enable
	//UartLoopbackLoop();

	MwModuleRun();
	// Wait 3 seconds for the module to start
	DelayFrames(3 * 60);
	MwVersionGetTest();
	// Wait 6 additional seconds for the module to get ready
	dtext("Connecting to router...", 1);
	DelayFrames(10 * 60);
//	MwEchoTest();
//	MwTcpHelloTest();
//	MwApConfig();
//	MwIpConfig();
	MwDatetime();
//	MwHrng();
//	MwCfgDefaultSet();
	MwConfigGet(0);
	MwConfigGet(1);
	MwConfigGet(2);
//	MwSntpConfSet();
//	MwScanTest();
//	MwApJoinTest(1);
//	MwFlashTest();

	while(1);

	return 0;	
}

