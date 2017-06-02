// File:    md380tools/applet/src/lcd_driver.c
// Author:  Wolf (DL4YHF) [initial version]
// Date:    2017-04-14
//  Implements a simple LCD driver for Retevis RT3 / Tytera MD380 / etc .
//  Works much better (!) than the stock driver in the original firmware
//             as far as speed and QRM(!) from the display cable is concerned.
//  Written for the 'alternative setup menu', but no dependcies from that,
//  thus may also be used to replace the 'console' display for Netmon & Co.
//
// To include this alternative LCD driver in the patched firmware,
//   add the following lines in applet/Makefile :
//      SRCS += lcd_driver.o
//      SRCS += lcd_fonts.o
//      SRCS += font_8_8.o
//

#include <stm32f4xx.h>
#include <stdint.h>
#include <string.h>       // memset(), ...
#include <gpio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
//#include "md380.h"
//#include "irq_handlers.h" // hardware-specific stuff like "LCD_CS_LOW", etc
#include "lcd_driver.h"   // constants + API prototypes for the *alternative* LCD driver (no "gfx")
#include "task.h"
#include "stm32f4xx_fsmc.h"
#include "stm32f4xx_rcc.h"
#include "usb_cdc.h"

// Regardless of the command line, let the compiler show ALL warnings from here on:
#pragma GCC diagnostic warning "-Wall"


extern const uint8_t font_8_8[256*8]; // extra font with 256 characters from 'codepage 437'
#define LCD_WriteCommand(cmd)	*(volatile uint8_t*)0x60000000 = cmd
#define LCD_WriteData(dta)	*(volatile uint8_t*)0x60040000 = dta
#define LCD_WritePixel(clr)			\
    do {					\
	    LCD_WriteData(((clr) >> 8) & 0xff);	\
	    LCD_WriteData((clr) & 0xff);	\
    } while(0)


//---------------------------------------------------------------------------
// Low-level LCD driver, completely bypasses Tytera's "gfx"
//  (but doesn't reconfigure the LCD controller so we can easily switch
//   back from the 'App Menu' into Tytera's menu, or the main screen)
//
// Based on a driver written by DL4YHF for a display with ST7735,
//   which seems to be quite compatible with the MD380 / RT3,
//   except for the 8-bit bus interface ("FSMC") and a few registers
//   in the LCD controller initialized by the original firmware .
//
// Note: Do NOT erase the screen (or portions of it) before drawing
//   into the framebuffer. That would cause an annoying flicker,
//   because this LCD controller doesn't support double buffering.
//   In other words, we always "paint" directly into the CURRENTLY
//   VISIBLE image - and painting isn't spectacularly fast !
//---------------------------------------------------------------------------

  // Not the same as Tytera's "gfx_write_pixel_to_framebuffer()" (@0x8033728) !
  // Only sends the 16-bit colour value, which requires much less accesses
  // to the external memory interface (which STM calls "FSMC"):
  //  9 accesses per pixel in the Tytera firmware,
  //  2 accesses per pixel in THIS implementation.
static void
LCD_WritePixels( uint16_t wColor, int nRepeats )
{
	while( nRepeats-- )
		LCD_WritePixel(wColor);
}

__attribute__ ((noinline))
void
LCD_ShortDelay(void) // for ST7735 chip sel
{
	int i=4;  // <- minimum for a clean timing between /LCD_WR and /LCD_CS
	while(i--) {
		asm("NOP"); // don't allow GCC to optimize this away !
	}
}

void
LimitInteger( int *piValue, int min, int max)
{
	int value = *piValue;
	if( value < min ) {
		value = min;
	}
	if( value > max ) {
		value = max;
	}
	*piValue = value;
}


static int
LCD_SetOutputRect( int x1, int y1, int x2, int y2 )
  // Sets the coordinates before writing a rectangular area of pixels.
  // Returns the NUMBER OF PIXELS which must be sent to the controller
  // after setting the rectangle (used at least in  ) .
  // In the ST7735 datasheet (V2.1 by Sitronix), these are commands
  // "CASET (2Ah): Column Address Set" and "RASET (2Bh): Row Address Set".
  // Similar but inefficient code is @0x8033728 in D13.020,
  // shown in the disassembly as 'gfx_write_pixel_to_framebuffer()' .
{
	int caset_xs, caset_xe, raset_ys, raset_ye;

	// Crude clipping, not bullet-proof but better than nothing:
	LimitInteger( &x1, 0, LCD_SCREEN_WIDTH-1 );
	LimitInteger( &x2, 0, LCD_SCREEN_WIDTH-1 );
	LimitInteger( &y1, 0, LCD_SCREEN_HEIGHT-1 );
	LimitInteger( &y2, 0, LCD_SCREEN_HEIGHT-1 );
	if( x1>x2 || y1>y2 ) {
		return 0;
	}

	caset_xs = x1;
	caset_xe = x2;
	raset_ys = y1;
	raset_ye = y2;

	LCD_WriteCommand(LCD_CMD_CASET);
	LCD_WriteData((uint8_t)caset_xs);
	LCD_WriteData((uint8_t)caset_xs);
	LCD_WriteData((uint8_t)caset_xe);
	LCD_WriteData((uint8_t)caset_xe);

	LCD_WriteCommand(LCD_CMD_RASET);
	LCD_WriteData((uint8_t)raset_ys);
	LCD_WriteData((uint8_t)raset_ys);
	LCD_WriteData((uint8_t)raset_ye);
	LCD_WriteData((uint8_t)raset_ye);

	LCD_WriteCommand(LCD_CMD_RAMWR);

	// Do NOT de-assert LCD_CS here .. reason below !
	return (1+x2-x1) * (1+y2-y1);
	// The LCD controller now expects as many 16-bit pixel data
	//     for the current drawing area as calculated above .
	// The ST7735 has an auto-incrementing pointer, which
	// eliminates the need to send the "output coordinate" for each
	// pixel of a filled block, bitmap image, or character.
	// But the Tytera firmware seems to ignore this important feature.
}

  // Inefficient method to draw anything (except maybe 'thin BRESENHAM lines').
  // Similar to Tytera's 'gfx_write_pixel_to_framebuffer' @0x08033728 in D13.020 .
  // When not disturbed by interrupts, it took 40 ms to fill 160*128 pixels .
void
LCD_SetPixelAt( int x, int y, uint16_t wColor )
{
	// Timing measured with the original firmware (D13.020),
	//  when setting a single pixel. Similar result with the C code below.
	//           __________   _   _   _   _   _   _   _   _   _________
	// /LCD_WR :           |_| |_| |_| |_| |_| |_| |_| |_| |_|
	//           ____       1   2   3   4   5   6   7   8   9   _____
	// /LCD_CS :     |_________________________________________|
	//
	//               |<-a->|<----------- 1.36 us ----------->|b|
	//               |<--------------- 1.93 us --------------->|
	//                   ->| |<- t_WR_low ~ 70ns
	//
	LCD_EnablePort();
	LCD_WriteCommand(LCD_CMD_CASET);
	// ... but there's something strange in Tytera's firmware,
	//     for reasons only they will now :
	//     In D13.020 @8033728 ("gfx_write_pixel_to_framebuffer"),
	//     the same 8-bit value (XS7..0) is written TWICE instead of
	//     sending a 16-bit coordinate as in the ST7735 datasheet.
	LCD_WriteData((uint8_t) x); // 2nd CASET param: "XS7 ..XS0" ("X-start", lo)
	LCD_WriteData((uint8_t) x); // 2nd CASET param: "XS7 ..XS0" ("X-start", lo)
//	LCD_WriteData((uint8_t) x); // 1st CASET param: "XS15..XS8" ("X-start", hi)

	LCD_WriteCommand(LCD_CMD_RASET);
	LCD_WriteData((uint8_t)y); // 1st RASET param: "YS15..YS8" ("Y-start", hi)
	LCD_WriteData((uint8_t)y); // 1st RASET param: "YS15..YS8" ("Y-start", hi)
//	LCD_WriteData((uint8_t)y); // 2nd RASET param: "YS7 ..YS0" ("Y-start", lo)

	LCD_WriteCommand(LCD_CMD_RAMWR);
	LCD_WritePixels( wColor,1 ); // (8,9) send 16-bit colour in two 8-bit writes
	LCD_ReleasePort();
}

 // Draws a frame-less, solid, filled rectangle
void LCD_FillRect(
        int x1, int y1,  // [in] pixel coordinate of upper left corner
        int x2, int y2,  // [in] pixel coordinate of lower right corner
        uint16_t wColor) // [in] filling colour (RGB565)
{
	int nPixels;

	LCD_EnablePort();
	// This function is MUCH faster than Tytera's 'gfx_blockfill'
	//  (or whatever the original name was), because the rectangle coordinates
	// are only sent to the display controller ONCE, instead of sending
	// a new coordinate for each stupid pixel (which is what the original FW did):
	nPixels = LCD_SetOutputRect( x1, y1, x2, y2 );  // send rectangle coordinates only ONCE

	if( nPixels<=0 ) {
		// something wrong with the coordinates
		LCD_ReleasePort();
		return;
	}

	// The ST7735(?) now knows where <n> Pixels shall be written,
	// so bang out the correct number of pixels to fill the rectangle:
	LCD_WritePixels( wColor, nPixels );

	LCD_ReleasePort();
}

 // Draws a thin horizontal line ..
void LCD_HorzLine(int x1, int y, int x2, uint16_t wColor)
{
	LCD_FillRect( x1, y, x2, y, wColor ); // .. just a camouflaged 'fill rectangle'
}

 // Draws a thin vertical line ..
void LCD_VertLine(int x, int y, int y2, uint16_t wColor)
{
	LCD_FillRect( x, y, x, y2, wColor ); // .. just a camouflaged 'fill rectangle'
}

  // Fills the framebuffer with a 2D colour gradient for testing .
  // If the colour-bit-fiddling below is ok, the display
  // should be filled with colour gradients :
  //       ,----------------, - y=0
  //       |Green ...... Red|
  //       | .            . |
  //       | .            . |
  //       |Cyan Blue Violet|
  //       '----------------' - y=127
  //       |                |
  //      x=0              x=159
  //
void LCD_FastColourGradient(void) {
	int x, y;
	lcd_colour_t px;

	LCD_EnablePort();
	LCD_SetOutputRect(0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);
	for (y = 0; y < LCD_SCREEN_HEIGHT; y++) {
		for (x = 0; x < LCD_SCREEN_WIDTH; x++) {
			px.packed.red = x/5;
			px.packed.green = ((159-x)*2)/5;
			px.packed.blue = y/4;
			LCD_WritePixel(px.RGB565);
		}
	}
	LCD_ReleasePort();
}

void LCD_DrawRGB(uint16_t *rgb, int x, int y, int w, int h) {
	int xx, yy;
	int i = 0;
	LCD_EnablePort();
	if (LCD_SetOutputRect(x, y, x + w - 1, y + h - 1) <= 0) {
		LCD_ReleasePort();
		return;
	}
	for (yy = 0; yy < h; ++yy) {
		for (xx = 0; xx < w; xx++) {
			i = (w * yy) + xx;
			LCD_WritePixel(rgb[i]);
		}
	}
	LCD_ReleasePort();
}

#include "led.h"
void LCD_DrawRGBTransparent(uint16_t *rgb, int x, int y, int w, int h, int t) {
	int xx, yy;
	int i = 0;
	bool last_transparent;
	bool row_has_transparent;

	LCD_EnablePort();
	if (rgb[0] != t) {
		if (LCD_SetOutputRect(x, y, x + w - 1, y + h - 1) <= 0) {
			LCD_ReleasePort();
			return;
		}
		last_transparent = false;
		row_has_transparent = false;
	}
	else {
		last_transparent = true;
		row_has_transparent = true;
	}
	for (yy = y; yy < y + h && yy < LCD_SCREEN_HEIGHT; ++yy) {
		for (xx = x; xx < x + w && xx < LCD_SCREEN_WIDTH; ++xx, ++i) {
			if (rgb[i] == t) {
				if (!last_transparent) {
					last_transparent = true;
					LCD_WriteCommand(LCD_CMD_NOP);
				}
				row_has_transparent = true;
			}
			else {
				if (last_transparent) {
					last_transparent = false;
					if (xx == x) {
						if (LCD_SetOutputRect(xx, yy, x + w - 1, y + h - 1) <= 0) {
							LCD_ReleasePort();
							return;
						}
					}
					else {
						if (LCD_SetOutputRect(xx, yy, x + w - 1, yy) <= 0) {
							LCD_ReleasePort();
							return;
						}
					}
				}
				LCD_WritePixel(rgb[i]);
			}
		}
		if (row_has_transparent)
			last_transparent = true;
	}
	LCD_ReleasePort();
}

// Centre coordinates x, y; radius r; RGB colour c; fill f
void LCD_DrawCircle(int x, int y, int r, uint16_t c, bool f) {
	int X = 0;
	int Y = r;
	int D = 3 - (2 * r);
	while (X < Y) {
		if (f) {
			LCD_HorzLine(x - X, y + Y, x + X - 1, c);
			LCD_HorzLine(x - X, y - Y, x + X - 1, c);
			LCD_HorzLine(x - Y, y + X, x + Y - 1, c);
			LCD_HorzLine(x - Y, y - X, x + Y - 1, c);
		}
		else {
			LCD_SetPixelAt(x + X, y + Y, c);
			LCD_SetPixelAt(x - X, y + Y, c);
			LCD_SetPixelAt(x + X, y - Y, c);
			LCD_SetPixelAt(x - X, y - Y, c);
			LCD_SetPixelAt(x + Y, y + X, c);
			LCD_SetPixelAt(x - Y, y + X, c);
			LCD_SetPixelAt(x + Y, y - X, c);
			LCD_SetPixelAt(x - Y, y - X, c);
		}
		++X;
		if (D < 0) {
			D = D + (4 * X) + 6;
		} else {
			--Y;
			D = D + (4 * (X - Y)) + 10;
		}
	}
}

// Top left coordinates x, y; width w; height h; RGB colour c; fill f
void LCD_DrawRectangle(int x, int y, int w, int h, uint16_t c, bool f) {
	if (f)
		LCD_FillRect(x, y, x+w-1, y+h-1, c);
	else {
		LCD_HorzLine(x, y, x+w-1, c);
		if (h>2) {
			LCD_VertLine(x, y+1, y+h-2, c);
			if (w > 1)
				LCD_VertLine(x+w-1, y+1, y+h-2, c);
		}
		LCD_HorzLine(x, y+h-1, x+w-1, c);
	}
}

// Top left coordinates x, y; terminal coordinates xx, yy; RGB colour c
void LCD_DrawLine(int x, int y, int xx, int yy, uint16_t c) {
	uint8_t dx = xx - x;
	uint8_t dy = yy - y;
	int sx = x < xx ? 1 : -1;
	int sy = y < yy ? 1 : -1;
	int err = dx - dy;
	int e2;

	if (x == xx)
		LCD_VertLine(x, y, yy, c);
	else if (y == yy)
		LCD_HorzLine(x, y, xx, c);
	else {
		do {
			LCD_SetPixelAt(x, y, c);
			e2 = 2 * err;
			if (e2 > -dy) {
				err -= dy;
				x += sx;
			}
			if (e2 < dx) {
				err += dx;
				y += sy;
			}
		} while (x != xx || y != yy);
	}
}

  // Retrieves the address of a character's font bitmap, 8 * 8 pixels .
  //  [in] 8-bit character, most likely 'codepage 437'
  //       (details on this ancient 'VGA'-font in applet/src/font_8_8.c) .
  // Hint: Print out the image applet/src/font_8_8.bmp to find the hex code
  //       of any of CP437's "line drawing" character easily :
  //       table row = upper hex digit, table column = lower hex digit.
  //       Welcome back to the stoneage with fixed font 'text-only' screens :o)
uint8_t *
LCD_GetFontPixelPtr_8x8( uint8_t c)
{
	return (uint8_t*)(font_8_8 + 8*c);
}

int
LCD_GetFontHeight( int font_options )
{
	int font_height = 8;
	if( font_options & LCD_OPT_DOUBLE_HEIGHT ) {
		font_height *= 2;
	}
	return font_height;
}

int
LCD_GetCharWidth( int font_options, char c )
{
	int width = 8;
	// As long as all fonts supported HERE are fixed-width,
	// ignore the character code ('c')  without a warning :
	(void)c;

	if( font_options & LCD_OPT_DOUBLE_WIDTH ) {
		width *= 2;
	}
	return width;
}

int
LCD_GetTextWidth( int font_options, const char *pszText )
{
	int text_width = 0;
	if( pszText != NULL ) {
		while(*pszText) {
			text_width += LCD_GetCharWidth(font_options, *(pszText++) );
		}
	}
	// special service for lazy callers: NULL = "average width of ONE character"
	else {
		text_width = LCD_GetCharWidth(font_options, 'A' );
	}
	return text_width;
}

  // Returns the graphic coordinate (x) to print the next character .
  //
  // Speed: *MUCH* higher than with Tytera's original code (aka 'gfx'),
  //        for reasons explained in LCD_SetOutputRect() .
  //
  // Drawing a zoomed character with 12 * 24 pixels took about 160 microseconds.
  // A screen with 5*13 'large' characters was filled in less than 11 ms.
  // Despite that, only redraw the screen when necessary because the QRM
  // from the display connector cable was still audible in an SSB receiver.
int
LCD_DrawCharAt( // lowest level of 'text output' into the framebuffer
        const char c,      // [in] character code (ASCII)
        int x, int y,      // [in] pixel coordinate
        uint16_t fg_color, // [in] foreground colour (RGB565)
        uint16_t bg_color, // [in] background colour (RGB565)
        int options )      // [in] LCD_OPT_... (bitwise combined)
{
	uint8_t *pbFontMatrix;
	int x_zoom = ( options & LCD_OPT_DOUBLE_WIDTH ) ? 2 : 1;
	int y_zoom = ( options & LCD_OPT_DOUBLE_HEIGHT) ? 2 : 1;
	int font_width,font_height,x2,y2;
	// use the 8*8 pixel font ?
	pbFontMatrix = LCD_GetFontPixelPtr_8x8(c);
	font_width = font_height = 8;
	x2 = x + x_zoom*font_width-1;
	y2 = y + y_zoom*font_height-1;
	if(x2 >=LCD_SCREEN_WIDTH ) {
		x2 = LCD_SCREEN_WIDTH-1;
		// Kludge to avoid an extra check in the loops further below:
		font_width = (1+x2-x) / x_zoom;
		x2 = x + x_zoom*font_width-1; // <- because the number of pixels
		// sent in the loops below must exactly match the rectangle sent
		// to the LCD controller via LCD_SetOutputRect()  !
		// This way, characters are truncated at the right edge.
	}
	// similar kludge to truncate at the bottom
	if(y2 >=LCD_SCREEN_HEIGHT ) {
		y2 = LCD_SCREEN_HEIGHT-1;
		font_height = (1+y2-y) / y_zoom;
		y2 = y + y_zoom*font_height-1;
	}

	// Instead of Tytera's 'gfx_drawtext' (or whatever the original name was),
	// use an important feature of the LCD controller (ST7735 or similar) :
	// Only set the drawing rectangle ONCE, instead of sending a new coordinate
	// to the display for each pixel (which wasted time and caused avoidable QRM):
	LCD_EnablePort();
	if( LCD_SetOutputRect( x, y, x2, y2 ) <= 0 ) {
		// something wrong with the graphic coordinates
		LCD_ReleasePort();
		return x;
	}

	// Without the display rotation and mirroring, we'd use this:
	// for( y=0; y<font_height; ++y)
	//  { for( x=0; x<font_width; ++x)
	//     {  ...
	// But in an RT3/MD380 (with D13.020), the pixels must be rotated and mirrored.
	// To avoid writing the COORDINATE to the ST7735 for each pixel in the matrix,
	// set the output rectangle only ONCE via LCD_SetOutputRect(). After that,
	// the pixels must be read from the font pixel matrix in a different sequence:

	// read 'monochrome pixel' from font bitmap..
	for (y=0; y<font_height; y++) {
		for (x=0; x<font_width; x++) {
			for (x_zoom = ( options & LCD_OPT_DOUBLE_WIDTH ) ? 2 : 1; x_zoom; x_zoom--) {
				if( pbFontMatrix[y] & (0x80>>x) ) {
					// approx. 1us per double-width pixel
					LCD_WritePixels( fg_color, y_zoom );
				}
				else {
					LCD_WritePixels( bg_color, y_zoom );
				}
			}
		}
	}
	LCD_ReleasePort();

	// pixel coord for printing the NEXT character
	return x2+1;
}

  // Clears the struct and sets the output clipping window to 'full screen'.
void
LCD_InitContext( lcd_context_t *pContext )
{
	memset( pContext, 0, sizeof( lcd_context_t ) );
	pContext->x2 = LCD_SCREEN_WIDTH-1;
	pContext->y2 = LCD_SCREEN_HEIGHT-1;
}

  // Draws a zero-terminated ASCII string. Should be simple but versatile.
  //  [in]  pContext, especially pContext->x,y = graphic output cursor .
  //        pContext->x1,x1,x2,y2 = clipping area (planned) .
  //  [out] pContext->x,y = graphic coordinate for the NEXT output .
  //        Return value : horizontal position for the next character (x).
  // For multi-line output (with '\r' or '\n' in the string),
  //     the NEXT line for printing is stored in pContext->y .
  // ASCII control characters with special functions :
  //   \n (new line) : wrap into the next line, without clearing the rest
  //   \r (carriage return) : similar, but if the text before the '\r'
  //                   didn't fill a line on the screen, the rest will
  //                   be filled with the background colour
  //      (can eliminate the need for "clear screen" before printing, etc)
  //   \t (horizontal tab) : doesn't print a 'tab' but horizontally
  //                   CENTERS the remaining text in the current line
int
LCD_DrawString( lcd_context_t *pContext, const char *cp )
{
	int fh = LCD_GetFontHeight(pContext->font);
	int w;
	int left_margin = pContext->x; // for '\r' or '\n', not pContext->x1 !
	int x = pContext->x;
	int y = pContext->y;
	unsigned char c;
	const unsigned char *cp2;
	while( (c=*cp++) != 0 ) {
		switch(c) {
		case '\r' :     // carriage return : almost the same as 'new line', but..
		                // as a service for flicker-free output, CLEARS ALL UNTIL THE END
		                // OF THE CURRENT LINE, so clearing the screen is unnecessary.
			LCD_FillRect( x, y, pContext->x2, y+fh-1, pContext->bg_color );
			// NO BREAK HERE !
		case '\n' :     // new line WITHOUT clearing the rest of the current line
			x = left_margin;
			y += fh;
			break;
		case '\t' :  // horizontally CENTER the text in the rest of the line
			w = 0; // measure the width UNTIL THE NEXT CONTROL CHARACTER:
			cp2 = (unsigned char*)cp;
			while( (c=*cp2++) >= 32 ) {
				w += LCD_GetCharWidth( pContext->font, c ); // suited for proportional fonts (which we don't have here yet?)
			}
			w = (pContext->x2 - x - w) / 2; // "-> half remaining width"
			if( w>0 ) {
				LCD_FillRect( x, y, x+w-1, y+fh-1, pContext->bg_color );
				x += w;
			}
			break;
		default   :  // anything should be 'printable' :
			x = LCD_DrawCharAt( c, x, y, pContext->fg_color, pContext->bg_color, pContext->font );
			break;
		}
	}
	// Store the new "output cursor position" (graphic coord) for the NEXT string:
	pContext->x = x;
	pContext->y = y;
	return x;
}

  // Almost the same as LCD_DrawString,
  // but with all goodies supported by tinyprintf .
  // Total length of the output should not exceed 80 characters.
int
LCD_Printf( lcd_context_t *pContext, const char *fmt, ... )
{
	char szTemp[84]; // how large is the stack ? Dare to use more ?
	va_list va;
	va_start(va, fmt);
	vsnprintf(szTemp, sizeof(szTemp)-1, fmt, va );
	va_end(va);
	szTemp[sizeof(szTemp)-1] = '\0';
	return LCD_DrawString( pContext, szTemp );
}

/**************************************************************************/
/*!
    Display Driver Lowest Layer Settings.
*/
/**************************************************************************/
static void FSMC_Conf(void)
{
	FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  p;

	/*-- FSMC Configuration ------------------------------------------------------*/
	p.FSMC_AddressSetupTime 		= 3;
	p.FSMC_AddressHoldTime 			= 3;
	p.FSMC_DataSetupTime 			= 4;
	p.FSMC_BusTurnAroundDuration	= 0;
	p.FSMC_CLKDivision 				= 1;
	p.FSMC_DataLatency 				= 0;
	p.FSMC_AccessMode 				= FSMC_AccessMode_B;

	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Enable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait= FSMC_AsynchronousWait_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

	/* Enable FSMC Bank1_SRAM Bank */
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
}

SemaphoreHandle_t LCD_Mutex;
enum LCD_Enabled LCD_Enabled = LCD_NOTYET;

void
LCD_EnablePort(void)
{
	xSemaphoreTake(LCD_Mutex, portMAX_DELAY);
	if (LCD_Enabled != LCD_ENABLED) {
		/* Set up pins */
		gpio_af_setup(pin_lcd_rd->bank, pin_lcd_rd->pin |
			pin_lcd_wr->pin | pin_lcd_rs->pin | pin_lcd_d0->pin |
			pin_lcd_d1->pin | pin_lcd_d2->pin | pin_lcd_d3->pin,
			GPIO_AF_FSMC, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		gpio_af_setup(pin_lcd_d4->bank, pin_lcd_d4->pin |
			pin_lcd_d5->pin | pin_lcd_d6->pin | pin_lcd_d7->pin,
			GPIO_AF_FSMC, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		gpio_output_setup(pin_lcd_rst->bank, pin_lcd_rst->pin | pin_lcd_cs->pin,
			GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		gpio_output_setup(pin_lcd_bl->bank, pin_lcd_bl->pin, GPIO_Speed_100MHz,
			GPIO_OType_PP, GPIO_PuPd_NOPULL);
		/* Prevent a glitch in the matrix */
		gpio_output_setup(GPIOA, GPIO_Pin_6, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		GPIO_ResetBits(GPIOA, GPIO_Pin_6);
		gpio_output_setup(GPIOD, GPIO_Pin_2 | GPIO_Pin_3, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		GPIO_ResetBits(GPIOD, GPIO_Pin_2 | GPIO_Pin_3);
		LCD_Enabled = LCD_ENABLED;
	}
	pin_reset(pin_lcd_cs);
	LCD_ShortDelay();	// TODO: Is this needed?
}

void
LCD_ReleasePort(void)
{
	LCD_ShortDelay();	// TODO: Is this needed?
	pin_set(pin_lcd_cs);
	xSemaphoreGive(LCD_Mutex);
}

extern uint16_t wlarc_logo[20480];
void LCD_Init(void)
{
	LCD_Mutex = xSemaphoreCreateMutex();
	RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	LCD_EnablePort();
	FSMC_Conf();

	pin_reset(pin_lcd_rst);
	vTaskDelay(120);
	pin_set(pin_lcd_rst);
	vTaskDelay(120);

	LCD_WriteCommand(LCD_CMD_COLMOD);
	LCD_WriteData(0x05);	// 16bpp
	LCD_WriteCommand(LCD_CMD_MADCTL);
#ifdef MD_390
	LCD_WriteData(0x60);	// Was 0xc8
#else
	LCD_WriteData(0xa8);	// Was 0x08
#endif
	LCD_WriteCommand(LCD_CMD_SETCYC);
	LCD_WriteData(0x00);	// Column inversion, 89 clocks per line
	LCD_WriteCommand(LCD_CMD_SLPOUT);
	vTaskDelay(130);
	LCD_ReleasePort();
	LCD_DrawRGB(wlarc_logo, 0, 0, 160, 128);
	LCD_EnablePort();
	LCD_WriteCommand(LCD_CMD_DISPON);
	LCD_WriteCommand(LCD_CMD_RAMWR);
	LCD_ReleasePort();
}

/* EOF < md380tools/applet/src/lcd_driver.c > */
