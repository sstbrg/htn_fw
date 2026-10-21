/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    crc.h
  * @brief   This file contains all the function prototypes for
  *          the crc.c file
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
#ifndef __CRC_H__
#define __CRC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CRC_HandleTypeDef hcrc;

/* USER CODE BEGIN Private defines */
extern DMA_HandleTypeDef handle_GPDMA1_Channel4;

#define NO_CRC 0xffffffff
#define NO_LEN 0xffffffff
#define CRC_TIMEOUT_MS 1000
#define HEADER_QWORD 1

#define FLASH_BASE_ADDRS_MAIN_APP 	0x08000000
#define FLASH_END_ADDRS_MAIN_APP 	0x083FFFFF
#define CRC_TIMEOUT_MS 1000
/* USER CODE END Private defines */

void MX_CRC_Init(void);

/* USER CODE BEGIN Prototypes */
uint32_t Get_CRC_File(void);
uint32_t Get_LEN_File(void);
HAL_StatusTypeDef Set_CRC_File(void);
uint32_t FlashCRC_Calc(uint32_t start_addr, uint32_t stop_addr);
void CRC_File_Handler(void);
static bool FlashRange_Valid(uint32_t start_addr, uint32_t stop_addr);
static inline void CRC_Reset(void);
void Clear_Header_File(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CRC_H__ */

