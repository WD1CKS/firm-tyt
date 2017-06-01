#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "led.h"
#include "usb_cdc.h"
#include "controls.h"
#include "gpio.h"
#include "lcd_driver.h"
#include "images/wlarc.h"

#ifdef CODEPLUGS
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#endif

static void sleep(void);
static void output_main(void*);
static void red_main(void*);
#ifdef CODEPLUGS
static void lua_main(void *args);
#endif
static SemaphoreHandle_t red_monitor;
static int  red_state;
lcd_context_t lcd;

int
main (void)
{
	red_monitor = xSemaphoreCreateMutex();
	xTaskCreate(output_main, "out", 2*1024, NULL, 1, NULL);
	xTaskCreate(red_main, "red", 256, NULL, 0, NULL);
#ifdef CODEPLUGS
	xTaskCreate(lua_main, "lua", 32*1024, NULL, 0, NULL);
#endif
	vTaskStartScheduler();
	for(;;)
		vTaskDelay(1000);
}

static void
set_red_state(int i)
{
	xSemaphoreTake(red_monitor, portMAX_DELAY);
	red_state = i;
	xSemaphoreGive(red_monitor);
}

static int
get_red_state()
{
	int i;
	xSemaphoreTake(red_monitor, portMAX_DELAY);
	i = red_state;
	xSemaphoreGive(red_monitor);
	return i;
}

static void
led_set(int red, int green)
{
	static int last_state = -1;
	static char enc[] = "encoder: 00, ";
	char kp[14];
	int ev;
	int state;
	char key;
	int vol;

	red_led(red);
	green_led(green);

	ev = Encoder_Read();
	state = ev<<2 | (red?2:0) | (green?1:0);
	if (state == last_state)
		return;
	last_state = state;
	enc[9] = ev / 10 + 48;
	enc[10] = ev % 10 + 48;
	usb_cdc_write(enc, 13);
	lcd.x = lcd.y = 0;
	LCD_DrawString(&lcd, enc);
	lcd.y = 8;
	const char red_on[] = "red on,  ";
	const char red_off[] = "red off, ";
	const char green_on[] = "green on\n";
	const char green_off[] = "green off\n";
        #define SEND_STR(s) { \
                usb_cdc_write((void*)(s), strlen(s)); \
                lcd.x = 0; \
                LCD_DrawString(&lcd, s); \
                lcd.y += 8; \
        }
	SEND_STR(red?red_on:red_off);
	SEND_STR(green?green_on:green_off);
	#undef SEND_STR
	vol = VOL_Read();
	lcd.x = 0;
	lcd.y += 8;
	LCD_Printf(&lcd, "Vol: %d   \n", vol);
	key = get_key();
	if (key) {
		if (key == '~')
			pin_toggle(pin_lcd_bl);
		if (key == 'M')
			LCD_DrawBGRTransparent(wlarc_logo, 0, 0, 160, 128, 65535);
		sprintf(kp, "%d (%c)\n", key, isprint(key)?key:'.');
		usb_cdc_write(kp, 11);
		if (key == 'P') {	// Power off
			// lcd.x=0;
			// lcd.y=56;
			// lcd.font = LCD_OPT_DOUBLE_WIDTH | LCD_OPT_DOUBLE_HEIGHT;
			// LCD_DrawString(&lcd, "\tFucker!");
			LCD_DrawBGR(wlarc_logo, 0, 0, 160, 128);
			lcd.font = 0;
			vTaskDelay(1500);
			Normal_Power();
		}
	}
}

static void output_main(void* machtnichts __attribute__((unused))) {
	led_setup();
        LCD_Init();
        LCD_InitContext(&lcd);
        lcd.fg_color = LCD_COLOR_BLACK;
        lcd.bg_color = LCD_COLOR_WHITE;
	Controls_Init();
	gpio_output_setup(pin_lcd_bl->bank, pin_lcd_bl->pin,
	    GPIO_Speed_2MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
	pin_set(pin_lcd_bl);
	usb_cdc_init();
	Power_As_Input();
	LCD_DrawBGR(wlarc_logo, 0, 0, 160, 128);
	vTaskDelay(3000);
	LCD_ColorGradientTest();

	for(;;) {
		led_set(get_red_state(), PTT_Read());
		vTaskDelay(50);
	}
}

static void red_main(void* machtnichts __attribute__((unused))) {

	for(;;){
		set_red_state(0);
		sleep();
		set_red_state(1);
		sleep();
	}
}

#ifdef CODEPLUGS
static void
lua_main(void *args)
{
	lua_State *lua;

	/* initialize Lua */
	lua = luaL_newstate();
	if (lua == NULL)
		return;
	luaL_openlibs(lua);

	lua_close(lua);
}
#endif

static void sleep(void) {
	TickType_t delay;

	delay = Encoder_Read() << 6;

	vTaskDelay(delay);
}
