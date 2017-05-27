#include "gpio.h"
#include "usb_cdc.h"

static void
input_setup(struct pin *pin)
{
	pin_enable(pin);
	pin_set_pupd(pin, PIN_PUPD_UP);
	pin_set_ospeed(pin, PIN_SPEED_50MHZ);
	pin_set_mode(pin, PIN_MODE_INPUT);
}

uint8_t
Encoder_Read(void)
{
	static const uint8_t map[16]={11, 12, 10, 9, 14, 13, 15, 16, 6, 5, 7, 8, 3, 4, 2, 1};

	input_setup(pin_ecn0);
	input_setup(pin_ecn1);
	input_setup(pin_ecn2);
	input_setup(pin_ecn3);
	return map[pin_read(pin_ecn0) |
	    (pin_read(pin_ecn1) << 1) |
	    (pin_read(pin_ecn2) << 2) |
	    (pin_read(pin_ecn3) << 3)];
}

bool
PTT_Read(void)
{
	input_setup(pin_ptt);
	return !pin_read(pin_ptt);
}
