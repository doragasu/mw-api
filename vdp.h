/************************************************************************//**
 * \brief Basic VDP handling routines. This module implements basic VDP
 * related routines for:
 * - VDP initialization.
 * - Font loading and colour text drawing on planes. 
 * No sprites or any other fancy stuff.
 *
 * \author Jesús Alonso (doragasu)
 * \date 2017
 * \defgroup vdp vdp
 * \{
 ****************************************************************************/
#ifndef _VDP_H_
#define _VDP_H_

#include <stdint.h>

// Screen width in pixels
#define VDP_SCREEN_WIDTH_PX		320
// Screen height in pixels
#define VDP_SCREEN_HEIGHT_PX		224

// Number of tiles per horizontal plane line
#define VDP_PLANE_HTILES		128

/** \addtogroup VdpIoPortAddr VdpIoPortAddr
 *  \brief Addresses for the VDP IO ports
 *  \{ */
/// VDP Data port address
#define VDP_DATA_PORT_ADDR 0xC00000
/// VDP Control port address
#define VDP_CTRL_PORT_ADDR 0xC00004
/// VDP scanline counter address
#define VDP_HV_COUNT_ADDR  0xC00008
/** \} */

/** \addtogroup VdpIoPort VdpIoPort
 *  \brief VDP control and data ports.
 *  \{ */
/// VDP data port, WORD access.
#define VDP_DATA_PORT_W  (*((volatile uint16_t*)VDP_DATA_PORT_ADDR))
/// VDP data port, DWORD access.
#define VDP_DATA_PORT_DW (*((volatile uint32_t*)VDP_DATA_PORT_ADDR))
/// VDP control port, WORD access.
#define VDP_CTRL_PORT_W  (*((volatile uint16_t*)VDP_CTRL_PORT_ADDR))
/// VDP control port, DWORD access.
#define VDP_CTRL_PORT_DW (*((volatile uint32_t*)VDP_CTRL_PORT_ADDR))
/// VDP scanline counter port, WORD access
#define VDP_HV_COUNT_W   (*((volatile uint16_t*)VDP_HV_COUNT_ADDR))
/** \} */

/// Build color in CRAM format, with 3-bit r, b and b components
#define VdpColor(r, g, b)	(((r)<<1) | ((g)<<5) | ((b)<<9))

/** \addtogroup VdpColors VdpColors
 *  \brief Simple color definitions in CRAM format.
 *  \{ */
/// Black color
#define VDP_COLOR_BLACK		VdpColor(0, 0, 0)
/// Red color
#define VDP_COLOR_RED		VdpColor(7, 0, 0)
/// Green color
#define VDP_COLOR_GREEN		VdpColor(0, 7, 0)
/// Blue color
#define VDP_COLOR_BLUE 		VdpColor(0, 0, 7)
/// Cyan color
#define VDP_COLOR_CYAN		VdpColor(0, 7, 7)
/// Magenta color
#define VDP_COLOR_MAGENTA	VdpColor(7, 0, 7)
/// Yellow color
#define VDP_COLOR_YELLOW	VdpColor(7, 7, 0)
/// White color
#define VDP_COLOR_WHITE		VdpColor(7, 7, 7)
/** \} */

/** \addtogroup VdpNametableAddr VdpNametableAddr
 *  \brief Nametable addresses in VRAM
 *  \{ */
/// PLANE A nametable address
#define VDP_PLANEA_ADDR		0x4000
/// PLANE B nametable address
#define VDP_PLANEB_ADDR		0x6000
/// WINDOW nametable address
#define VDP_WIN_ADDR		0x8000
/** \} */

/// Horizontal scroll address en VRAM
#define VDP_HSCROLL_ADDR	0xA000

/** \addtogroup VdpTextColors VdpTextColors
 *  \brief Available text colors, to use with
 *         VdpDrawText() and VdpDrawHex() calls.
 *  \{ */
/// White text color
#define VDP_TXT_COL_WHITE	0x00
/// Cyan text color
#define VDP_TXT_COL_CYAN	0x60
/// Magenta text color
#define VDP_TXT_COL_MAGENTA	0xC0
/** \} */

#define VDP_DMA_68K		(0x00<<6)
#define VDP_DMA_FILL		(0x02<<6)
#define VDP_DMA_COPY		(0x03<<6)

// CD1,CD0,0,0,0,0,1,0,0,CD2,0,0,0,0
#define VDP_DMA_MEM_VRAM	0x4080
#define VDP_DMA_MEM_CRAM	0xC080
#define VDP_DMA_MEM_VSRAM	0x4090

/// RAM types managed by the VDP.
enum {
	VDP_VRAM_RD = 0,	///< VRAM read
	VDP_VRAM_WR,		///< VRAM write
	VDP_CRAM_RD,		///< CRAM read
	VDP_CRAM_WR,		///< CRAM write
	VDP_VSRAM_RD,		///< VSRAM read
	VDP_VSRAM_WR,		///< VSRAM write
	VDP_VRAM_RD_8B,		///< VRAM 8-bit read (undocumented)
	VDP_RAM_TYPE_MAX	///< Limit (do not use)
};

/// Flag of the status register corresponging to the VBLANK interval.
#define VDP_STAT_VBLANK		0x0008

/// VDP registers
typedef enum {
	VDP_REG_MODE1 = 0,	///< Mode set register #1
 	VDP_REG_MODE2,		///< Mode set register #2
	VDP_REG_PLANEA_NT,	///< Plane A pattern name table
	VDP_REG_WIN_NT,		///< Window pattern name table
	VDP_REG_PLANEB_NT,	///< Plane A pattern name table
	VDP_REG_SPR_T,		///< Sprite attribute table base address
	VDP_REG_SPR_PGADDR,	///< Sprite pattern generator base address
	VDP_REG_BGCOL,		///< Background colour
	VDP_REG_UNUSED1,	///< Unused
	VDP_REG_UNUSED2,	///< Unused
	VDP_REG_HINT_CNT,	///< H-Interrupt register
	VDP_REG_MODE3,		///< Mode set register #3
	VDP_REG_MODE4,		///< Mode set register #4
	VDP_REG_HSCROLL,	///< H scroll data table base address
	VDP_REG_NT_ADDR,	///< Nametable pattern generator base address
	VDP_REG_INCR,		///< Auto increment data
	VDP_REG_PSIZE,		///< Plane size
	VDP_REG_WIN_HPOS,	///< Window H position
	VDP_REG_WIN_VPOS,	///< Window V position
	VDP_REG_DMALEN1,	///< DMA Length register #1
	VDP_REG_DMALEN2,	///< DMA Length register #2
	VDP_REG_DMASRC1,	///< DMA source register #1
	VDP_REG_DMASRC2,	///< DMA source register #2
	VDP_REG_DMASRC3,	///< DMA source register #3
	VDP_REG_MAX			///< Limit (do not use)
} VdpReg;

/// Mask for reading/writing from/to VDP memory
extern const uint16_t cdMask[VDP_RAM_TYPE_MAX];

/************************************************************************//**
 * VDP Initialization. Call this function once before using this module.
 ****************************************************************************/
void VdpInit(void);

/************************************************************************//**
 * Write a word to the specified VDP RAM.
 *
 * \param[in] type VDP ram type (see VdpRamAccess).
 * \param[in] addr Address of the specified VRAM.
 * \param[in] val  Word value to write to VDP RAM.
 *
 * \warning function does not check user is effectively requesting a write.
 *          If read is requested, this function will not work.
 ****************************************************************************/
static inline void VdpRamWrite(uint8_t type, uint16_t addr, uint16_t val) {
	if (type >= VDP_RAM_TYPE_MAX) return;
	VDP_CTRL_PORT_DW = (uint32_t)(((cdMask[type] & 0xFF00)<<16) |
		               (cdMask[type] & 0xFF) |
					   ((addr & 0x3FFF)<<16) |
					   (addr>>14));
	VDP_DATA_PORT_W = val;
}

/************************************************************************//**
 * Prepares for a Read/Write operation on the VDP RAM. The function just
 * accesses the control port, but does not perform the read/write operation.
 * User must perform the R/W operation on the VDP data port.
 *
 * \param[in] type VDP ram type and access (see VdpRamAccess).
 * \param[in] addr Address of the specified VRAM.
 ****************************************************************************/
static inline void VdpRamRwPrep(uint8_t type, uint16_t addr) {
	if (type >= VDP_RAM_TYPE_MAX) return;
	VDP_CTRL_PORT_DW = (uint32_t)(((cdMask[type] & 0xFF00)<<16) |
			(cdMask[type] & 0xFF) |
			((addr & 0x3FFF)<<16) |
			(addr>>14));
}

/************************************************************************//**
 * Clears (sets to 0) the specified VRAM range.
 *
 * \param[in] addr VRAM address to clear.
 * \param[in] wlen Length in words of the range to clear.
 ****************************************************************************/
static inline void VdpVRamClear(uint16_t addr, uint16_t wlen) {
	uint16_t i;

	VdpRamRwPrep(VDP_VRAM_WR, addr);

	for (i = 0; i < wlen; i++) VDP_DATA_PORT_W = 0;
}

/************************************************************************//**
 * Loads a 1bpp font on the VRAM, setting specified foreground and
 * background colours.
 *
 * \param[in] font  Array containing the 1bpp font (8 bytes per character).
 * \param[in] chars Number of characters contained in font.
 * \param[in] addr  VRAM Address to load the font in.
 * \param[in] fgcol Foreground colour, in CRAM colour format.
 * \param[in] bgcol Background colour, in CRAM colour format.
 ****************************************************************************/
void VdpFontLoad(const uint32_t *font, uint8_t chars, uint16_t addr,
		uint8_t fgcol, uint8_t bgcol);

/************************************************************************//**
 * Clears (sets to 0) the specified VRAM range.
 *
 * \param[in] addr VRAM address to clear.
 * \param[in] wlen Length in words of the range to clear.
 ****************************************************************************/
void VdpVRamClear(uint16_t addr, uint16_t wlen);

/************************************************************************//**
 * Clears (sets to 0) the plane line.
 *
 * \param[in] planeAddr Address in VRAM of the plane to clear.
 * \param[in] line      Line number to clear.
 *
 * \warning This function starts the DMA fill but does not wait for the fill
 * operation to complete. During fill operation, only VDP status register,
 * H/V counter and PSG registers can be accessed. If you need to access any
 * other VDP register, call VdpDmaWait() first.
 ****************************************************************************/
void VdpLineClear(uint16_t planeAddr, uint8_t line);

/************************************************************************//**
 * Draws text on a plane.
 *
 * \param[in] planeAddr Address in VRAM of the plane used to draw text.
 * \param[in] x         Horizontal text coordinate.
 * \param[in] y         Vertical text coordinate.
 * \param[in] txtColor  Text colour (see VdpTextColors).
 * \param[in] maxChars  Maximum number of characters to write
 * \param[in] text      Null terminated text to write to the plane.
 * \param[in] fillChar  When nonzero, fill unused line space with it.
 ****************************************************************************/
void VdpDrawText(uint16_t planeAddr, uint8_t x, uint8_t y, uint8_t txtColor,
		uint8_t maxChars, const char *text, char fillChar);

void VdpDrawChars(uint16_t planeAddr, uint8_t x, uint8_t y, uint8_t txtColor,
		uint8_t numChars, const char *text);

/************************************************************************//**
 * Draws an 8-bit hexadecimal number on a plane.
 *
 * \param[in] planeAddr Address in VRAM of the plane used to draw text.
 * \param[in] x         Horizontal text coordinate.
 * \param[in] y         Vertical text coordinate.
 * \param[in] txtColor  Text colour (see VdpTextColors).
 * \param[in] num       Number to draw on the plane in hexadecimal format.
 ****************************************************************************/
void VdpDrawHex(uint16_t planeAddr, uint8_t x, uint8_t y, uint8_t txtColor,
		uint8_t num);

void VdpDrawU32(uint16_t planeAddr, uint8_t x, uint8_t y, uint8_t txtColor,
		uint32_t num);

/************************************************************************//**
 * Draws an 8-bit decimal number on a plane.
 *
 * \param[in] planeAddr Address in VRAM of the plane used to draw text.
 * \param[in] x         Horizontal text coordinate.
 * \param[in] y         Vertical text coordinate.
 * \param[in] txtColor  Text colour (see VdpTextColors).
 * \param[in] num       Number to draw on the plane in hexadecimal format.
 *
 * \return Number of characters used by the drawn number.
 ****************************************************************************/
uint8_t VdpDrawDec(uint16_t planeAddr, uint8_t x, uint8_t y, uint8_t txtColor,
		uint8_t num);

/************************************************************************//**
 * Waits until the beginning of the next VBLANK cycle.
 ****************************************************************************/
void VdpVBlankWait(void);

/************************************************************************//**
 * Wait the specified number of frames
 ****************************************************************************/
void VdpFramesWait(uint16_t frames);

/************************************************************************//**
 * Copies from VRAM to VRAM a specified region.
 *
 * \param[in] src Start VRAM memory address to copy.
 * \param[in] dst Destination VRAM address of the copy operation.
 * \param[in] len Length in bytes of the zone to copy.
 *
 * \warning This function starts the DMA copy but does not wait for the copy
 * operation to complete. During copy operation, only VDP status register,
 * H/V counter and PSG registers can be accessed. If you need to access any
 * other VDP register, call VdpDmaWait() first.
 ****************************************************************************/
void VdpDmaVRamCopy(uint16_t src, uint16_t dst, uint16_t len);

/************************************************************************//**
 * Fills specified VRAM region
 *
 * \param[in] dst  Start VRAM memory address to fill.
 * \param[in] len  Length in bytes of the VRAM zone to fill.
 * \param[in] incr Address increment after each byte copy.
 * \param[in] fill Byte to write to the filled zone.
 *
 * \warning This function starts the DMA fill but does not wait for the fill
 * operation to complete. During fill operation, only VDP status register,
 * H/V counter and PSG registers can be accessed. If you need to access any
 * other VDP register, call VdpDmaWait() first.
 ****************************************************************************/
void VdpDmaVRamFill(uint16_t dst, uint16_t len, uint16_t incr, uint16_t fill);

void VdpDma(uint32_t src, uint16_t dst, uint16_t wLen, uint16_t mem);

/************************************************************************//**
 * Waits until there is no DMA operation in progress. Useful only for VRAM
 * DMA copy and VRAM DMA fill operations, since 68k to VRAM copy freezes the
 * processor.
 ****************************************************************************/
#define VdpDmaWait()	while(VDP_CTRL_PORT_W & 0x2)

#endif /*_VDP_H_*/

/** \} */

