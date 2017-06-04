/**
  ******************************************************************************
  * @file    SPI/SPI_FLASH/spi_flash.h
  * @author  MCD Application Team
  * @version V1.8.0
  * @date    04-November-2016
  * @brief   This file contains all the functions prototypes for the spi_flash
  *          firmware driver.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_FLASH_H
#define __SPI_FLASH_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_spi.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* W25QxBV SPI Flash supported commands */  
#define sFLASH_CMD_WREN			0x06	/* Write enable */
#define sFLASH_CMD_WRENVSR 		0x50	/* Write enable for Volatile Status Register */
#define sFLASH_CMD_WRDI			0x04	/* Write disable */
#define sFLASH_CMD_RDSR			0x05	/* Read Status Register-1 */
#define sFLASH_CMD_RDSR2		0x35	/* Read Status Register-2 */
#define sFLASH_CMD_WRSR			0x01	/* Write Status Register */
#define sFLASH_CMD_WRITE		0x02	/* Page Program */
#define sFLASH_CMD_WRITEQ		0x32	/* Quad Page Program */
#define sFLASH_CMD_SE			0x20	/* Sector Erase (4k) instruction */
#define sFLASH_CMD_BE32			0x52	/* Block Erase (32k) */
#define sFLASH_CMD_BE64			0xD8	/* Block Erase (64k) */
#define sFLASH_CMD_BE			0xC7	/* Chip Erase */
#define sFLASH_CMD_EPS			0x75	/* Erase/Program Suspend */
#define sFLASH_CMD_EPR			0x7A	/* Erase/Program Resume */
#define sFLASH_CMD_PD			0xB9	/* Power Down */
#define sFLASH_CMD_CRR			0xFF	/* Continuous Read Mode Reset */

#define sFLASH_CMD_READ			0x03	/* Read Data */
#define sFLASH_CMD_FREAD		0x0B	/* Fast Read */
#define sFLASH_CMD_FREAD_DO		0x3B	/* Fast Read Dual Output */
#define sFLASH_CMD_FREAD_QO		0x6B	/* Fast Read Quad Output */
#define sFLASH_CMD_FREAD_DIO		0xBB	/* Fast Read Dual I/O */
#define sFLASH_CMD_FREAD_QIO		0xEB	/* Fast Read Quad I/O */
#define sFLASH_CMD_WREAD_QIO		0xE7	/* Word Read Quad I/O */
#define sFLASH_CMD_OWREAD_QIO		0xE3	/* Octal Word Read Quad I/O */
#define sFLASH_CMD_SBW			0x77	/* Set Burst with Wrap */

#define sFLASH_CMD_RPDID		0xAB	/* Release Power down/Device ID */
#define sFLASH_CMD_MDID			0x90	/* Manufacturer/Device ID */
#define sFLASH_CMD_MDID_DIO		0x92	/* Manufacturer/Device ID by Dual I/O */
#define sFLASH_CMD_MDID_QIO		0x94	/* Manufacturer/Device ID by Quad I/O */
#define sFLASH_CMD_RDID			0x9F	/* JEDEC ID */
#define sFLASH_CMD_RUID			0x48	/* Read Unique ID */
#define sFLASH_CMD_RSFDP		0x5A	/* Read SFDP Register */
#define sFLASH_CMD_ESR			0x44	/* Erase Security Registers */
#define sFLASH_CMD_PSR			0x42	/* Program Security Registers */
#define sFLASH_CMD_RSR			0x48	/* Read Security Registers */

#define sFLASH_WIP_FLAG			0x01	/* Write In Progress (WIP) flag */
#define sFLASH_SR_BUSY			0x01	/* Erase/Writein Progress */
#define sFLASH_SR_WEL			0x02	/* Write Enable Latch */
#define sFLASH_SR_BP0			0x04	/* Block Protect Bit 0 (non-volatile) */
#define sFLASH_SR_BP1			0x08	/* Block Protect Bit 1 (non-volatile) */
#define sFLASH_SR_BP2			0x10	/* Block Protect Bit 2 (non-volatile) */
#define sFLASH_SR_BP			0x1c	/* Block Protect Bits (non-volatile) */
#define sFLASH_SR_TB			0x20	/* Top/Bottom Protect (non-volatile) */
#define sFLASH_SR_SEC			0x40	/* Sector Protect (non-volatile) */
#define sFLASH_SR_SRP0			0x80	/* Status Register Protect 0 (non-volatile) */
#define sFLASH_SR_SRP1			0x0100	/* Status Register Protect 1 (non-volatile) */
#define sFLASH_SR_QE			0x0200	/* Quad Enable */
#define sFLASH_SR_RESERVED		0x0400	/* Reserved */
#define sFLASH_SR_LB1			0x0800	/* Security Register Lock Bit 0 (non-volatile OTP) */
#define sFLASH_SR_LB2			0x1000	/* Security Register Lock Bit 1 (non-volatile OTP) */
#define sFLASH_SR_LB3			0x2000	/* Security Register Lock Bit 2 (non-volatile OTP) */
#define sFLASH_SR_LB			0x3800	/* Security Register Lock Bits (non-volatile OTP) */
#define sFLASH_SR_CMP			0x4000	/* Complement Protect (non-volatile) */
#define sFLASH_SR_SUS			0x8000	/* Suspend Status */

#define sFLASH_DUMMY_BYTE		0xA5
#define sFLASH_SPI_PAGESIZE		0x100

#define sFLASH_W25Q128BV_ID		0x00ef4018
#define sFLASH_W25Q80BV_ID		0x00ef4014

/* W25QxBV FLASH SPI Interface pins  */  
#define sFLASH_SPI                           SPI1
#define sFLASH_SPI_CLK                       RCC_APB2Periph_SPI1
#define sFLASH_SPI_CLK_INIT                  RCC_APB2PeriphClockCmd

#define sFLASH_SPI_SCK_PIN                   GPIO_Pin_3
#define sFLASH_SPI_SCK_GPIO_PORT             GPIOB
#define sFLASH_SPI_SCK_GPIO_CLK              RCC_AHB1Periph_GPIOB
#define sFLASH_SPI_SCK_SOURCE                GPIO_PinSource3
#define sFLASH_SPI_SCK_AF                    GPIO_AF_SPI1

#define sFLASH_SPI_MISO_PIN                  GPIO_Pin_4
#define sFLASH_SPI_MISO_GPIO_PORT            GPIOB
#define sFLASH_SPI_MISO_GPIO_CLK             RCC_AHB1Periph_GPIOB
#define sFLASH_SPI_MISO_SOURCE               GPIO_PinSource4
#define sFLASH_SPI_MISO_AF                   GPIO_AF_SPI1

#define sFLASH_SPI_MOSI_PIN                  GPIO_Pin_5
#define sFLASH_SPI_MOSI_GPIO_PORT            GPIOB
#define sFLASH_SPI_MOSI_GPIO_CLK             RCC_AHB1Periph_GPIOB
#define sFLASH_SPI_MOSI_SOURCE               GPIO_PinSource5
#define sFLASH_SPI_MOSI_AF                   GPIO_AF_SPI1

#define sFLASH_CS_PIN                        GPIO_Pin_7
#define sFLASH_CS_GPIO_PORT                  GPIOD
#define sFLASH_CS_GPIO_CLK                   RCC_AHB1Periph_GPIOD

/* Exported macro ------------------------------------------------------------*/
/* Select sFLASH: Chip Select pin low */
#define sFLASH_CS_LOW()       GPIO_ResetBits(sFLASH_CS_GPIO_PORT, sFLASH_CS_PIN)
/* Deselect sFLASH: Chip Select pin high */
#define sFLASH_CS_HIGH()      GPIO_SetBits(sFLASH_CS_GPIO_PORT, sFLASH_CS_PIN)   

/* Exported functions ------------------------------------------------------- */

/* High layer functions  */
void sFLASH_DeInit(void);
void sFLASH_Init(void);
void sFLASH_EraseSector(uint32_t SectorAddr);
void sFLASH_Erase32KBlock(uint32_t SectorAddr);
void sFLASH_Erase64KBlock(uint32_t SectorAddr);
#if 0
void sFLASH_EraseBulk(void);
#endif
void sFLASH_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void sFLASH_WriteBuffer(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void sFLASH_ReadBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
void sFLASH_ReadSecurityBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
uint32_t sFLASH_ReadID(void);
void sFLASH_StartReadSequence(uint32_t ReadAddr);

/* Low layer functions */
uint8_t sFLASH_ReadByte(void);
uint8_t sFLASH_SendByte(uint8_t byte);
uint16_t sFLASH_SendHalfWord(uint16_t HalfWord);
void sFLASH_WriteEnable(void);
void sFLASH_WaitForWriteEnd(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_FLASH_H */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
