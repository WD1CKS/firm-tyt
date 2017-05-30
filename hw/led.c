#include "led.h"
#include "gpio.h"
#include "stm32f4xx_rcc.h"

void
led_setup(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	gpio_output_setup(pin_green_led->bank, pin_green_led->pin | pin_red_led->pin,
	    GPIO_Speed_2MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
}
