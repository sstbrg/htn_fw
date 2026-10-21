/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    crc.c
  * @brief   This file provides code for the configuration
  *          of the CRC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "crc.h"

/* USER CODE BEGIN 0 */
#include "flash.h"
#include "flash_handler.h"
/* USER CODE END 0 */

CRC_HandleTypeDef hcrc;

/* CRC init function */
void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

void HAL_CRC_MspInit(CRC_HandleTypeDef* crcHandle)
{

  if(crcHandle->Instance==CRC)
  {
  /* USER CODE BEGIN CRC_MspInit 0 */

  /* USER CODE END CRC_MspInit 0 */
    /* CRC clock enable */
    __HAL_RCC_CRC_CLK_ENABLE();
  /* USER CODE BEGIN CRC_MspInit 1 */

  /* USER CODE END CRC_MspInit 1 */
  }
}

void HAL_CRC_MspDeInit(CRC_HandleTypeDef* crcHandle)
{

  if(crcHandle->Instance==CRC)
  {
  /* USER CODE BEGIN CRC_MspDeInit 0 */

  /* USER CODE END CRC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CRC_CLK_DISABLE();
  /* USER CODE BEGIN CRC_MspDeInit 1 */

  /* USER CODE END CRC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void CRC_File_Handler(void){
	if (Get_CRC_File() == NO_CRC && Get_LEN_File() == NO_LEN) //Means Not programmed from IDE
		Set_CRC_File();
	else //header exists
		return;
}

void Clear_Header_File(void){
	HAL_StatusTypeDef status = Flash_Erase(FLASH_HEADER_USER_BANK, HEADER_PAGE);
}

HAL_StatusTypeDef Set_CRC_File(void){
	uint32_t crc = FlashCRC_Calc(FLASH_APP_ADDRESS,FLASH_APP_ADDRESS + PAGES_TO_BYTE(LEN_TO_CRC_CHECK));
	HEADER_FILE header_file = {0};
	header_file.crc = crc;
	header_file.len = PAGES_TO_BYTE(LEN_TO_CRC_CHECK);
	Clear_Header_File();	 //erase crc header just in-case before writing.
	HAL_StatusTypeDef error = Flash_Write(HEADER_FILE_CRC_ADDRESS, (uint64_t*)&header_file, HEADER_QWORD);
	if(error != HAL_OK){
		return HAL_ERROR;
	}
	Update_Cash_Mem();
	return HAL_OK;
}


uint32_t Get_CRC_File(void){
	return ((uint32_t)*(__IO uint32_t *)HEADER_FILE_CRC_ADDRESS);
}
uint32_t Get_LEN_File(void){
	return ((uint32_t)*(__IO uint32_t *)HEADER_FILE_LEN_ADDRESS);
}



uint32_t FlashCRC_Calc(uint32_t start_addr, uint32_t stop_addr) {
	//safety check
	if (!FlashRange_Valid(start_addr, stop_addr)) {
		return 0U;
	}

	CRC_Reset();

	uint32_t addr = start_addr;
	uint32_t len = stop_addr - start_addr;

	uint32_t head_bytes = addr & 0x3U;
	if (head_bytes) {
		head_bytes = 4U - head_bytes;
		if (head_bytes > len) {
			head_bytes = len;
		}

		uint32_t word = 0;
		for (uint32_t i = 0; i < head_bytes; ++i) {
			word |= ((uint32_t) (*((volatile uint8_t*) (addr + i))))
					<< (8U * i);
		}
		hcrc.Instance->DR = word;
		addr += head_bytes;
		len -= head_bytes;
	}

//	uint32_t word_count = len / 4U;
//	uint32_t tail_bytes = len % 4U;
	uint32_t chunk_size;

	while (len > 0) {
		if (len > 0x8000)
			chunk_size = 0x8000;
		else
			chunk_size = len;

		if (HAL_DMA_Start(&handle_GPDMA1_Channel4, (uint32_t) addr, (uint32_t) &(hcrc.Instance->DR), chunk_size) != HAL_OK) {
			for (uint32_t i = 0; i < (0x8000 / 4); ++i) {
				uint32_t w = *((volatile uint32_t*) (addr + (i * 4U)));
				hcrc.Instance->DR = w;
			}
		} else {
			if (HAL_DMA_PollForTransfer(&handle_GPDMA1_Channel4, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY) != HAL_OK) {
				return 0U;
			}
		}
		addr += chunk_size;
		len -= chunk_size;
	}

	uint32_t crc = hcrc.Instance->DR;
	return crc;
}


static bool FlashRange_Valid(uint32_t start_addr, uint32_t stop_addr) {
	if (start_addr > stop_addr) {
		return false;
	}
	if (start_addr < FLASH_BASE_ADDRS_MAIN_APP) {
		return false;
	}
	if (stop_addr > FLASH_END_ADDRS_MAIN_APP + 1U) {
		return false;
	}

	if (stop_addr & 0x3)
		return false;
	return true;
}

static inline void CRC_Reset(void) {
	__HAL_CRC_DR_RESET(&hcrc);
}
/* USER CODE END 1 */
