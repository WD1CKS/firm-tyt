#ifndef _LED_H_
#define _LED_H_

#include "gpio.h"

void led_setup(void);
#define green_led(on)	pin_write(pin_green_led, on)
#define red_led(on)		pin_write(pin_red_led, on)

#endif
