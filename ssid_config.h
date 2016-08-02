// Add your AP credentials used for tests to this file. Then to avoid them
// being uploaded to GitHub, thell git to ignore changes to this file:
//
// git update-index --assume-unchanged include/ssid_config.h 
//
// This way they will never be commited.
//
// For reference, see
//   https://www.kernel.org/pub/software/scm/git/docs/git-update-index.html

#ifndef _SSID_CONFIG_H_
#define _SSID_CONFIG_H_

#error "Edit WIFI_SSID and WIFI_PASS to match your AP credentials, then follow above instructions and remove this line."

#define WIFI_SSID "MySsid"
#define WIFI_PASS "MyPassword"

#endif // _SSID_CONFIG_H_
