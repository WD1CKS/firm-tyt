/**
  ******************************************************************************
  * @file    usb_bsp.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   This file is responsible to offer board support package and is
  *          configurable by user.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "usb_bsp.h"

#include "gpio.h"
//#include "interrupt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_rcc.h"

/** USB pin definitions for the STM32F4. */
#define USB_VBUS_PIN pin_a9
#define USB_DM_PIN   pin_a11
#define USB_DP_PIN   pin_a12

/**
 * @brief  USB_OTG_BSP_Init
 *         Initilizes BSP configurations
 * @param  None
 * @retval None
 */
void USB_OTG_BSP_Init(USB_OTG_CORE_HANDLE *usb_dev)
{
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
  gpio_input_setup(USB_VBUS_PIN->bank, USB_VBUS_PIN->pin);

  gpio_af_setup(USB_DM_PIN->bank, USB_DM_PIN->pin | USB_DP_PIN->pin,
    GPIO_AF_OTG_FS, GPIO_High_Speed, GPIO_OType_PP, GPIO_PuPd_NOPULL);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
  RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS, ENABLE);
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}

/**
 * @brief  USB_OTG_BSP_EnableInterrupt
 *         Enabele USB Global interrupt
 * @param  None
 * @retval None
 */
void USB_OTG_BSP_EnableInterrupt(USB_OTG_CORE_HANDLE *pdev)
{
  NVIC_EnableIRQ(OTG_FS_IRQn);
}

/**
 * @brief  USB_OTG_BSP_uDelay
 *         This function provides delay time in micro sec
 * @param  usec : Value of delay required in micro sec
 * @retval None
 */
void USB_OTG_BSP_uDelay(uint32_t usec)
{

	while (usec > 1000) {
		USB_OTG_BSP_mDelay(1);
		usec -= 1000;
	}

	const uint32_t bound = usec * 168;
	for (uint32_t j = 0; j < bound; j+= 4) {
		asm volatile ("mov r0,r0");
	}

}

/**
 * @brief  USB_OTG_BSP_mDelay
 *          This function provides delay time in milli sec
 * @param  msec : Value of delay required in milli sec
 * @retval None
 */
void USB_OTG_BSP_mDelay(const uint32_t msec)
{
	vTaskDelay(msec);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
