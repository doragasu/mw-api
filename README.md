# MegaWiFi API

## Introduction

This repository contains the MegaWiFi API, to use the WiFi capabilities of the MegaWiFi cartridges.

## API documentation

The full API documentation [can be found here](https://doragasu.github.io/mw-api/doc/html/index.html).

## Building

You will need a complete Genesis/Megadrive toolchain. The sources use some C standard library calls, such as `memcpy()`, `setjmp()`, `longjmp()`, etc. Thus your toolchain must include a C standard library implementation such as *newlib*.

To build the files, you can use the provided Makefile, suiting it to your needs, or just add the source files to your project to build them.

## Overview

The MegaWiFi API consists of the following modules:
* loop: Loop handling for single threaded Megadrive programs.
* mpool: Simple memory pool implementation.
* megawifi: Communications with the WiFi module and the Internet, including sockets and HTTP/HTTPS.
* mw-msg: MegaWiFi command message definitions.
* util: General purpose utility functions and macros.

The `mw-msg` module contains the message definitions for the different MegaWiFi commands and command replies. Fear not because usually you do not need to use this module, unless you are doing something pretty advanced not covered by the `megawifi` module API.

The `util` module contains general purpose functions and macros not fitting in the other modules, such as `ip_validate()` to check if a string is a valid IP address, `str_to_uint8()` to convert a string to an 8-bit number, etc.

The other modules (`loop`, `mpool`, `megawifi`) are covered in depth below.

There is also a `json` module wich includes `jsmn` library along with some helper functions to parse JSON formatted strings. Please read `jsmn` documentation to learn how its tokenizer works.

### Loop module

This module implements the main loop of the program. It allows easily adding and removing functions to be run on the main loop, as well as timers based on the frame counter. It also provides a hacky interface to perform pseudo syncrhonous calls (through the `loop_pend()` and `loop_post()` semantics) without disturbing the loop execution.

A typical Megadrive game contains a main loop with a structure similar to this:

```C
void main(void)
{
	// Perform initialization
	init();

	// Infinite loop with game logic
	while(1) {
		wait_vblank();
		draw_screen();
		play_sound();
		read_input();
		game_logic();
	}
}
```

The game performs the initialization using `init()` function, and then enters an infinite loop that:
1. Waits for the vertical blanking period to begin.
2. Updates the frame (scroll, sprites, tiles, etc).
3. Keeps the music and SFX playing.
4. Reads controller inputs.
5. Computes game logic, such as collision detection, player/enemy movements, etc. When this step finishes, we have all the data to draw the next frame.

The order of these elements might be slightly different, but these are the usual suspects in loop game design.

On the contrary, the recommended way to write a MegaWiFi program requires using the `loop` module to implement the main loop. When using MegaWiFi API, the code above should be written like this (my recommendation with these examples is that you read the code from the bottom function to the top ones):

```C
#include "mw/util.h"
#include "mw/loop.h"

#define MW_MAX_LOOP_FUNCS 2
#define MW_MAX_LOOP_TIMERS 4

// Run once per frame
static void frame_cb(struct loop_timer *t)
{
	// Avoid compiler warning because unused t parameter
	UNUSED_PARAM(t);

	draw_screen();
	play_sound();
	read_input();
	game_logic();
}

static void main_loop_init(void)
{
	static struct loop_timer frame_timer = {
		.timer_cb = frame_cb,
		.frames = 1,
		.auto_reload = TRUE
	};

	loop_init(MW_MAX_LOOP_FUNCS, MW_MAX_LOOP_TIMERS);
	loop_timer_add(&frame_timer);
}

static void init(void)
{
	// Initialize game stuff
	// ...

	// Initialize game loop
	main_loop_init();
}

void main(void)
{
	// Initialization
	init();

	loop();
	// Function above should never return
}
```

The `init()` function now calls `main_loop_init()` to:
1. Initialize the loop module by calling `loop_init()`.
2. Add a *loop_timer* that runs `frame_cb()` callback once per frame, ideally at the beginning of the vertical blanking period.

As `frame_cb()` is run once per frame, we can (and we **must**) remove the `wait_vblank()` function call, and add all the remaining code previously inside the `while(1)` to the `frame_cb()`.

Using this module, now if you for example want to add another *loop_timer* running each 5 frames to update the background animation, you just have to create the callback and the *loop_timer* structure, to finally add it to the loop by calling `loop_timer_add()`.

In addition to timers, the module also allows adding functions that are run the spare frame time. We will see this in greater detail when talking about the `megawifi` module.

The `loop` module also implements another functionality: pseudo-synchronous event waiting. This eases avoiding the typical callback hell that occurs when coding asynchronous programs. The pseudo-synchronous waits work like this:
1. The function that wants to wait for an event, calls `loop_pend()` function. The execution of the function is then suspended.
2. The suspended function is resumed by doing a `loop_post()` call from other point of the code.

The important thing to take into account, is that while a function is suspended on a `loop_pend()` call, the other loop functions and loop timers continue running undisturbed. Isn't this neat?

Nevertheless, you have to be careful when using `loop_pend()`/`loop_post()`: If you nest several `loop_pend()` calls, the following `loop_post()` calls will resume suspended functions in the reverse order of the `loop_pend()` calls. This is not probably what you want, and thus nesting `loop_pend()` calls is discouraged unless you know what you are doing.

Also when using the `loop` module, you have to be careful not to block a loop function or loop timer. These functions must do their task quickly and exit as fast as possible. Otherwise, if you block by polling (e.g waiting for vblank, or waiting for the player to press a button), other loop functions or loop timers will not be able to get any CPU time. The only allowed way to block a loop function or loop timer, is by calling `loop_pend()` function (or any other function that uses it, such as `mw_sleep()`.

As using the `loop` module seems to make the code more complex, maybe you are wondering why bothering with it. We will come to that later.

### Mpool module

This module implements a very fast and simple memory pool for dynamic memory allocation. Allocated memory is obtained from the unused region between the end of the `.bss` section and the stack top. The implementation is pretty simple: an internal pointer grows when memory is requested using `mp_alloc()`, and is reset to the specified position to free memory using `mp_free_to()`. This restricts the usage of the module to scenarios that free memory in exactly the reverse order in which they requested it (it does not allow generic allocate/free such as `malloc()` does).

One thing interesting about this module is that you can free the memory allocated by several `mp_alloc()` calls with a single `mp_free_to()` call. This behavior can be nasty if you are accustomed to the usual *one `free()` per `malloc()`* scheme, but it is sometimes handy. For example, it helps avoiding memory fragmentation and memory leaks when changing from one game level to another. Imagine that you have two game levels and they allocate memory for several structures:

```C
void level1_init(void)
{
	struct level1_data *l1d = mp_alloc(sizeof(struct level1_data));
	struct enemy *enem = mp_alloc(L1_NUM_ENEMIES * sizeof(struct enemy));
}

void level2_init(void)
{
	struct level2_data *l2d = mp_alloc(sizeof(struct level2_data));
	struct enemy *enem = mp_alloc(L2_NUM_ENEMIES * sizeof(struct enemy));
}
```

Imagine also that the game allocates more memory during level play for other purposes (bullets, explosions, etc). Now you finish the level and want to make sure all the memory is freed to load the new level. With `malloc()`/`free()` you would need to track each allocation and do the corresponding `free()`. But with the `mpool` module it is way easier:

```C
void level1_deinit(void)
{
	mp_free_to(l1d);
}
void level2_deinit(void)
{
	mp_free_to(l2d);
}
```

And that's all, the call to `mp_free_to(l1d)` will deallocate all the memory obtained since the call to the first `mp_alloc()` in `level1_init()`, and you make sure no memory fragmentation and no memory leaks are caused by all the allocations in the level.

### Megawifi module

And we finally arrive to the `megawifi` module API. This API allows of course sending and receiving data to/from the Internet, along with some more functions such as:

* Scanning APs, associating and disassociating to/from them.
* Creating both client and server network sockets.
* Performing HTTP/HTTPS client requests.
* Reading and writing from/to the non-volatile flash memory in the WiFi module.
* Keeping the time and day, accurately synchronized to NTP servers.
* Generating large amounts of random numbers blazingly fast.

You can use this API the hard way (directly sending commands defined in `mw-msg`), or the easy way (through the API calls in `megawifi`). Of course the latter is recommended.

Most API functions require sending and receiving data to/from the WiFi module. But the data send/reception is decoupled from the command functions: the API functions prepare the module to send/receive data, but the data is not sent/received until the `mw_process()` function is called. As the `mw_process()` function polls the WiFi module for data, it is advisable to run it as frequently as possible. The easiest way to do this, is using a *loop_func*. Just set up a *loop_func* running `mw_process()` to ensure this function will be continuously executed, and you're done:

```C
#include "mw/util.h"
#include "mw/loop.h"
#include "mw/megawifi.h"

static void megawifi_loop_cb(struct loop_func *f)
{
	UNUSED_PARAM(f);
	mw_process();
}

static void main_loop_init(void)
{
	// Loop initialization code
	// [...]
	static struct loop_func megawifi_loop = {
		.func_cb = megawifi_loop_cb
	};
	loop_func_add(&megawifi_loop);
}
```

Using a *loop_func* like this makes sending and receiving data during game idle time way easier. Just make sure you set up your loops properly, and leave a bit of time for the loop function running `mw_process()`. Otherwise this function will starve and no data will be sent/received!

About the API calls, basically all of them are synchronous or pseudo-synchronous, excepting the following ones, that are asynchronous and use callbacks to signal task completion:

* `mw_send()`: Send data to the other socket end.
* `mw_recv()`: Receive data from the other socket end.
* `mw_cmd_send()`: Send a command to the WiFi module.
* `mw_cmd_recv()`: Receive a command reply from the WiFi module.

Usually `mw_cmd_send()` and `mw_cmd_recv()` are not needed unless you decide to go down the hard path (using `mw-msg` to build commands yourself). For sending/receiving data, it's up to you using the asynchronous `mw_send()` and `mw_recv()` or their pseudo-synchronous counterparts `mw_send_sync()` and `mw_recv_sync()`.

To save precious RAM, command functions reuse the same buffer. Thus when a command reply is obtained, you have to copy the needed data from the buffer before issuing another command. Otherwise the data in the previously received buffer will be lost.

## Putting all together

In this section several examples explaining how to code typical tasks are presented.

### Connection configuration

MegaWiFi modules have 3 configuration slots, allowing to store 3 different network configurations. The configuration parameters are:

* Access point configuration (SSID, password), using `mw_ap_cfg_set()`. This usually requires a previous AP scan using `mw_ap_scan()` and cycling through scan results using `mw_ap_fill_next()`.
* IP configuration, using `mw_ip_cfg_set()`. Both automatic (DHCP) and manual configurations are supported.

The good news is that you do not need to code the connection configuration, you can use the [wflash bootloader](https://github.com/doragasu/mw-wflash/) to configure the network. As the configuration is stored inside the module, you can use it from your game, even if you delete the wflash bootloader ROM from the MegaWiFi cartridge.

### Program initialization

Basically you have to initialize megawifi and the game loop as explained before. You also have to create a *loop_func* to run `mw_process()` and a *loop_timer* with a 1 frame period to handle the game loop. The code below shows how to do this, and also how to detect if the WiFi module is installed, along with its firmware version.

```C
#include "mw/util.h"
#include "mw/mpool.h"
#include "mw/loop.h"
#include "mw/megawifi.h"

// Length of the wflash buffer
#define MW_BUFLEN	1440

// TCP port to use (set to Megadrive release year ;-)
#define MW_CH_PORT 	1985

// Maximum number of loop functions
#define MW_MAX_LOOP_FUNCS	2

// Maximun number of loop timers
#define MW_MAX_LOOP_TIMERS	4

// Command buffer
static char cmd_buf[MW_BUFLEN];

// Runs mw_process() during idle time
static void idle_cb(struct loop_func *f)
{
	UNUSED_PARAM(f);
	mw_process();
}

// MegaWiFi initialization
static void megawifi_init_cb(struct loop_func  *f)
{
	uint8_t ver_major = 0, ver_minor = 0;
	char *variant = NULL;
	enum mw_err err;

	// megawifi_init_cb is run only once. Use idle_cb from now on
	f->func_cb = idle_cb;

	// Initialize MegaWiFi
	mw_init(cmd_buf, MW_BUFLEN);

	// Try detecting the module
	err = mw_detect(&ver_major, &ver_minor, &variant);

	if (MW_ERR_NONE != err) {
		// Megawifi cart not found!
		// [...]
	} else {
		// MegaWiFi found!
		// [...]
	}
}

// Run the game loop once per frame
static void frame_cb(struct loop_timer *t)
{
	UNUSED_PARAM(t);

	// One iteration of game loop
	draw_screen();
	play_sound();
	read_input();
	game_logic();
}

// Loop initialization
static void main_loop_init(void)
{
	static struct loop_timer frame_timer = {
		.timer_cb = frame_cb,
		.frames = 1,
		.auto_reload = TRUE
	};
	static struct loop_func megawifi_loop = {
		.func_cb = megawifi_init_cb
	};

	loop_init(MW_MAX_LOOP_FUNCS, MW_MAX_LOOP_TIMERS);
	loop_timer_add(&frame_timer);
	loop_func_add(&megawifi_loop);
}

// Global initialization
static void init(void)
{
	// Initialize hardware and game
	// [...]
	// Initialize memory pool
	mp_init(0);
	// Initialize game loop
	main_loop_init();
}

/// Entry point
void main(void)
{
	// Initialization
	init();

	loop();
	// loop() should never return
}
```

### Associating to an AP

Once configured, associating to an AP is easy. Just call `mw_ap_assoc()` with the desired configuration slot, and the module will start the process. You can wait until the association is successful or fails (because of timeout) by calling `mw_ap_assoc_wait()`. The following code tries to associate to an AP during 30 seconds (*fps* must be set previously to 60 on NTSC machines or 50 on PAL machines).

```C
	enum mw_err err;

	err = mw_ap_assoc(slot);
	if (MW_ERR_NONE == err) {
		err = mw_ap_assoc_wait(30 * fps);
	}
	if (MW_ERR_NONE == err) {
		// Association succeeded
	} else {
		// Association failed
	}
```

Once association has succeeded, you can try connecting to a server, or creating a server socket. DNS service will also start automatically after associating to the AP, but it takes a little bit more time. So if you need to use DNS just after associating to an AP, you should wait an additional second, e.g. by calling `mw_sleep(MS_TO_FRAMES(1000))`.

### Connecting to a TCP server

Connecting to a server is straightforward: just call `mw_tcp_connect()` with the channel to use, the destination address (both IPv4 addresses and domain names are supported), the destination port, and optionally the origin port (if NULL, it will be automatically set):

```C
	enum mw_err err;

	err = mw_tcp_connect(1, "www.duck.com", "443", NULL);
	if (MW_ERR_NONE == err) {
		// Connection succeeded
	} else {
		// Connection failed
	}
```

Once connected, you can start sending and receiving data. When no longer needed, remember to close the connection with `mw_tcp_disconnect()`. The channel number must be from 1 to `LSD_MAX_CH - 1` (usually 2). The used channel number will be passed to all the calls relative to the connected socket (think about it like a socket number).

### Creating a TCP server socket

Creating a TCP server socket requires binding it to a port, using `mw_tcp_bind()`. After this, MegaWiFi will automatically accept any incoming connection on this port. You can check when the connection has been established by calling `mw_sock_conn_wait()`:

```C
	enum mw_err err;

	err = mw_tcp_bind(1, 1985);
	if (MW_ERR_NONE == err) {
		// Wait up to an hour for an incoming connection
		err = mw_sock_conn_wait(1, 60 * 60 * fps);
	}
	if (MW_ERR_NONE == err) {
		// Incoming connection established
	} else {
		// Timeout, no connection established
	}
```

### Sending data

You can send data once a connection has been established. The easiest way is using the synchronous variant, but as it suspends the execution of the calling function until data is sent, sometimes the asynchronous version is more convenient. The following code shows how to send *data* buffer of *data_length* length using channel 1, with a two second timeout:

```C
	enum mw_err err;

	err = mw_send_sync(1, data, data_length, 2 * fps);
	if (MW_ERR_NONE == err) {
		// Data sent
	} else {
		// Timeout, data was not sent
	}
```

The same data can be sent this way using the asynchronous API:

```C
void send_complete_cb(enum lsd_status stat, void *ctx)
{
	UNUSED_PARAM(ctx);

	if (LSD_STAT_COMPLETE == stat) {
		// Data successfully sent
	} else {
		// Sending data failed
	}
}

void send_example(void)
{
	enum lsd_status stat;

	stat = mw_send(1, data, data_length, NULL, send_complete_cb);
	if (stat < 0) {
		// Sending failed
	}
}
```

When using the asynchronous API, sometimes you do not need confirmation about when data has been sent. In that case, you do not need to use a completion callback, and can call `mw_send()` with this parameter set to NULL.

### Receiving data

You can receive data once a connection has been established. The easiest way is using the synchronous variant, but as it suspends the execution of the calling function until data is received, sometimes the asynchronous version is more convenient. The following code shows how to receive *data* buffer of *buf_length* maximum length using channel 1, with a 30 second timeout:

```C
	enum mw_err err;

	err = mw_recv_sync(1, data, &buf_length, 30 * fps);
	if (MW_ERR_NONE == err) {
		// Data received
	} else {
		// Failed to receive data
	}
```

Note that when the function successfully returns, *buf_length* contains the number of bytes received.

The same data can be received this way using the asynchronous API:

```C
void recv_complete_cb(enum lsd_status stat, uint8_t ch, char *data,
			uint16_t len, void *ctx)
{
	UNUSED_PARAM(ctx);

	if (LSD_STAT_COMPLETE == stat) {
		// Data successfully received
	} else {
		// Data reception failed
	}
}

void recv_example(void)
{
	enum lsd_status stat;

	stat = mw_recv(data, data_length, NULL, recv_complete_cb);
	if (stat < 0) {
		// Reception failed
	} else {
		// Data will be received by mw_process()
	}
}
```

### Performing an HTTP/HTTPS request

`megawifi` module allows performing HTTP and HTTPS requests in a simple way. HTTP and HTTPS use the same API, the only difference is that setting an SSL certificate is required only if you want to use HTTPS. You can skip this step when using plain HTTP. Performing an HTTPS request requires the following steps. Some of them are optional and depend on the use case.

1. (**Optional**) Set the SSL certificate. This is only required when using HTTPS. You can retrieve the x509 hash of the currently stored certificate by calling `mw_http_cert_query()`. To set a different certificate, call `mw_http_cert_set()`. Once set, the certificate is stored on the non volatile memory, and will remain until replaced with a new one. Only one certificate can be stored at a time. Note you should not use this function unless required, because as it writes to Flash memory, it can wear the storage if used too often.
2. Set the URL (e.g. https://www.example.com). Use `mw_http_url_set()` for this purpose.
3. Set the HTTP method. Most commonly used ones are `MW_HTTP_METHOD_GET` and `MW_HTTP_METHOD_POST`. Use `mw_http_method_set()` to set it.
4. (**Optional**) add HTTP headers to the request. Many aspects of the requests can be controlled via headers. E.g. you can define the formatting of the data you are posting by adding the header "Content-type" with the mime type "text/html", "application/json", etc.
5. Open the connection. In this step, the HTTP or HTTPS connection is opened, and the request (including any headers added) is sent to the server. If the request contains a data payload, in this step the payload length is specified. The function `mw_http_open()` does this.
6. (**Optional**) send the request data payload (if any). This must be performed only if a payload length (greater than 0) was specified in the previous step. This is done the same way as sending data through sockets, with the `mw_send()` or `mw_send_sync()` functions, using the HTTP reserved channel (`MW_CH_HTTP`).
7. Finish the transaction. Call `mw_http_finish()` for the HTTP client to obtain the response to the request, along with its associated headers. If a response includes a data payload, its length is obtained in this step.
8. (**Optional**) if the previous step returned a reply payload length greater than 0, it must be received in this step by calling `mw_recv()` or `mw_recv_sync()` using the HTTP reserved channel (`MW_CH_HTTP`).

By looking to this list of steps, it might look complicated to perform an HTTP request, but the steps are relatively simple, and can be easily added to some functions. E.g., this code allows performing arbritrary GET (without payload) and POST (with JSON payload) requests:

```C
// Performs initial steps of an HTTP request
static int http_begin(enum mw_http_method type, const char *url,
		unsigned int len) {
	enum mw_err err;

	err = mw_http_url_set(url);
	if (!err) {
		err = mw_http_method_set(type);
	}
	if (!err) {
		err = mw_http_open(len);
	}

	return err;
}

// Tries to synchronously receive exactly the indicated data length
static int sync_recv(uint8_t ch, char *buf, int len, uint16_t tout_frames)
{
	int recvd = 0;
	int err = 0;
	uint8_t get_ch = ch;
	int16_t get_len;

	while (err == 0 && recvd < len) {
		get_len = len - recvd;
		err = mw_recv_sync(&get_ch, buf + recvd, &get_len, tout_frames);
		if (!err) {
			if (get_ch != ch) {
				err = -1;
			} else {
				recvd += get_len;
			}
		}
	}

	return err;
}

// Performs final steps of an HTTP request
static int http_finish(char *recv_buf, unsigned int *len)
{
	enum mw_err err;
	uint32_t content_len = 0;

	err = mw_http_finish(&content_len, MS_TO_FRAMES(60000));
	err = err >= 200 && err <= 300 ? 0 : err;
	if (content_len > b.msg_buf_len) {
		err = -1;
	}
	if (!err && content_len) {
		err = sync_recv(MW_HTTP_CH, recv_buf, content_len, 0);
	}
	if (!err && len) {
		*len = content_len;
	}

	return err;
}

/************************************************************************//**
 * \brief Generic HTTP GET request without data payload.
 *
 * \param[in]  url      URL for the request.
 * \param[out] recv_buf Buffer used for HTTP response data.
 * \param[out] len      Length of the response.
 *
 * \return HTTP status code, or -1 if request was not completed.
 ****************************************************************************/
int http_get(const char *url, char *recv_buf, unsigned int *len)
{
	enum mw_err err;

	err = http_begin(MW_HTTP_METHOD_GET, url, 0);
	if (!err) {
		err = http_finish(recv_buf, len);
	}

	return err;
}

/************************************************************************//**
 * \brief Generic POST request with JSON data payload.
 *
 * \param[in]    url          URL for the request.
 * \param[in]    data         Data to send in the POST data payload.
 * \param[in]    length       Length of the data to send.
 * \param[in]    content_type Content-Type HTTP header for the data payload.
 * \param[out]   recv_buf     Buffer used to receive reply data.
 * \param[inout] recv_len     On input, length of recv_buf. On output, length
 *                            of the received reply data.
 *
 * \return HTTP status code or -1 if the request was not completed.
 ****************************************************************************/
int http_post(const char *url, const char *data, int length,
		const char *content_type, char *recv_buf,
		unsigned int *recv_len)
{
	enum mw_err err;

	// If this function fails, it is not critical
	mw_http_header_add("Content-Type", content_type);

	err = http_begin(MW_HTTP_METHOD_POST, url, length);
	if (!err) {
		err = mw_send_sync(MW_HTTP_CH, data, length, 0);
	}
	if (!err) {
		err = http_finish(recv_buf, recv_len);
	}

	return err;
}
```

In case you want HTTPS, you can set a PEM formatted certificate as follows:

```C
void http_cert_set(const char *cert, int cert_len, uint32_t cert_hash)
{
	uint32_t hash = mw_http_cert_query();
	if (hash != cert_hash) {
		mw_http_cert_set(cert_hash, cert, cert_len);
	}
}
```

This function only sets the certificate if it has not been previously stored, avoiding to innecesarily wear the Flash memory. To obtain a correct certificate hash, you can use openssl:

```
$ openssl x509 -hash in <cert_file_name> -noout
```

### Getting the date and time

NOTE: As of version 0.8 of the API, this function is not supported.

MegaWiFi allows to synchronize the date and time to NTP servers. It is important to note that on console power up, the module date and time will be incorrect and should not be used. For the date and time to be synchronized, the module must be associated to an AP with Internet connectivity. Once associated, the date and time is automatically synchronized. The synchronization procedure usually takes only a few seconds, and once completed, date/time should be usable until the console is powered off.


To know if the date and time is in sync, you can use the `mw_sys_stat_get()` command:

```C
	union mw_msg_sys_stat *sys_stat;

	sys_stat = mw_sys_stat_get();
	if (sys_stat && sys_stat->dt_ok) {
		// Date and time syncrhonized
	} else {
		// Date and time **not** synchronized, or other error
	}
```

Once date and time is synchronized, you can get it, both in human readable format, and in the number of seconds elapsed since the epoch, with a single call to `mw_date_time_get()`:

```C
	char *date_time_string;
	uint32_t date_time_bin[2];

	date_time_string = mw_date_time_get(date_time_bin);
```

### Setting and getting gamertag information

MegaWiFi API allows to store and retrieve up to 3 gamertags. The gamertag information is contained in the *mw_gamertag* structure. This structure holds the gamertag unique identifier, nickname, security credentials (password) and a 32x48 avatar (tile information and palette). This example shows how to set a gamertag (excepting the graphics data):

```C
void gamertag_set(int slot, int id, const char *name,
		const char *security, const char *tagline)
{
	struct mw_gamertag gamertag = {};
	struct mw_err err;

	gamertag.id = id;
	strcpy(gamertag.nickname, name);
	strcpy(gamertag.security, security);
	strcpy(gamertag.tagline, tagline);

	err = mw_gamertag_set(slot, &gamertag);
	if (MW_ERR_NONE == err) {
		// Gamertag set successfully
	} else {
		// Setting gamertag failed
	}
}
```

To read the gamertag, just call `mw_gamertag_get()` function, and the information corresponding to the requested slot will be returned:

```C
	struct mw_gamertag *gamertag = mw_gamertag_get(slot);

	if (!gamertag) {
		// Something went wrong
	} else {
		// Success!
	}
```

### Reading the module BSSIDs

The module has two network interfaces, each one with its unique BSSID (MAC address). One interface is used for the station mode, while the other is for the AP mode. Each BSSID is 6-byte long. Currently the API does not allow using the AP mode, so to get the station mode BSSID you can do as follows:

```C
	uint8_t *bssid = mw_bssid_get(MW_IF_STATION);

	if (!bssid) {
		// Something went wrong
	} else {
		// Success! You can use bssid[0] through bssid[5]
	}
```

### Reading and writing to non-volatile Flash

In addition to the standard 32 megabits of Flash ROM memory connected to the Megadrive 68k bus, MegaWiFi cartridges have 28 megabits of additional flash storage, directly usable by the game. This memory is organized in 4 KiB sectors, and supports the following operations:

* Identify: call `mw_flash_id_get()` to obtain the flash memory identifiers. Usually not needed.
* Erase: call `mw_flash_sector_erase()` to erase an entire 4 KiB sector. Erased sectors will be read as 0xFF.
* Program: call `mw_flash_write()` to write the specified data buffer to the indicated address. Prior to programming, **make sure the programmed address range is previously erased**, otherwise operation will fail.
* Read: call `mw_flash_read()` to read the specified amount of data from the indicated address.

This functions can be used e.g. for highscore keeping or DLCs. When using these functions, you have to keep in mind that flash can only be erased in a 1 sector (i.e. 4 KiB) granularity, and thus if e.g. you want to keep highscores, to update one of the high scores, you will have to erase the complete sector, and write it again in its entirety.

Also keep in mind that flash memory suffers from wearing, so do not perform more writes than necessary.

### Test program

The main.c file contains a test program that detects the WiFi module, associates to the AP on slot 0, and tries connecting to `www.duck.com` on port 443.

## Author

This API and documentation has been written by Jesús Alonso (doragasu).

## Contributions

Contributions are welcome. If you find a bug please open an issue, and if you have implemented a cool feature/improvement, please send a pull request.

## License

This program is provided with NO WARRANTY, under the [Mozilla Public License (MPL)](https://www.mozilla.org/en-US/MPL/).

