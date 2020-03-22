#ifndef _FONT_H_
#define _FONT_H_

#include <stdint.h>

/// Number of characters in font
#define FONT_NCHARS	96

/// 1 bpp font. 8 bytes per character.
extern const uint32_t font[FONT_NCHARS * 2];


#endif /*_FONT_H_*/

