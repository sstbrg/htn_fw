/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash.h
  * @brief   This file contains all the function prototypes for
  *          the flash.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __flash_H__
#define __flash_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

#define FLASH_APP_BANK FLASH_BANK_2

#define QWORD_TO_BYTE 16
#define NUM_OF_QUADWORDS(byte) byte/QWORD_TO_BYTE
#define JUMP_QWORD(qword) QWORD_SIZE_BYTE*qword

#define FLASH_QUADWORD_HEADER_NUM 1
#define HEADER_FILE_ADDRESS 0x083FE000
#define HEADER_FILE_CRC_ADDRESS 0x083FE000
#define HEADER_FILE_LEN_ADDRESS 0x083FE004
#define BANK_SIZE_BYTES 		0x200000
#define APP_ADDRESS 0x08200000
#define HEADER_PAGE		255
#define HEADER_BANK		2


#define PAGE_TO_BYTE 8192
#define  LEN_TO_CRC_CHECK 255 // max pages 256
#define  PAGES_TO_BYTE(num_of_pages) num_of_pages*PAGE_TO_BYTE
/* USER CODE END Private defines */

void MX_FLASH_Init(void);

/* USER CODE BEGIN Prototypes */
HAL_StatusTypeDef FLASH_MassErase(uint32_t Banks);
HAL_StatusTypeDef Flash_Write(uint32_t address, uint64_t *data, uint32_t num_of_quadwords);
HAL_StatusTypeDef Flash_Erase(uint32_t page);
uint32_t Get_CRC_File(void);
uint32_t Get_Len_File(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __flash_H__ */

