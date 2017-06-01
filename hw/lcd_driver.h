#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

// File:    md380tools/applet/src/lcd_driver.h
// Author:  Wolf (DL4YHF) [initial version]
// Date:    2017-04-14
//  API for a simple LCD driver for ST7735-compatible controllers,
//             tailored for Retevis RT3 / Tytera MD380 / etc .
//  Details in the implementation; md380tools/applet/src/lcd_driver.h .
//

// Defines (macro constants, plain old "C"..)

#define LCD_SCREEN_WIDTH  160
#define LCD_SCREEN_HEIGHT 128

#define LCD_FONT_WIDTH  6  // width  of a character cell in the 'small', fixed font
#define LCD_FONT_HEIGHT 12 // height of a character cell in the 'small', fixed font

// ST7735 System Function Commands
#define LCD_CMD_NOP		0x00	// No Operation
#define LCD_CMD_SWRESET		0x01	// Software reset
#define LCD_CMD_RDDID		0x04	// Read Display ID
#define LCD_CMD_RDDST		0x09	// Read Display Status
#define LCD_CMD_RDDPM		0x0a	// Read Display Power
#define LCD_CMD_RDD_MADCTL	0x0b	// Read Display
#define LCD_CMD_RDD_COLMOD	0x0c	// Read Display Pixel
#define LCD_CMD_RDDDIM		0x0d	// Read Display Image
#define LCD_CMD_RDDSM		0x0e	// Read Display Signal
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
#define LCD_CMD_RAMRD		0x2e	// Memory read
#define LCD_CMD_PTLAR		0x30	// Partial start/end address set
#define LCD_CMD_TEOFF		0x34	// Tearing effect line off
#define LCD_CMD_TEON		0x35	// Tearing effect mode set & on
#define LCD_CMD_MADCTL		0x36	// Memory data access control
#define LCD_CMD_IDMOFF		0x38	// Idle mode off
#define LCD_CMD_IDMON		0x39	// Idle mode on
#define LCD_CMD_COLMOD		0x3a	// Interface pixel format
#define LCD_CMD_RDID1		0xda	// Read ID1
#define LCD_CMD_RDID2		0xdb	// Read ID2
#define LCD_CMD_RDID3		0xdc	// Read ID3

// Panel Function Commands
#define LCD_CMD_FRMCTR1		0xb1	// In normal mode
#define LCD_CMD_FRMCTR2		0xb2	// In idle mode
#define LCD_CMD_FRMCTR3		0xb3	// In partial mode + Full colors
#define LCD_CMD_INVCTR		0xb4	// Display inversion control
#define LCD_CMD_DISSET5		0xb6	// Display function setting
#define LCD_CMD_PWCTR1		0xc0	// Power control setting
#define LCD_CMD_PWCTR2		0xc1	// Power control setting
#define LCD_CMD_PWCTR3		0xc2	// In normal mode (Full colors)
#define LCD_CMD_PWCTR4		0xc3	// In Idle mode (8-colors)
#define LCD_CMD_PWCTR5		0xc4	// In partial mode + Full colors
#define LCD_CMD_VMCTR1		0xc5	// VCOM control 1
#define LCD_CMD_VMOFCTR		0xc7	// Set VCOM offset control
#define LCD_CMD_WRID2		0xd1	// Set LCM version code
#define LCD_CMD_WRID3		0xd2	// Customer Project code
#define LCD_CMD_PWCTR6		0xfc	// In partial mode + Idle
#define LCD_CMD_NVCTR1		0xd9	// EEPROM control status
#define LCD_CMD_NVCTR2		0xde	// EEPROM Read Command
#define LCD_CMD_NVCTR3		0xdf	// EEPROM Write Command
#define LCD_CMD_GAMCTRP1	0xe0	// Set Gamma adjustment +
#define LCD_CMD_GAMCTRN1	0xe1	// Set Gamma adjustment -
#define LCD_CMD_EXTCTRL		0xf0	// Extension Command Control
#define LCD_CMD_VCOM4L		0xff	// Vcom 4 Level control

// Colour values for the internally used 16-bit "BGR565" format :
// BLUE component in bits 15..11, GREEN in bits 10..5, RED in bits 4..0 :
#define LCD_COLORBIT0_RED   0
#define LCD_COLORBIT0_GREEN 5
#define LCD_COLORBIT0_BLUE  11
#define LCD_COLOR_WHITE 0xFFFF
#define LCD_COLOR_BLACK 0x0000
#define LCD_COLOR_BLUE  0xF800
#define LCD_COLOR_GREEN 0x07E0
#define LCD_COLOR_RED   0x001F
#define LCD_COLOR_YELLOW (LCD_COLOR_RED|LCD_COLOR_GREEN)
#define LCD_COLOR_CYAN   (LCD_COLOR_BLUE|LCD_COLOR_GREEN)
#define LCD_COLOR_PURPLE (LCD_COLOR_RED|LCD_COLOR_BLUE)
#define LCD_COLOR_MD380_BKGND_BLUE 0xFC03 // BGR565-equivalent of Tytera's blue background for the main screen


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

typedef struct tLcdContext
{
  int x,y;  // graphic output coord, updated after printing each character .
  uint16_t fg_color, bg_color; // foreground and background colour
  int font; // current font, zoom, and character output options
            // (bitwise combineable, LCD_OPT_FONT_...)
  int x1, y1, x2, y2; // simple clipping and margins for 'printing'.
  // The above range is set for 'full screen' in LCD_InitContext.
} lcd_context_t;


  // Structure of the 32-bit RGB colour type (3 * 8 colour bits),
  // used by LCD_NativeColorToRGB() + LCD_RGBToNativeColor() :
typedef union T_RGB_Quad
{ uint32_t u32;  // "all in one DWORD", compatible with 6-digit "hex codes"
  struct
   { uint8_t b; // blue component, 0..255
     // (least significant byte for "hex-code"-compatibility on little endian system.
     //  rgb_quad_t.u32 = 0xFF0000 shall be the same as "#ff0000" = PURE RED, not BLUE)
     uint8_t g; // green component, 0..255
     uint8_t r; // red component, 0..255
     uint8_t a; // alignment dummy (no "alpha" channel here)
   } s;
  uint8_t ba[4]; // the same as a 4-byte array (for processing in loops)
          // Beware, here: ba[0] = least significant byte = BLUE component !
} rgb_quad_t;




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
        uint16_t wColor); // [in] filling colour (BGR565)
void LCD_HorzLine( int x1, int y, int x2, uint16_t wColor );

void LCD_ColorGradientTest(void); // Fills the framebuffer with a
  // 2D color gradient. Used for testing .. details in lcd_driver.c .
void LCD_FastColourGradient(void);

uint8_t *LCD_GetFontPixelPtr_8x8( uint8_t c);
  // Retrieves the address of a character's font bitmap, 8 * 8 pixels .
  // Unlike the fonts in Tytera's firmware, the 8*8-pixel font
  // supports all 256 fonts from the ancient 'codepage 437',
  // and can thus be used to draw tables, boxes, etc, as in the old days.

uint32_t LCD_NativeColorToRGB( uint16_t native_colour );
uint16_t LCD_RGBToNativeColor( uint32_t u32RGB );

  // Crude measure for the "similarity" of two colour values.
  // First used in app_menu.c to find out if two colours are
  // "different enough" to be used as back- and foreground colours.
int LCD_GetColorDifference( uint16_t color1, uint16_t color2 );
uint16_t LCD_GetGoodContrastTextColor( uint16_t backgnd_color );

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

void LCD_DrawBGR(uint16_t *bgr, int x, int y, int w, int h);
void LCD_DrawBGRTransparent(uint16_t *bgr, int x, int y, int w, int h, int t);

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
