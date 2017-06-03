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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>       // memset(), ...

#include "gpio.h"
#include "lcd_driver.h"   // constants + API prototypes for the *alternative* LCD driver (no "gfx")
#include "task.h"
#include "stm32f4xx.h"
#include "stm32f4xx_fsmc.h"
#include "stm32f4xx_rcc.h"

extern const uint8_t font_8_8[256*8]; // extra font with 256 characters from 'codepage 437'

/*
 * Low-level LCD driver
 *
 * Based on a driver written by DL4YHF for a display with ST7735,
 *   which seems to be quite compatible with the MD380 / RT3,
 *   except for the 8-bit bus interface ("FSMC") and a few registers
 *   in the LCD controller initialized by the original firmware .
 *
 * Note: Do NOT erase the screen (or portions of it) before drawing
 *   into the framebuffer. That would cause an annoying flicker,
 *   because this LCD controller doesn't support double buffering.
 *   In other words, we always "paint" directly into the CURRENTLY
 *   VISIBLE image - and painting isn't spectacularly fast !
 */

#define LCD_WriteCommand(cmd)	*(volatile uint8_t*)0x60000000 = cmd
#define LCD_WriteData(dta)	*(volatile uint8_t*)0x60040000 = dta
#define LCD_WritePixel(clr)				\
	do {						\
		LCD_WriteData(((clr) >> 8) & 0xff);	\
		LCD_WriteData((clr) & 0xff);		\
	} while(0)

static void
LCD_WritePixels( uint16_t wColor, uint16_t nRepeats )
{
	while( nRepeats-- )
		LCD_WritePixel(wColor);
}

static void
LimitUInt8( uint8_t *piValue, uint8_t min, uint8_t max)
{
	uint8_t value;

	value = *piValue;
	if( value < min ) {
		value = min;
	}
	if( value > max ) {
		value = max;
	}
	*piValue = value;
}


/*
 * Sets the coordinates before writing a rectangular area of pixels.
 * Returns the NUMBER OF PIXELS which must be sent to the controller
 * after setting the rectangle.
 *
 * Request LCD_EnablePort() to have been called.
 */
static uint16_t
LCD_SetOutputRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	// Crude clipping, not bullet-proof but better than nothing:
	LimitUInt8(&x1, 0, LCD_SCREEN_WIDTH-1);
	LimitUInt8(&x2, 0, LCD_SCREEN_WIDTH-1);
	LimitUInt8(&y1, 0, LCD_SCREEN_HEIGHT-1);
	LimitUInt8(&y2, 0, LCD_SCREEN_HEIGHT-1);

	if(x1>x2 || y1>y2)
		return 0;

	LCD_WriteCommand(LCD_CMD_CASET);
	LCD_WriteData(x1);
	LCD_WriteData(x1);
	LCD_WriteData(x2);
	LCD_WriteData(x2);

	LCD_WriteCommand(LCD_CMD_RASET);
	LCD_WriteData(y1);
	LCD_WriteData(y1);
	LCD_WriteData(y2);
	LCD_WriteData(y2);

	LCD_WriteCommand(LCD_CMD_RAMWR);

	return (1+x2-x1) * (1+y2-y1);
}

/*
 * Inefficient method to draw anything (except maybe 'thin BRESENHAM lines').
 * Similar to Tytera's 'gfx_write_pixel_to_framebuffer' @0x08033728 in D13.020 .
 * When not disturbed by interrupts, it took 40 ms to fill 160*128 pixels .
 */
void
LCD_SetPixelAt(uint8_t x, uint8_t y, uint16_t wColor)
{
	LCD_EnablePort();
	LCD_WriteCommand(LCD_CMD_CASET);
	/*
	 * For unknown reasons, we need to write the same value twice
	 * rather than a 16-bit value.  In a 128x160 screen, the coordinates
	 * will always fit in a uint8_t, so a single write would make sense,
	 * but it seems that it needs to be each value twice.
	 */
	LCD_WriteData(x); // 2nd CASET param: "XS7 ..XS0" ("X-start", lo)
	LCD_WriteData(x); // 2nd CASET param: "XS7 ..XS0" ("X-start", lo)
	/* It seems we don't need to set an end though. */

	LCD_WriteCommand(LCD_CMD_RASET);
	LCD_WriteData(y); // 1st RASET param: "YS15..YS8" ("Y-start", hi)
	LCD_WriteData(y); // 1st RASET param: "YS15..YS8" ("Y-start", hi)
	/* It seems we don't need to set an end though. */

	LCD_WriteCommand(LCD_CMD_RAMWR);
	LCD_WritePixels(wColor, 1);
	LCD_ReleasePort();
}

/*
 * Draws a frame-less, solid, filled rectangle, much like LCD_DrawRectangle()
 * used to implement lines and filled rects.  Not exported though since we
 * don't need two functions that do the same thing.
 *
 * This one takes the coordinates of the top-left and bottom-right corners rather
 * than the top-left and size.
 */
static void
LCD_FillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t wColor)
{
	uint16_t nPixels;

	LCD_EnablePort();
	nPixels = LCD_SetOutputRect(x1, y1, x2, y2);  // send rectangle coordinates only ONCE

	if (nPixels<=0) {
		// something wrong with the coordinates
		LCD_ReleasePort();
		return;
	}

	LCD_WritePixels(wColor, nPixels);
	LCD_ReleasePort();
}

/*
 * Draws a thin horizontal line...
 * Much faster than Bresenham, so used when possible.
 *
 * Again though, no need to export it.
 */
static void
LCD_HorzLine(uint8_t x1, uint8_t y, uint8_t x2, uint16_t wColor)
{
	LCD_FillRect(x1, y, x2, y, wColor); // .. just a camouflaged 'fill rectangle'
}

/*
 * Draws a thin vertical line...
 * Much faster than Bresenham, so used when possible.
 *
 * Again though, no need to export it.
 */
static void
LCD_VertLine(uint8_t x, uint8_t y, uint8_t y2, uint16_t wColor)
{
	LCD_FillRect(x, y, x, y2, wColor); // .. just a camouflaged 'fill rectangle'
}

/*
 * Fills the framebuffer with a 2D colour gradient for testing.
 *       ,----------------, - y=0
 *       |Green ...... Red|
 *       | .            . |
 *       | .            . |
 *       |Cyan Blue Violet|
 *       '----------------' - y=127
 *       |                |
 *      x=0              x=159
 */
void
LCD_FastColourGradient(void)
{
	lcd_colour_t px;
	uint8_t x, y;

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

/*
 * Draws an RGB image in the screen given the image data, top-left
 * position, width, and height.
 */
void
LCD_DrawRGB(uint16_t *rgb, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	uint16_t i;
	uint8_t xx, yy;

	LCD_EnablePort();
	if (LCD_SetOutputRect(x, y, x + w - 1, y + h - 1) <= 0) {
		LCD_ReleasePort();
		return;
	}
	for (i = 0, yy = 0; yy < h; ++yy) {
		for (xx = 0; xx < w; xx++, i++)
			LCD_WritePixel(rgb[i]);
	}
	LCD_ReleasePort();
}

/*
 * Draws an RGB image as LCD_DrawRGB, but with a transparent colour specified.
 * If a pixel is of the transparent colour, it is not drawn, and the screen at
 * that position remains unchanged.
 */
void
LCD_DrawRGBTransparent(uint16_t *rgb, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t t)
{
	uint16_t i;
	uint8_t xx, yy;
	enum rect_state {
		RECTANGLE_NONE,
		RECTANGLE_LINE,
		RECTANGLE_FULL
	} rect;

	LCD_EnablePort();
	if (rgb[0] != t) {
		/* If the first pixel is not transparent, set up the full rectangle */
		if (LCD_SetOutputRect(x, y, x + w - 1, y + h - 1) <= 0) {
			LCD_ReleasePort();
			return;
		}
		rect = RECTANGLE_FULL;
	}
	else {
		rect = RECTANGLE_NONE;
	}
	for (yy = y, i = 0; yy < y + h && yy < LCD_SCREEN_HEIGHT; ++yy) {
		for (xx = x; xx < x + w && xx < LCD_SCREEN_WIDTH; ++xx, ++i) {
			if (rgb[i] == t) {
				/*
				 * When we hit a transparent pixel, cancel any existing
				 * drawing rectangle.
				 */
				if (rect != RECTANGLE_NONE) {
					LCD_WriteCommand(LCD_CMD_NOP);
					rect = RECTANGLE_NONE;
				}
			}
			else {
				/*
				 * If we have a pixel to draww, but no rectangle, set one up.
				 */
				if (rect == RECTANGLE_NONE) {
					if (xx == x) {
						/*
						 * If the first column has a non-transparent pixel, set up
						 * the full drawing rectangle
						 */
						if (LCD_SetOutputRect(xx, yy, x + w - 1, y + h - 1) <= 0) {
							LCD_ReleasePort();
							return;
						}
						rect = RECTANGLE_FULL;
					}
					else {
						/*
						 * Otherwise, only set up the rest of the row for output.
						 */
						if (LCD_SetOutputRect(xx, yy, x + w - 1, yy) <= 0) {
							LCD_ReleasePort();
							return;
						}
						rect = RECTANGLE_LINE;
					}
				}
				LCD_WritePixel(rgb[i]);
			}
		}
		/* If we finish a line rectangle, it is completed. */
		if (rect == RECTANGLE_LINE)
			rect = RECTANGLE_NONE;
	}
	LCD_ReleasePort();
}

/*
 * Draws a circly centred on x/y with radius of r using colour c
 * if f is true, the circle is solid.
 */
void
LCD_DrawCircle(uint8_t x, uint8_t y, uint8_t r, uint16_t c, bool f)
{
	int D = 3 - (2 * r);
	uint8_t X = 0;
	uint8_t Y = r;

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

/*
 * Draws a rectangle with top-left position of x/y using the specified
 * width and height of the specified colour.  If f is true, the rectangle
 * is solid.
 */
void
LCD_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t c, bool f)
{
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

/*
 * Draws a line from x/y to xx/yy in colour c.
 */
void
LCD_DrawLine(uint8_t x, uint8_t y, uint8_t xx, uint8_t yy, uint16_t c)
{
	int16_t sx;
	int16_t sy;
	int16_t err;
	int16_t e2;
	int16_t dx;
	int16_t dy;

	sx = x < xx ? 1 : -1;
	sy = y < yy ? 1 : -1;
	dx = xx - x;
	dy = yy - y;
	err = dx - dy;

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

/*
 * A couple functions to apply font options
 */
uint8_t
LCD_GetCharHeight(uint32_t font_options)
{
	int font_height = 8;

	if( font_options & LCD_OPT_DOUBLE_HEIGHT ) {
		font_height *= 2;
	}

	return font_height;
}

uint8_t
LCD_GetCharWidth(uint32_t font_options)
{
	int width = 8;

	if( font_options & LCD_OPT_DOUBLE_WIDTH ) {
		width *= 2;
	}

	return width;
}

/*
 * Draws character c at position x/y using the specified fg/bg colours and
 * the specified font options.
 *
 * only redraw the screen when necessary because the QRM from the display
 * connector cable was still audible in an SSB receiver.
 */
uint8_t
LCD_DrawCharAt(const char c, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color, uint32_t options)
{
	const uint8_t *cp;		// Pointer to start of character data
	uint8_t x_zoom, y_zoom;		// Multiplier x/y sizes
	uint8_t x2, y2;			// Rectangle end points
	uint8_t font_width, font_height;	// Font data size (not output size)
	uint8_t i;

	x_zoom = (options & LCD_OPT_DOUBLE_WIDTH) ? 2 : 1;
	y_zoom = (options & LCD_OPT_DOUBLE_HEIGHT) ? 2 : 1;
	font_width = font_height = 8;

	if (x >= LCD_SCREEN_WIDTH || y >= LCD_SCREEN_HEIGHT)
		return x;

	cp = &font_8_8[font_height * c];
	x2 = x + x_zoom * font_width - 1;
	y2 = y + y_zoom * font_height - 1;

	if(x2 >= LCD_SCREEN_WIDTH) {
		/* Clip now to avoid clipping in the loop. */
		font_width = (1 + (LCD_SCREEN_WIDTH - 1) - x) / x_zoom;
		/* Recalculate x2 in case it's double-width to avoid half-pixels */
		x2 = x + x_zoom * font_width - 1;
	}

	if(y2 >= LCD_SCREEN_HEIGHT) {
		/* Clip now to avoid clipping in the loop. */
		font_height = (1 + (LCD_SCREEN_WIDTH - 1) - y) / y_zoom;
		/* Recalculate y2 in case it's double-width to avoid half-pixels */
		y2 = y + y_zoom*font_height-1;
	}

	if (font_width == 0 || font_height == 0)
		return x;

	LCD_EnablePort();
	if( LCD_SetOutputRect( x, y, x2, y2 ) <= 0 ) {
		// something wrong with the graphic coordinates
		LCD_ReleasePort();
		return x;
	}

	for (y=0; y<font_height; y++) {
		for (i=0; i<y_zoom; i++) {
			for (x=0; x<font_width; x++) {
				if(cp[y] & (0x80>>x) ) {
					LCD_WritePixels( fg_color, x_zoom );
				}
				else {
					LCD_WritePixels( bg_color, x_zoom );
				}
			}
		}
	}
	LCD_ReleasePort();

	// pixel coord for printing the NEXT character
	return x2+1;
}

/*
 * Clears the context and sets the output clipping window to 'full screen'.
 */
void
LCD_InitContext( lcd_context_t *pContext )
{
	memset( pContext, 0, sizeof( lcd_context_t ) );
	pContext->x2 = LCD_SCREEN_WIDTH-1;
	pContext->y2 = LCD_SCREEN_HEIGHT-1;
}

/*
 * Draws a zero-terminated ASCII string. Should be simple but versatile.
 *  [in]  pContext, especially pContext->x,y = graphic output cursor .
 *        pContext->x1,x1,x2,y2 = clipping area (planned) .
 *  [out] pContext->x,y = graphic coordinate for the NEXT output .
 *        Return value : horizontal position for the next character (x).
 * For multi-line output (with '\r' or '\n' in the string),
 *     the NEXT line for printing is stored in pContext->y .
 * ASCII control characters with special functions :
 *   \n (new line) : wrap into the next line, without clearing the rest
 *   \r (carriage return) : similar, but if the text before the '\r'
 *                   didn't fill a line on the screen, the rest will
 *                   be filled with the background colour
 *      (can eliminate the need for "clear screen" before printing, etc)
 *   \t (horizontal tab) : doesn't print a 'tab' but horizontally
 *                   CENTERS the remaining text in the current line
 */
uint8_t
LCD_DrawString(lcd_context_t *pContext, const char *cp)
{
	const char *cp2;
	int w;
	uint8_t fh;
	uint8_t fw;

	fh = LCD_GetCharHeight(pContext->font);
	fw = LCD_GetCharWidth(pContext->font);
	for (; *cp; cp++) {
		switch(*cp) {
		case '\r':
			/*
			 * carriage return : almost the same as 'new line', but..
			 * as a service for flicker-free output, CLEARS ALL UNTIL THE END
			 * OF THE CURRENT LINE, so clearing the screen is unnecessary.
			 */
			LCD_FillRect( pContext->x, pContext->y, pContext->x2, pContext->y + fh - 1, pContext->bg_color );
			/* Fall-through */
		case '\n':
			pContext->x = pContext->x1;
			pContext->y += fh;
			break;
		case '\t':  // horizontally CENTER the text in the rest of the line
			for (cp2 = cp+1; *cp2; cp2++) {
				if (*cp2 == '\t' || *cp2 == '\n' || *cp2 == '\r')
					break;
			}
			w = ((cp2 - cp - 1) * fw);
			w = (pContext->x2 - pContext->x - w) / 2; // "-> half remaining width"
			if(w > 0) {
				LCD_FillRect(pContext->x, pContext->y,
				    pContext->x + w - 1, pContext->y + fh - 1, pContext->bg_color);
				pContext->x += w;
			}
			break;
		default   :  // anything should be 'printable' :
			pContext->x = LCD_DrawCharAt( *cp, pContext->x, pContext->y,
			    pContext->fg_color, pContext->bg_color, pContext->font );
			break;
		}
	}
	return pContext->x;
}

/*
 * printf() wrapper around LCD_DrawString()
 */
uint8_t
LCD_Printf(lcd_context_t *pContext, const char *fmt, ...)
{
	char *s = NULL;
	uint8_t rc;

	va_list va;
	va_start(va, fmt);
	if (vasprintf(&s, fmt, va ) == -1 || s == NULL) {
		va_end(va);
		return pContext->x;
	}
	va_end(va);
	rc = LCD_DrawString(pContext, s);
	free(s);
	return rc;
}

/**************************************************************************/
/*!
    Display Driver Lowest Layer Settings.
*/
/**************************************************************************/
static void
FSMC_Conf(void)
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
}

void
LCD_ReleasePort(void)
{
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
