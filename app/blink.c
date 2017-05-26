
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "led.h"
#include "gpio.h"
#include "usb_cdc.h"

static void sleep(void);

static void output_main(void*);
static void red_main(void*);
static SemaphoreHandle_t red_monitor;

int main (void) {
	red_monitor = xSemaphoreCreateMutex();
	xTaskCreate(output_main, "out", 256, NULL, 1, NULL);
	xTaskCreate(red_main, "red", 256, NULL, 0, NULL);
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
	green_led(green);

	const char red_on[] = "red on,  ";
	const char red_off[] = "red off, ";
	const char green_on[] = "green on\n";
	const char green_off[] = "green off\n";
	#define SEND_STR(s) usb_cdc_write((void*)(s), strlen(s))
	SEND_STR(red?red_on:red_off);
	SEND_STR(green?green_on:green_off);
	#undef SEND_STR
}

static void input_setup(struct pin *pin) {
	pin_enable(pin);
	pin_set_mode(pin, PIN_MODE_INPUT);
	pin_set_pupd(pin, PIN_PUPD_NONE);
}

static void output_main(void* machtnichts) {

	led_setup();
	usb_cdc_init();

	input_setup(pin_e11);

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

static void sleep(void) {
	TickType_t delay = 0;

	if (pin_read(pin_ecn0))
		delay |= 1<<0;
	if (pin_read(pin_ecn1))
		delay |= 1<<1;
	if (pin_read(pin_ecn2))
		delay |= 1<<2;
	if (pin_read(pin_ecn3))
		delay |= 1<<3;
	delay <<= 6;
	vTaskDelay(delay);
}
