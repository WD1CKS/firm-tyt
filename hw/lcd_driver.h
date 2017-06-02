#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

// File:    md380tools/applet/src/lcd_driver.h
// Author:  Wolf (DL4YHF) [initial version]
// Date:    2017-04-14
//  API for a simple LCD driver for Retevis RT3 / Tytera MD380 / etc .
//  Details in the implementation;
//

// Defines (macro constants, plain old "C"..)

#define LCD_SCREEN_WIDTH  160
#define LCD_SCREEN_HEIGHT 128

// Taken from HX8353-E datasheet, actual chip in MD-380 is HX8302-A
#define LCD_CMD_NOP		0x00	// No Operation
#define LCD_CMD_SWRESET		0x01	// Software reset
#define LCD_CMD_RDDIDIF		0x04	// Read Display ID Info
#define LCD_CMD_RDDST		0x09	// Read Display Status
#define LCD_CMD_RDDPM		0x0a	// Read Display Power
#define LCD_CMD_RDD_MADCTL	0x0b	// Read Display
#define LCD_CMD_RDD_COLMOD	0x0c	// Read Display Pixel
#define LCD_CMD_RDDDIM		0x0d	// Read Display Image
#define LCD_CMD_RDDSM		0x0e	// Read Display Signal
#define LCD_CMD_RDDSDR		0x0f	// Read display self-diagnostic resut
#define LCD_CMD_SLPIN		0x10	// Sleep in & booster off
#define LCD_CMD_SLPOUT		0x11	// Sleep out & booster on
#define LCD_CMD_PTLON		0x12	// Partial mode on
#define LCD_CMD_NORON		0x13	// Partial off (Normal)
#define LCD_CMD_INVOFF		0x20	// Display inversion off
#define LCD_CMD_INVON		0x21	// Display inversion on
#define LCD_CMD_GAMSET		0x26	// Gamma curve select
#define LCD_CMD_DISPOFF		0x28	// Display off
#define LCD_CMD_DISPON		0x29	// Display on
#define LCD_CMD_CASET		0x2a	// Column address set
#define LCD_CMD_RASET		0x2b	// Row address set
#define LCD_CMD_RAMWR		0x2c	// Memory write
#define LCD_CMD_RGBSET		0x2d	// LUT parameter (16-to-18 color mapping)
#define LCD_CMD_RAMRD		0x2e	// Memory read
#define LCD_CMD_PTLAR		0x30	// Partial start/end address set
#define LCD_CMD_VSCRDEF		0x33	// Vertical Scrolling Direction
#define LCD_CMD_TEOFF		0x34	// Tearing effect line off
#define LCD_CMD_TEON		0x35	// Tearing effect mode set & on
#define LCD_CMD_MADCTL		0x36	// Memory data access control
#define LCD_CMD_VSCRSADD	0x37	// Vertical scrolling start address
#define LCD_CMD_IDMOFF		0x38	// Idle mode off
#define LCD_CMD_IDMON		0x39	// Idle mode on
#define LCD_CMD_COLMOD		0x3a	// Interface pixel format
#define LCD_CMD_RDID1		0xda	// Read ID1
#define LCD_CMD_RDID2		0xdb	// Read ID2
#define LCD_CMD_RDID3		0xdc	// Read ID3

// Extended command set
#define LCD_CMD_SETOSC		0xb0	// Set internal oscillator
#define LCD_CMD_SETPWCTR	0xb1	// Set power control
#define LCD_CMD_SETDISPLAY	0xb2	// Set display control
#define LCD_CMD_SETCYC		0xb4	// Set dispaly cycle
#define LCD_CMD_SETBGP		0xb5	// Set BGP voltage
#define LCD_CMD_SETVCOM		0xb6	// Set VCOM voltage
#define LCD_CMD_SETEXTC		0xb9	// Enter extension command
#define LCD_CMD_SETOTP		0xbb	// Set OTP
#define LCD_CMD_SETSTBA		0xc0	// Set Source option
#define LCD_CMD_SETID		0xc3	// Set ID
#define LCD_CMD_SETPANEL	0xcc	// Set Panel characteristics
#define LCD_CMD_GETHID		0xd0	// Read Himax internal ID
#define LCD_CMD_SETGAMMA	0xe0	// Set Gamma
#define LCD_CMD_SET_SPI_RDEN	0xfe	// Set SPI Read address (and enable)
#define LCD_CMD_GET_SPI_RDEN	0xff	// Get FE A[7:0] parameter

// Colour values for the internally used 16-bit "RGB565" format :
// RED component in bits 15..11, GREEN in bits 10..5, BLUE in bits 4..0 :
#define LCD_COLORBIT0_RED   11
#define LCD_COLORBIT0_GREEN 5
#define LCD_COLORBIT0_BLUE  0
#define LCD_COLOR_WHITE 0xFFFF
#define LCD_COLOR_BLACK 0x0000
#define LCD_COLOR_RED  0xF800
#define LCD_COLOR_GREEN 0x07E0
#define LCD_COLOR_BLUE   0x001F
#define LCD_COLOR_YELLOW (LCD_COLOR_RED|LCD_COLOR_GREEN)
#define LCD_COLOR_CYAN   (LCD_COLOR_BLUE|LCD_COLOR_GREEN)
#define LCD_COLOR_PURPLE (LCD_COLOR_RED|LCD_COLOR_BLUE)
#define LCD_COLOR_MD380_BKGND_BLUE 0x1C1F // RGB565-equivalent of Tytera's blue background for the main screen


// Bitwise combineable 'options' for some text drawing functions:
#define LCD_OPT_NORMAL_OUTPUT 0x00 // "nothing special" (use default font, not magnified)
#define LCD_OPT_DOUBLE_WIDTH  0x02 // double-width character output
#define LCD_OPT_DOUBLE_HEIGHT 0x04 // double-height character output
#define LCD_OPT_RESERVED_FONT 0x08 // reserved for a future font, possibly "proportional"
#define LCD_OPT_FONT_8x16  (LCD_OPT_DOUBLE_HEIGHT)
#define LCD_OPT_FONT_16x16 (LCD_OPT_DOUBLE_WIDTH|LCD_OPT_DOUBLE_HEIGHT)

//---------------------------------------------------------------------------
// Structures for the 'mid level' LCD drawing functions.
//   Keeping all in a small struct reduces the overhead
//   to pass two colours and a coordinate to LCD_Printf()...
//---------------------------------------------------------------------------

typedef union tLcdColour
{
	uint16_t RGB565;
	struct tLcdPackedColour {
		uint16_t	blue : 5;
		uint16_t	green : 6;
		uint16_t	red : 5;
	} packed;
}  __attribute__((packed)) lcd_colour_t;

typedef struct tLcdContext
{
  int x,y;  // graphic output coord, updated after printing each character .
  uint16_t fg_color, bg_color; // foreground and background colour
  int font; // current font, zoom, and character output options
            // (bitwise combineable, LCD_OPT_FONT_...)
  int x1, y1, x2, y2; // simple clipping and margins for 'printing'.
  // The above range is set for 'full screen' in LCD_InitContext.
} lcd_context_t;


//---------------------------------------------------------------------------
// Global vars - use a few as possible, there's not much RAM to waste !
// The LOW-level LCD functions also do NOT use fancy structs, objects,
//     "graphic contexts", "handles", to keep the footprint low.
// Everything a function needs to know is passed in through the argument list,
//     i.e. only occupies a few bytes on the stack.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Prototypes for LOW-LEVEL LCD driver functions
//---------------------------------------------------------------------------

void LimitInteger( int *piValue, int min, int max);
void LCD_SetPixelAt( int x, int y, uint16_t wColor ); // inefficient.. avoid if possible

void LCD_FillRect( // Draws a frame-less, solid, filled rectangle
        int x1, int y1,  // [in] pixel coordinate of upper left corner
        int x2, int y2,  // [in] pixel coordinate of lower right corner
        uint16_t wColor); // [in] filling colour (RGB565)
void LCD_HorzLine( int x1, int y, int x2, uint16_t wColor );

void LCD_FastColourGradient(void); // Fills the framebuffer with a
  // 2D color gradient. Used for testing .. details in lcd_driver.c .

uint8_t *LCD_GetFontPixelPtr_8x8( uint8_t c);
  // Retrieves the address of a character's font bitmap, 8 * 8 pixels .
  // Unlike the fonts in Tytera's firmware, the 8*8-pixel font
  // supports all 256 fonts from the ancient 'codepage 437',
  // and can thus be used to draw tables, boxes, etc, as in the old days.

//---------------------------------------------------------------------------
// Prototypes for MID-LEVEL LCD driver functions (text output, etc)
//---------------------------------------------------------------------------

int LCD_GetFontHeight(int font_options );
int LCD_GetCharWidth( int font_options, char c );
int LCD_GetTextWidth( int font_options, const char *pszText );

int LCD_DrawCharAt( // lowest level of 'text output' into the framebuffer
        char c,            // [in] character code (ASCII)
        int x, int y,      // [in] pixel coordinate
        uint16_t fg_color, // [in] foreground colour (BGR565)
        uint16_t bg_color, // [in] background colour (BGR565)
        int options );     // [in] LCD_OPT_... (bitwise combined)
  // Returns the graphic coordinate (x) to print the next character .

void LCD_InitContext( lcd_context_t *pContext );
  // Clears the struct and sets the output clipping window to 'full screen'.

int LCD_DrawString( lcd_context_t *pContext, const char *cp );
  // Draws a zero-terminated ASCII string.
  // Returns the graphic coordinate (x) to print the next character .

int LCD_Printf( lcd_context_t *pContext, const char *fmt, ... );
  // Almost the same as LCD_DrawString,
  // but with all goodies supported by tinyprintf .

void LCD_DrawRGB(uint16_t *rgb, int x, int y, int w, int h);
void LCD_DrawRGBTransparent(uint16_t *rgb, int x, int y, int w, int h, int t);
void LCD_DrawCircle(int x, int y, int r, uint16_t c, bool f);
void LCD_DrawRectangle(int x, int y, int w, int h, uint16_t c, bool f);
void LCD_DrawLine(int x, int y, int xx, int yy, uint16_t c);

void LCD_Init(void);
void LCD_EnablePort(void);
void LCD_ReleasePort(void);
extern SemaphoreHandle_t LCD_Mutex;
extern enum LCD_Enabled {
	LCD_NOTYET,
	LCD_ENABLED,
	LCD_KEYPAD
} LCD_Enabled;

/* EOF < md380tools/applet/src/lcd_driver.h > */

#endif
