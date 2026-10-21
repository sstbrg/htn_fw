/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash.c
  * @brief   This file provides code for the configuration
  *          of the flash instances.
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
#include "flash.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* FLASH init function */
void MX_FLASH_Init(void)
{

  /* USER CODE BEGIN FLASH_Init 0 */

  /* USER CODE END FLASH_Init 0 */

  FLASH_OBProgramInitTypeDef pOBInit = {0};
  FLASH_BBAttributesTypeDef FLASH_BBSecInitStruct = {0};

  /* USER CODE BEGIN FLASH_Init 1 */

  /* USER CODE END FLASH_Init 1 */
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    Error_Handler();
  }

  /* Option Bytes settings */

  if (HAL_FLASH_OB_Unlock() != HAL_OK)
  {
    Error_Handler();
  }
  pOBInit.OptionType = OPTIONBYTE_BOOTADDR;
  pOBInit.BootAddrConfig = OB_BOOTADDR_NS0;
  pOBInit.BootAddr = 0x08000000;
  if (HAL_FLASHEx_OBProgram(&pOBInit) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_FLASH_OB_Lock() != HAL_OK)
  {
    Error_Handler();
  }

  /* Block-based page protection */

  FLASH_BBSecInitStruct.Bank = FLASH_BANK_1;
  FLASH_BBSecInitStruct.BBAttributesType = FLASH_BB_PRIV;
  FLASH_BBSecInitStruct.BBAttributes_array[0] =   0xFFFFFFFF;
  FLASH_BBSecInitStruct.BBAttributes_array[1] =   0xFFFFFFFF
                              ;
  FLASH_BBSecInitStruct.BBAttributes_array[2] =   0xFFFFFFFF;
  FLASH_BBSecInitStruct.BBAttributes_array[3] =   0xFFFFFFFF
                              ;
  FLASH_BBSecInitStruct.BBAttributes_array[4] =   0xFFFFFFFF;
  FLASH_BBSecInitStruct.BBAttributes_array[5] =   0xFFFFFFFF
                              ;
  FLASH_BBSecInitStruct.BBAttributes_array[6] =   0xFFFFFFFF;
  FLASH_BBSecInitStruct.BBAttributes_array[7] =   0x7FFFFFFF;
  if (HAL_FLASHEx_ConfigBBAttributes(&FLASH_BBSecInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_FLASH_Lock() != HAL_OK)
  {
    Error_Handler();
  }

  /* Launch Option Bytes Loading */
  /*HAL_FLASH_OB_Launch(); */

  /* USER CODE BEGIN FLASH_Init 2 */

  /* USER CODE END FLASH_Init 2 */

}

/* USER CODE BEGIN 1 */
HAL_StatusTypeDef Flash_Write(uint32_t address, uint64_t *data, uint32_t num_of_quadwords)
{

	HAL_StatusTypeDef status;
	uint32_t i;
	// Unlock Flash
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
	// Write data in 128-bit (16-byte) chunks
	for (i = 0; i < num_of_quadwords; i++)
	{
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, address, (uint32_t) data);
		address += 16;
		data += 2;
		if (status != HAL_OK)
		{
			break;
		}
		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
	}
	// Lock Flash
	HAL_FLASH_Lock();
	Update_Cash_Mem();
	return status;
}



HAL_StatusTypeDef Flash_Erase(uint32_t page)
{
	HAL_StatusTypeDef status;
	__IO uint32_t *reg_cr;
	// Unlock Flash
	HAL_FLASH_Unlock();

	/* Wait for last operation to be completed */
	status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

	if (status == HAL_OK)
	{
		/* Access to SECCR or NSCR depends on operation type */
		reg_cr = IS_FLASH_SECURE_OPERATION() ? &(FLASH->SECCR) : &(FLASH->NSCR);
		FLASH_PageErase(page, FLASH_APP_BANK);
		/* Wait for last operation to be completed */
		status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

		/* If the erase operation is completed, disable the associated bits */
		CLEAR_BIT((*reg_cr), (FLASH_NSCR_PER) & (~(FLASH_NON_SECURE_MASK)));
	}
	// Lock Flash
	HAL_FLASH_Lock();
	Update_Cash_Mem();
	return status;
}


HAL_StatusTypeDef FLASH_MassErase(uint32_t Banks){

	HAL_StatusTypeDef status;
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
	FLASH_EraseInitTypeDef erase;
	uint32_t error =0;
	erase.TypeErase = FLASH_TYPEERASE_MASSERASE;
	erase.Banks = Banks;
	HAL_FLASH_Unlock();
	status = HAL_FLASHEx_Erase(&erase, &error);
	HAL_FLASH_Lock();
	Update_Cash_Mem();
	return status;
}


uint32_t Get_CRC_File(void){
	return ((uint32_t)*(__IO uint32_t *)HEADER_FILE_CRC_ADDRESS);
}
uint32_t Get_Len_File(void){
	return ((uint32_t)*(__IO uint32_t *)HEADER_FILE_LEN_ADDRESS);
}

/* USER CODE END 1 */
