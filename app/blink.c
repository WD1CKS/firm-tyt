
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "led.h"
#include "usb_cdc.h"
#include "controls.h"
#include "gpio.h"

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

int main (void) {
	red_monitor = xSemaphoreCreateMutex();
	xTaskCreate(output_main, "out", 2*1024, NULL, 1, NULL);
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
	static int last_state = -1;
	static char enc[] = "encoder: 00, ";
	int ev;
	int state;

	red_led(red);
	if (red)
		pin_reset(pin_lcd_bl);
	else
		pin_set(pin_lcd_bl);
	green_led(green);

	ev = Encoder_Read();
	state = ev<<2 | (red?2:0) | (green?1:0);
	if (state == last_state)
		return;
	last_state = state;
	enc[9] = ev / 10 + 48;
	enc[10] = ev % 10 + 48;
	usb_cdc_write(enc, 13);
	const char red_on[] = "red on,  ";
	const char red_off[] = "red off, ";
	const char green_on[] = "green on\n";
	const char green_off[] = "green off\n";
	#define SEND_STR(s) usb_cdc_write((void*)(s), strlen(s))
	SEND_STR(red?red_on:red_off);
	SEND_STR(green?green_on:green_off);
	#undef SEND_STR
}

static void output_main(void* machtnichts) {
	led_setup();
	pin_set_mode(pin_lcd_bl, PIN_MODE_OUTPUT);
	pin_set_otype(pin_lcd_bl, PIN_TYPE_PUSHPULL);
	pin_set_ospeed(pin_lcd_bl, PIN_SPEED_2MHZ);
	pin_set_pupd(pin_lcd_bl, PIN_PUPD_NONE);
	usb_cdc_init();

	for(;;) {
		led_set(get_red_state(), PTT_Read());
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

	delay = Encoder_Read() << 6;

	vTaskDelay(delay);
}
