#include <strings.h>	// ffs()
#include "gpio.h"

#define DEFINE_PIN(b, n)			\
const struct gpio_pin_def _pin_ ## b ## n = {	\
    .bank = GPIO ## b,				\
    .pin = GPIO_Pin_ ## n			\
}

#define DEFINE_BANK(b)			\
    DEFINE_PIN(b, 0);			\
    DEFINE_PIN(b, 1);			\
    DEFINE_PIN(b, 2);			\
    DEFINE_PIN(b, 3);			\
    DEFINE_PIN(b, 4);			\
    DEFINE_PIN(b, 5);			\
    DEFINE_PIN(b, 6);			\
    DEFINE_PIN(b, 7);			\
    DEFINE_PIN(b, 8);			\
    DEFINE_PIN(b, 9);			\
    DEFINE_PIN(b, 10);			\
    DEFINE_PIN(b, 11);			\
    DEFINE_PIN(b, 12);			\
    DEFINE_PIN(b, 13);			\
    DEFINE_PIN(b, 14);			\
    DEFINE_PIN(b, 15)

DEFINE_BANK(A);
DEFINE_BANK(B);
DEFINE_BANK(C);
DEFINE_BANK(D);
DEFINE_BANK(E);
DEFINE_BANK(F);
DEFINE_BANK(G);
DEFINE_BANK(H);
DEFINE_BANK(I);

void
gpio_input_setup(GPIO_TypeDef* bank, uint16_t pins, GPIOPuPd_TypeDef pupd)
{
	GPIO_InitTypeDef def = {
	    .GPIO_Pin = pins,
	    .GPIO_Mode = GPIO_Mode_IN,
	    .GPIO_PuPd = pupd
	};
	GPIO_Init(bank, &def);
}

void
gpio_af_setup(GPIO_TypeDef* bank, uint16_t pins, uint8_t af, GPIOSpeed_TypeDef speed, GPIOOType_TypeDef type, GPIOPuPd_TypeDef pupd)
{
	uint16_t	af_pins = pins;

	/* stm32*_fsmc.h says to call GPIO_PinAFConfig() first */
	while (af_pins) {
		uint8_t src = ffs(af_pins) - 1;
		GPIO_PinAFConfig(bank, src, af);
		af_pins &= ~(1<<src);
	}
	GPIO_InitTypeDef def = {
	    .GPIO_Pin = pins,
	    .GPIO_Mode = GPIO_Mode_AF,
	    .GPIO_Speed = speed,
	    .GPIO_OType = type,
	    .GPIO_PuPd = pupd
	};
	GPIO_Init(bank, &def);
}

void
gpio_output_setup(GPIO_TypeDef* bank, uint16_t pins,
    GPIOSpeed_TypeDef speed, GPIOOType_TypeDef type,
    GPIOPuPd_TypeDef pupd)
{
	GPIO_InitTypeDef def = {
	    .GPIO_Pin = pins,
	    .GPIO_Mode = GPIO_Mode_OUT,
	    .GPIO_Speed = speed,
	    .GPIO_OType = type,
	    .GPIO_PuPd = pupd
	};
	GPIO_Init(bank, &def);
}

void
gpio_analog_setup(GPIO_TypeDef*bank, uint16_t pins, GPIOPuPd_TypeDef pupd)
{
	GPIO_InitTypeDef def = {
	    .GPIO_Pin = GPIO_Mode_AN,
	    .GPIO_PuPd = pupd
	};
	GPIO_Init(bank, &def);
}
