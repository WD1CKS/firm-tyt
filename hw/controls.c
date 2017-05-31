#include <strings.h>	// ffs()

#include "FreeRTOS.h"
#include "task.h"

#include "controls.h"
#include "gpio.h"
#include "stm32f4xx_rcc.h"
#include "usb_cdc.h"

void
Controls_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	gpio_input_setup(GPIOB, GPIO_Pin_10 | GPIO_Pin_11, GPIO_High_Speed, GPIO_PuPd_NOPULL);
	gpio_input_setup(GPIOE, GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_11, GPIO_High_Speed, GPIO_PuPd_NOPULL);

	/* Overrides power switch */
	gpio_output_setup(GPIOA, GPIO_Pin_7, GPIO_Low_Speed, GPIO_OType_PP, GPIO_PuPd_NOPULL);
	pin_set(pin_a7);
	/* Reads current power switch state */
	gpio_input_setup(GPIOA, GPIO_Pin_1, GPIO_Low_Speed, GPIO_PuPd_NOPULL);
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

#include <stdio.h>
#include "lcd_driver.h"
extern lcd_context_t lcd;
uint32_t
keypad_read(void)
{
	uint32_t	ret;
	uint16_t	gpios;

	xSemaphoreTake(LCD_Mutex, portMAX_DELAY);
	if (LCD_Enabled != LCD_KEYPAD) {
		gpio_input_setup(GPIOD, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_14 | GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_PuPd_UP);
		gpio_input_setup(GPIOE, GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10, GPIO_Speed_50MHz, GPIO_PuPd_UP);
		gpio_output_setup(GPIOA, GPIO_Pin_6, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		gpio_output_setup(GPIOD, GPIO_Pin_2 | GPIO_Pin_3, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL);
		LCD_Enabled = LCD_KEYPAD;
	}
	GPIO_ResetBits(GPIOA, GPIO_Pin_6);
	GPIO_SetBits(GPIOD, GPIO_Pin_2 | GPIO_Pin_3);
	vTaskDelay(2);
	gpios = GPIO_ReadInputData(GPIOD) & 0xc003;
	gpios |= GPIO_ReadInputData(GPIOE) & 0x0780;
	ret = gpios;
	pin_set(pin_a6);
	pin_reset(pin_d2);
	vTaskDelay(2);
	gpios = GPIO_ReadInputData(GPIOD) & 0xc003;
	gpios |= GPIO_ReadInputData(GPIOE) & 0x0780;
	ret |= gpios << 16;
	pin_set(pin_d2);
	pin_reset(pin_d3);
	vTaskDelay(2);
	if (pin_read(pin_e10))
		ret |= 0x04;
	if (pin_read(pin_e9))
		ret |= 0x040000;
	pin_set(pin_d3);
	xSemaphoreGive(LCD_Mutex);
	ret |= (pin_read(pin_a1) << 4);
	if (pin_read(pin_ptt))
		ret |= 0x08;
	lcd.x = 0;
	lcd.y = 24;
	char ks[16];
	sprintf(ks, "Keys: %08x", ret);
	LCD_DrawString(&lcd, ks);
	return ret;
}

static uint32_t last_state = 0xc787c78f;
static const char *keymap = "34MTP..560*...12\x19""7~....89#\x1b""...\n\x18";

char
get_key(void)
{
	uint32_t state = keypad_read();
	uint32_t pressed;
	int key;

	/* Anything that was released goes directly into the last state mask */
	last_state |= state;
	if (state == last_state)
		return 0;

	/* Now find things that are newly zero... */
	pressed = last_state ^ state;

	key = ffs(pressed) - 1;
	last_state &= ~(1<<(key));
	lcd.x = 0;
	lcd.y = 32;
	char kp[16];
	sprintf(kp, "Key: 0x%02x (%c)", keymap[key], keymap[key]);
	LCD_DrawString(&lcd, kp);
	return keymap[key];
}
