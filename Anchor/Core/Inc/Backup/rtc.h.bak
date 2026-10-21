/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.h
  * @brief   This file contains all the function prototypes for
  *          the rtc.c file
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
#ifndef __RTC_H__
#define __RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN Private defines */
#define RTC_WRITE_COUNTER(IDX,VAL)	{\
		TAMP->BKP##IDX##R = VAL;		\
}

#define  BKUP_REG6 6
#define  UPDATE_FW_MODE 0x12345678
/* USER CODE END Private defines */

void MX_RTC_Init(void);

/* USER CODE BEGIN Prototypes */
typedef struct{
	uint32_t _0_Nmi_counter;
	uint32_t _1_HardFault_counter;
	uint32_t _2_MemManage_counter;
	uint32_t _3_BusFault_counter;
	uint32_t _4_UsageFault_counter;
	uint32_t _5_Watchdog_counter;
}Fault_Counter;


void Fault_BackupRegister_read(Fault_Counter* f); //for debug operation
void BackupRegister_write(void);
uint32_t BackupRegister_read(void);
bool Update_FW_Boot_Flag(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H__ */

