#include <strings.h>	// ffs()

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "controls.h"
#include "gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_adc.h"
#include "usb_cdc.h"

static SemaphoreHandle_t ADC1_Mutex;

void
Controls_Init(void)
{
	ADC_InitTypeDef ai;
	ADC_CommonInitTypeDef aci;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	gpio_input_setup(GPIOB, GPIO_Pin_10 | GPIO_Pin_11, GPIO_PuPd_NOPULL);
	gpio_input_setup(GPIOE, GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_11, GPIO_PuPd_NOPULL);

	/* And ADC1 */
	ADC1_Mutex = xSemaphoreCreateMutex();
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	aci.ADC_Mode = ADC_Mode_Independent;
	aci.ADC_Prescaler = ADC_Prescaler_Div2;
	aci.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	aci.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&aci);

	ai.ADC_Resolution = ADC_Resolution_12b;
	ai.ADC_ScanConvMode = DISABLE;
	ai.ADC_ContinuousConvMode = DISABLE;
	ai.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ai.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_TRGO;
	ai.ADC_DataAlign = ADC_DataAlign_Right;
	ai.ADC_NbrOfConversion = 1;
	ADC_Init(ADC1, &ai);

	ADC_Cmd(ADC1, ENABLE);
}

void
Power_As_Input(void)
{
	/* Overrides power switch */
	gpio_output_setup(GPIOA, GPIO_Pin_7, GPIO_Low_Speed, GPIO_OType_PP, GPIO_PuPd_NOPULL);
	pin_set(pin_a7);
	/* Reads current power switch state */
	gpio_input_setup(GPIOA, GPIO_Pin_1, GPIO_PuPd_NOPULL);
}

void
Normal_Power(void)
{
	gpio_output_setup(GPIOA, GPIO_Pin_7, GPIO_Low_Speed, GPIO_OType_PP, GPIO_PuPd_NOPULL);
	pin_reset(pin_a7);
}

uint8_t
Encoder_Read(void)
{
	static const uint8_t map[16]={11, 12, 10, 9, 14, 13, 15, 16, 6, 5, 7, 8, 3, 4, 2, 1};

	/* TODO: We only need a single read here. */
	return map[pin_read(pin_ecn0) |
	    (pin_read(pin_ecn1) << 1) |
	    (pin_read(pin_ecn2) << 2) |
	    (pin_read(pin_ecn3) << 3)];
}

static uint16_t
ADC1_Read(uint8_t chan, const struct gpio_pin_def *pin)
{
	uint16_t ret;

	xSemaphoreTake(ADC1_Mutex, portMAX_DELAY);
	if (pin)
		gpio_analog_setup(pin->bank, pin->pin, GPIO_PuPd_NOPULL);
	ADC_RegularChannelConfig(ADC1, chan, 1, ADC_SampleTime_480Cycles);
	ADC_SoftwareStartConv(ADC1);
	while (ADC_GetSoftwareStartConvStatus(ADC1) != RESET)
		vTaskDelay(1);
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
		vTaskDelay(1);
	if (pin)
		gpio_input_setup(pin->bank, pin->pin, GPIO_PuPd_NOPULL);
	ret = ADC_GetConversionValue(ADC1);
	xSemaphoreGive(ADC1_Mutex);
	return ret;
}

uint16_t
VOL_Read(void)
{
	return ADC1_Read(ADC_Channel_0, pin_a0);
}

uint16_t
BATT_Read(void)
{
	return ADC1_Read(ADC_Channel_1, pin_a1);
}

uint16_t
BATT2_Read(void)
{
	uint16_t rc;

	ADC_VBATCmd(ENABLE);
	rc = ADC1_Read(ADC_Channel_18, NULL);
	ADC_VBATCmd(DISABLE);
	return rc;
}

uint16_t
Temp_Read(void)
{
	uint16_t rc;

	ADC_TempSensorVrefintCmd(ENABLE);
	rc = ADC1_Read(ADC_Channel_16, NULL);
	ADC_TempSensorVrefintCmd(DISABLE);
	return rc;
}

/* Applies the taper to generate a 1-10 number */
uint8_t
VOL_Taper(uint16_t vol)
{
	if (vol < 350)
		return (vol/70)+1;
	vol-=350;
	vol /= 330;
	vol += 6;
	if (vol > 10)
		vol = 10;
	return vol;
}

uint8_t
PTT_Read(void)
{
	uint8_t ret;
	ret = !pin_read(pin_ptt);
	ret |= (!pin_read(pin_extptt)) << 1;
	return ret;
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
		gpio_input_setup(GPIOD, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_14 | GPIO_Pin_15, GPIO_PuPd_UP);
		gpio_input_setup(GPIOE, GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10, GPIO_PuPd_UP);
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
	/* TODO: Combine pin reads */
	if (pin_read(pin_e10))
		ret |= 0x04;
	if (pin_read(pin_e9))
		ret |= 0x040000;
	pin_set(pin_d3);
	xSemaphoreGive(LCD_Mutex);
	if (pin_read(pin_ptt))
		ret |= 0x08;
	if (pin_read(pin_extptt))
		ret |= 0x10;
	ret |= (pin_read(pin_a1) << 5);
	lcd.x = 0;
	lcd.y = 24;
	char ks[16];
	sprintf(ks, "Keys: %08" PRIx32, ret);
	LCD_DrawString(&lcd, ks);
	return ret;
}

static uint32_t last_state = 0xc787c7cf;
static const char *keymap = "34MTtP.560*...12\x19""7~....89#\x1b""...\n\x18";

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
