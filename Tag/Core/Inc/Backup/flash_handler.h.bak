/*
 * flash_hander.h
 *
 *  Created on: Mar 31, 2025
 *      Author: LironAvrmov
 */

#ifndef FLASH_HANDLER_H
#define FLASH_HANDLER_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

#define FLASH_APP_ADDRESS  0x08200000    // Last flash address in bank 1
#define FLASH_HEADER_USER_BANK FLASH_BANK_2
#define HEADER_PAGE		255

#define FLASH_SELF_ID_USER_BANK FLASH_BANK_1
#define FLASH_SELF_ID_PAGE 255
#define FLASH_SELF_ID_ADDRESS  	0x081FE000   // (bank 1, Page 255)

#define FLASH_KEY_PAGE 1
#define FLASH_KEY_ADDRESS  		0x08202000    // (bank 1, Page 1)

#define SELF_ID (*(uint16_t*)FLASH_SELF_ID_ADDRESS)
#define ID_MSB ((uint8_t)(SELF_ID >> 8))
#define ID_LSB ((uint8_t)(SELF_ID & 0xFF))

#define HEADER_PAGE		255

#define BOOT_START_ADDR 0x08000000UL

typedef void (*app_entry_t)(void);

#ifdef __cplusplus
extern "C" {
#endif
HAL_StatusTypeDef Flash_Write(uint32_t address, uint64_t *data, uint32_t num_of_quadwords);
HAL_StatusTypeDef Flash_Erase(uint32_t bank, uint32_t page);
void Update_Cash_Mem(void);

void Jump_To_Boot(void);

#ifdef __cplusplus
}
#endif


#endif
