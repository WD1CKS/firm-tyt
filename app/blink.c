
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "led.h"
#include "gpio.h"
#include "usb_cdc.h"
#include "lcd_driver.h"

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
static lcd_context_t lcd;

int main (void) {
	red_monitor = xSemaphoreCreateMutex();
	xTaskCreate(output_main, "out", 256, NULL, 1, NULL);
	xTaskCreate(red_main, "red", 256, NULL, 0, NULL);
#ifdef CODEPLUGS
	xTaskCreate(lua_main, "lua", 32*1024, NULL, 0, NULL);
#endif
	vTaskStartScheduler();
	for(;;);
}

static int  red_state;
static void set_red_state(int i) {
	xSemaphoreTake(red_monitor, portMAX_DELAY);
	red_state = i;
	xSemaphoreGive(red_monitor);
}
static int  get_red_state() {
	int i;
	xSemaphoreTake(red_monitor, portMAX_DELAY);
	i = red_state;
	xSemaphoreGive(red_monitor);
	return i;
}


static void led_set(int red, int green) {
	red_led(red);
	if (!red)
		pin_set(pin_lcd_bl);
	green_led(green);

	const char red_on[] = "red on,  ";
	const char red_off[] = "red off, ";
	const char green_on[] = "green on\n";
	const char green_off[] = "green off\n";
	#define SEND_STR(s) usb_cdc_write((void*)(s), strlen(s))
	SEND_STR(red?red_on:red_off);
	SEND_STR(green?green_on:green_off);
	#undef SEND_STR
	lcd.x = 0;
	lcd.y = 0;
	LCD_DrawString(&lcd, red?red_on:red_off);
	LCD_DrawString(&lcd, green?green_on:green_off);
}

static void input_setup(struct pin *pin) {
	pin_enable(pin);
	pin_set_mode(pin, PIN_MODE_INPUT);
	pin_set_pupd(pin, PIN_PUPD_NONE);
}

static void output_setup(struct pin *pin) {
	pin_enable(pin);
	pin_set_mode(pin, PIN_MODE_OUTPUT);
	pin_set_otype(pin, PIN_TYPE_PUSHPULL);
	pin_set_ospeed(pin, PIN_SPEED_2MHZ);
	pin_set_pupd(pin, PIN_PUPD_NONE);
	pin_reset(pin);
}

static void output_main(void* machtnichts) {

	led_setup();
	usb_cdc_init();

	input_setup(pin_e11);
	input_setup(pin_ecn0);
	input_setup(pin_ecn1);
	input_setup(pin_ecn2);
	input_setup(pin_ecn3);
	output_setup(pin_lcd_bl);
	LCD_InitContext(&lcd);
	lcd.bg_color = LCD_COLOR_WHITE;
	lcd.fg_color = LCD_COLOR_BLACK;

	for(;;) {
		// PTT is active low
		int ptt = !pin_read(pin_e11);
		int red = get_red_state();
		led_set(red, ptt);
		vTaskDelay(50);
	}
}

static void red_main(void* machtnichts) {

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

	delay = (pin_read(pin_ecn0) << 6) |
		(pin_read(pin_ecn1) << 7) |
		(pin_read(pin_ecn2) << 8) |
		(pin_read(pin_ecn3) << 9);

	vTaskDelay(delay);
}
