#include "controls.h"
#include "gpio.h"
#include "stm32f4xx_rcc.h"

void
Controls_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	gpio_input_setup(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
	gpio_input_setup(GPIOE, GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_11);
}

uint8_t
Encoder_Read(void)
{
	static const uint8_t map[16]={11, 12, 10, 9, 14, 13, 15, 16, 6, 5, 7, 8, 3, 4, 2, 1};

	return map[pin_read(pin_ecn0) |
	    (pin_read(pin_ecn1) << 1) |
	    (pin_read(pin_ecn2) << 2) |
	    (pin_read(pin_ecn3) << 3)];
}

bool
PTT_Read(void)
{
	return !pin_read(pin_ptt);
}
