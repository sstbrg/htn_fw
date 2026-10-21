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
#define HEADER_FILE_CRC_ADDRESS 0x083FE000
#define HEADER_FILE_LEN_ADDRESS 0x083FE004
#define BANK_SIZE_BYTES 		0x200000

#define PAGE_TO_BYTE 8192
#define  LEN_TO_CRC_CHECK 255 // max pages 256, flash_clr uses range: 0-255
#define  PAGES_TO_BYTE(num_of_pages) num_of_pages*PAGE_TO_BYTE
/* USER CODE END Private defines */

void MX_FLASH_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __flash_H__ */

