// File:    md380tools/applet/src/lcd_driver.c
// Author:  Wolf (DL4YHF) [initial version]
// Date:    2017-04-14
//  Implements a simple LCD driver for ST7735-compatible controllers,
//             tailored for Retevis RT3 / Tytera MD380 / etc .
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

  // I/O address taken from DL4YHF's dissassembly, @0x803352c in D13.020 .
static void
LCD_WriteCommand( uint8_t bCommand )
{
	*(volatile uint8_t*)0x60000000 = bCommand;
}

static void
LCD_WriteData( uint8_t bData )
{
	// I/O address taken from DL4YHF's dissassembly, @0x8033534 in D13.020 .
	*(volatile uint8_t*)0x60040000 = bData; // one address bit controls "REGISTER SELECT"
}

  // Not the same as Tytera's "gfx_write_pixel_to_framebuffer()" (@0x8033728) !
  // Only sends the 16-bit colour value, which requires much less accesses
  // to the external memory interface (which STM calls "FSMC"):
  //  9 accesses per pixel in the Tytera firmware,
  //  2 accesses per pixel in THIS implementation.
static void
LCD_WritePixels( uint16_t wColor, int nRepeats )
{
	while( nRepeats-- ) {
		// ST7735 datasheet V2.1 page 39, "8-bit data bus for 16-bit/pixel,
		//                 (RGB 5-6-5-bit input), 65K-Colors, Reg 0x3A = 0x05 " :
		LCD_WriteData( wColor >> 8 );  // 5 "blue" bits + 3 upper "green" bits
		// __NOP;     // some delay between these two 8-bit transfers ?
		LCD_WriteData( wColor );       // 3 lower green bits + 5 "red" bits
		// Don't de-assert LCD_CS here yet ! In a character,
		// more pixels will be emitted into the same output rectangle
	}
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

#if(0)
	// one would expect this...
	caset_xs = x1;
	caset_xe = x2;
	raset_ys = y1;
	raset_ye = y2;
#else
	// but in an RT3, 'X' and 'Y' had to be swapped, and..
	caset_xs = y1;
	caset_xe = y2;
	raset_ys = 159-x2;  // low  'start' value
	raset_ye = 159-x1;  // high 'end' value
#endif

	LCD_WriteCommand( 0x2A ); // ST7735 DS V2.1 page 100 : "CASET" ....
	// ... but beware: 'columns' and 'rows' from the ST7735's
	//     point of view must be swapped, because the display initialisation
	//     (which still only takes place in TYTERA's part of the firmware),
	//     the possibility of rotating the display by 90° BY THE LCD CONTROLLER
	//     are not used !
	// To 'compensate' this in software, x- and y-coordinates would have
	// to be swapped, and (much worse and CPU-hogging) the character bitmaps
	// would have to be rotated by 90° before being sent to the controller.
	//     The ST7735 datasheet (V2.1 by Sitronix) explains on pages 60..61 .
	//     how to control "Memory Data Write/Read Direction" via 'MADCTL' .
	//     The use the term "X-Y Exchange" for a rotated display,
	//     and there are EIGHT combinations of 'Mirror' and 'Exchange'.
	//
	// The original firmware (D13.020 @ 0x08033590) uses different 'MADCTL'
	//     settings, depending on some setting in SPI-Flash (?):
	//  * 0x08 for one type of display     : MADCTL.MY=0, MX=0, MV=0, ML=0, RGB=1
	//  * 0x48 for another type of display : MADCTL.MY=0, MX=1, MV=0, ML=0, RGB=1
	LCD_WriteData((uint8_t)(caset_xs>>8)); // 1st param: "XS15..XS8" ("X-start", hi)
	LCD_WriteData((uint8_t) caset_xs    ); // 2nd param: "XS7 ..XS0" ("X-start", lo)
	LCD_WriteData((uint8_t)(caset_xe>>8)); // 3rd param: "XE15..XE8" ("X-end",   hi)
	LCD_WriteData((uint8_t) caset_xe    ); // 4th param: "XE7 ..XE0" ("X-end",   lo)

	LCD_WriteCommand( 0x2B ); // ST7735 DS V2.1 page 102 : "RASET" ....
	LCD_WriteData((uint8_t)(raset_ys>>8)); // 1st param: "YS15..YS8" ("Y-start", hi)
	LCD_WriteData((uint8_t) raset_ys    ); // 2nd param: "YS7 ..YS0" ("Y-start", lo)
	LCD_WriteData((uint8_t)(raset_ye>>8)); // 3rd param: "YE15..YE8" ("Y-end",   hi)
	LCD_WriteData((uint8_t) raset_ye    ); // 4th param: "YE7 ..YE0" ("Y-end",   lo)

	LCD_WriteCommand( 0x2C ); // ST7735 DS V2.1 page 104 : "RAMWR" / "Memory Write"

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
	LCD_WriteCommand( 0x2A ); // (1) ST7735 DS V2.1 page 100 : "CASET" ....
	// ... but there's something strange in Tytera's firmware,
	//     for reasons only they will now :
	//     In D13.020 @8033728 ("gfx_write_pixel_to_framebuffer"),
	//     the same 8-bit value (XS7..0) is written TWICE instead of
	//     sending a 16-bit coordinate as in the ST7735 datasheet.
#if(0) // For a "normally" mounted display, we would use THIS:
	LCD_WriteData((uint8_t)(x>>8)); // 1st CASET param: "XS15..XS8" ("X-start", hi)
	LCD_WriteData((uint8_t) x    ); // 2nd CASET param: "XS7 ..XS0" ("X-start", lo)
#else  // ... but for the unknown LCD controller in an RT3, Tytera only sends this:
	LCD_WriteData( (uint8_t)y    ); // (2) 1st CASET param: "XS15..XS8" ("X-start", hi)
	LCD_WriteData( (uint8_t)y    ); // (3) 2nd CASET param: "XS7 ..XS0" ("X-start", lo)
#endif // ST7735 or whatever-is-used in an MD380 / RT3 ?

	LCD_WriteCommand( 0x2B ); // (4) ST7735 DS V2.1 page 102 : "RASET" ....
#if(0) // For a "normally" mounted display, we would use THIS:
	LCD_WriteData((uint8_t)(y>>8)); // 1st RASET param: "YS15..YS8" ("Y-start", hi)
	LCD_WriteData((uint8_t) y    ); // 2nd RASET param: "YS7 ..YS0" ("Y-start", lo)
#else  // ... but for the unknown LCD controller in an RT3, we need this:
	LCD_WriteData( (uint8_t)(159-x) ); // (5) 1st RASET param: "YS15..YS8" ("Y-start", hi)
	LCD_WriteData( (uint8_t)(159-x) ); // (6) 2nd RASET param: "YS7 ..YS0" ("Y-start", lo)
#endif // ST7735 or whatever-is-used in an MD380 / RT3 ?

	LCD_WriteCommand( 0x2C );    // (7) ST7735 DS V2.1 p. 104 : "RAMWR"
	LCD_WritePixels( wColor,1 ); // (8,9) send 16-bit colour in two 8-bit writes
	LCD_ReleasePort();
}

 // Draws a frame-less, solid, filled rectangle
void LCD_FillRect(
        int x1, int y1,  // [in] pixel coordinate of upper left corner
        int x2, int y2,  // [in] pixel coordinate of lower right corner
        uint16_t wColor) // [in] filling colour (BGR565)
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
void LCD_ColorGradientTest(void)
{
	int x,y;
	for(y=0; y<128; ++y) {
		for(x=0; x<160; ++x) {
			// 'Native' colour format, found by trial-and-error:
			//   BGR565 = 32 levels of BLUE  (in bits 15..11),
			//            64 levels of GREEN (in bits 10..5),
			//            32 levels of RED   (in bits 4..0).
			LCD_SetPixelAt( x,y,
			    (x/5) |         // increasing from left to right: RED
			    ((63-(2*x)/5)<<5) // increasing from right to left: GREEN
			    |((y/4)<<11) );   // increasing from top to bottom: BLUE
		}
	}
}

void LCD_FastColourGradient(void) {
	int x, y;
	uint16_t px;
	LCD_EnablePort();
	LCD_SetOutputRect(0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);
	for (x = LCD_SCREEN_WIDTH - 1; x >= 0; --x) {
		for (y = 0; y < LCD_SCREEN_HEIGHT; ++y) {
			px = (x/5)|((63-(2*x)/5)<<5)|((y/4)<<11);
			LCD_WriteData(px>>8);
			LCD_WriteData(px&255);
		}
	}
	LCD_ReleasePort();
}

void LCD_DrawBGR(uint16_t *bgr, int x, int y, int w, int h) {
	int xx, yy;
	int i = 0;
	LCD_EnablePort();
	if (LCD_SetOutputRect(x, y, x + w - 1, y + h - 1) <= 0)
		return;
	for (xx = w; xx > 0; --xx) {
		for (yy = 0; yy < h; ++yy) {
			i = (w * yy) + xx;
			LCD_WriteData(bgr[i]>>8);
			LCD_WriteData(bgr[i]&255);
		}
	}
	LCD_ReleasePort();
}

void LCD_DrawBGRTransparent(uint16_t *bgr, int x, int y, int w, int h, int t) {
	int xx, yy;
	int i = 0;
	for (yy = y; yy < y + h && yy < LCD_SCREEN_HEIGHT; ++yy) {
		for (xx = x; xx < x + w && xx < LCD_SCREEN_WIDTH; ++xx) {
			if (bgr[i] != t)
				LCD_SetPixelAt( xx, yy, bgr[i] );
			++i;
		}
	}
}

// Centre coordinates x, y; radius r; BGR colour c; fill f
void LCD_DrawCircle(int x, int y, int r, uint16_t c, bool f) {
	int xx;
	int X = 0;
	int Y = r;
	int D = 3 - (2 * r);
	while (X < Y) {
		LCD_SetPixelAt(x + X, y + Y, c);
		LCD_SetPixelAt(x - X, y + Y, c);
		LCD_SetPixelAt(x + X, y - Y, c);
		LCD_SetPixelAt(x - X, y - Y, c);
		LCD_SetPixelAt(x + Y, y + X, c);
		LCD_SetPixelAt(x - Y, y + X, c);
		LCD_SetPixelAt(x + Y, y - X, c);
		LCD_SetPixelAt(x - Y, y - X, c);
		if (f) {
			for (xx = x - X + 1; xx < x + X; ++xx) {
				LCD_SetPixelAt(xx, y + Y, c);
				LCD_SetPixelAt(xx, y - Y, c);
			}
			for (xx = x - Y + 1; xx < x + Y; ++xx) {
				LCD_SetPixelAt(xx, y + X, c);
				LCD_SetPixelAt(xx, y - X, c);
			}
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

// Top left coordinates x, y; width w; height h; BGR colour c; fill f
void LCD_DrawRectangle(int x, int y, int w, int h, uint16_t c, bool f) {
	int xx, yy;
	for (yy = 0; yy < h; yy++) {
		for (xx = 0; xx < w; xx++) {
			if (f || yy == 0 || yy == h - 1 || xx == 0 || xx == w - 1) {
				LCD_SetPixelAt(x + xx, y + yy, c);
			}
		}
	}
}

// Top left coordinates x, y; terminal coordinates xx, yy; BGR colour c
void LCD_DrawLine(int x, int y, int xx, int yy, uint16_t c) {
	uint8_t dx = xx - x;
	uint8_t dy = yy - y;
	int sx = x < xx ? 1 : -1;
	int sy = y < yy ? 1 : -1;
	int err = dx - dy;
	int e2;
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

  // Converts a 'native' colour (in the display's hardware-specific format)
  // into red, green, and blue component; each ranging from 0 to ~~255 .
uint32_t
LCD_NativeColorToRGB( uint16_t native_color )
{
	rgb_quad_t rgb;
	int i;
	i = (native_color & LCD_COLOR_RED) >> LCD_COLORBIT0_RED; // -> 0..31 (5 'red bits' only)
	i = (255*i) / 31;
	rgb.s.r = (uint8_t)i;
	i = (native_color & LCD_COLOR_GREEN)>> LCD_COLORBIT0_GREEN; // -> 0..63 (6 'green bits')
	i = (255*i) / 63;
	rgb.s.g = (uint8_t)i;
	i = (native_color & LCD_COLOR_BLUE) >> LCD_COLORBIT0_BLUE; // -> 0..31 (5 'blue bits')
	i = (255*i) / 31;
	rgb.s.b = (uint8_t)i;
	rgb.s.a = 0; // clear bits 31..24 in the returned DWORD (nowadays known as uint32_t)

	return rgb.u32;
}

  // Converts an RGB mix (red,green,blue ranging from 0 to 255)
  // into the LCD controller's hardware specific format (here: BGR565).
uint16_t
LCD_RGBToNativeColor( uint32_t u32RGB )
{
	rgb_quad_t rgb;
	rgb.u32 = u32RGB;
	return ((uint16_t)(rgb.s.r & 0xF8) >> 3)  // red  : move bits 7..3 into b 4..0
	    | ((uint16_t)(rgb.s.g & 0xFC) << 3)  // green: move bits 7..2 into b 10..5
	    | ((uint16_t)(rgb.s.b & 0xF8) << 8); // blue : move bits 7..3 into b 15..11
}

  // Crude measure for the "similarity" of two colours:
  // Colours are split into R,G,B (range 0..255 each),
  // then absolute differences added.
  // Theoretic maximum 3*255, here slightly less (doesn't matter).
int
LCD_GetColorDifference( uint16_t color1, uint16_t color2 )
{
	rgb_quad_t rgb[2];
	int delta_r, delta_g, delta_b;

	rgb[0].u32 = LCD_NativeColorToRGB( color1 );
	rgb[1].u32 = LCD_NativeColorToRGB( color2 );
	delta_r = (int)rgb[0].s.r - (int)rgb[1].s.r;
	delta_g = (int)rgb[0].s.g - (int)rgb[1].s.g;
	delta_b = (int)rgb[0].s.b - (int)rgb[1].s.b;
	if( delta_r<0)
		delta_r = -delta_r;  // abs() may be a code-space-hogging macro..
	if( delta_g<0)
		delta_g = -delta_g;  // .. and there are already TOO MANY dependencies here
	if( delta_b<0)
		delta_b = -delta_b;
	return delta_r + delta_g + delta_b;
}

  // Returns either LCD_COLOR_BLACK or LCD_COLOR_WHITE,
  // whichever gives the best contrast for text output
  // using given background colour. First used in color_picker.c .
uint16_t
LCD_GetGoodContrastTextColor( uint16_t backgnd_color )
{
	rgb_quad_t rgb;
	rgb.u32 = LCD_NativeColorToRGB( backgnd_color );
	if( ((int)rgb.s.r + (int)rgb.s.g + (int)rgb.s.b/2 ) > 333 )
		return LCD_COLOR_BLACK;
	return LCD_COLOR_WHITE;
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
        uint16_t fg_color, // [in] foreground colour (BGR565)
        uint16_t bg_color, // [in] background colour (BGR565)
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
	for( x=font_width-1; x>=0; --x) {
		x_zoom = ( options & LCD_OPT_DOUBLE_WIDTH ) ? 2 : 1;
		while(x_zoom--) {
			// .. rotated by 90° and horizontally mirrored
			for( y=0; y<font_height; ++y) {
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
	p.FSMC_DataSetupTime 			= 2;
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
	LCD_WriteData(0xc8);
#else
	LCD_WriteData(0x08);
#endif
	LCD_WriteCommand(0xfe);
	LCD_WriteCommand(0xef);
	LCD_WriteCommand(LCD_CMD_INVCTR);
	LCD_WriteData(0x00);	// Line inversion in all modes
	LCD_WriteCommand(LCD_CMD_VCOM4L);
	LCD_WriteData(0x16);
	LCD_WriteCommand(0xfd);
	LCD_WriteData(0x40);
	LCD_WriteCommand(0xa4);
	LCD_WriteData(0x70);
	LCD_WriteCommand(0xe7);
	LCD_WriteData(0x94);
	LCD_WriteData(0x88);
	LCD_WriteCommand(0xea);
	LCD_WriteData(0x3a);
	LCD_WriteCommand(0xed);
	LCD_WriteData(0x11);
	LCD_WriteCommand(0xe4);
	LCD_WriteData(0xc5);
	LCD_WriteCommand(0xe2);
	LCD_WriteData(0x80);
	LCD_WriteCommand(0xa3);
	LCD_WriteData(0x12);
	LCD_WriteCommand(0xe3);
	LCD_WriteData(0x07);
	LCD_WriteCommand(0xe5);
	LCD_WriteData(0x10);
	LCD_WriteCommand(LCD_CMD_EXTCTRL);
	LCD_WriteData(0x00);
	LCD_WriteCommand(0xf1);
	LCD_WriteData(0x55);
	LCD_WriteCommand(0xf2);
	LCD_WriteData(0x05);
	LCD_WriteCommand(0xf3);
	LCD_WriteData(0x53);
	LCD_WriteCommand(0xf4);
	LCD_WriteData(0x00);
	LCD_WriteCommand(0xf5);
	LCD_WriteData(0x00);
	LCD_WriteCommand(0xf7);
	LCD_WriteData(0x27);
	LCD_WriteCommand(0xf8);
	LCD_WriteData(0x22);
	LCD_WriteCommand(0xf9);
	LCD_WriteData(0x77);	// 0xff in disassembly
	LCD_WriteCommand(0xfa);
	LCD_WriteData(0x35);
	LCD_WriteCommand(0xfb);
	LCD_WriteData(0x00);
	LCD_WriteCommand(LCD_CMD_PWCTR6);
	LCD_WriteData(0x00);
	LCD_WriteCommand(0xfe);
	LCD_WriteCommand(0xef);
	LCD_WriteCommand(0xe9);
	LCD_WriteData(0x00);
	vTaskDelay(20);
	LCD_WriteCommand(LCD_CMD_SLPOUT);
	vTaskDelay(130);
	LCD_WriteCommand(LCD_CMD_DISPON);
	LCD_WriteCommand(LCD_CMD_RAMWR);
	LCD_ReleasePort();
}

/* EOF < md380tools/applet/src/lcd_driver.c > */
