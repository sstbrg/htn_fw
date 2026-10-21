#include <string.h>
#include "flash_handler.h"
#include "main.h"

extern uint16_t reset_counter;
void Update_Cash_Mem(void);

#ifdef DEBUG_RESET_COUNTER_IN_FLASH

uint16_t Flash_Increase_ResetCounter(){
		reset_counter = reset_counter + 1;
		const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;	// keep antenna delays (slots 1-2)
	uint16_t new_id[8] = {PC_ID,page[1],page[2],page[3],page[4],page[5],page[6],reset_counter};
		uint8_t error;
		error = Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK;
		if (error){
			return 0xffff;
		}
		error = Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)&new_id, 1) != HAL_OK;
		return 0;
	}

uint16_t Flash_Read_ResetCounter(void){
	 if (HAL_FLASH_Unlock() != HAL_OK)
	 {
		 return 0xffff;
	 }
	 uint16_t reset_counter = *(uint32*)FLASH_RESET_COUNTER;
	 return reset_counter;
}

uint16_t Flash_Reset_ResetCounter(){
	reset_counter = 0;
	const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;	// keep antenna delays (slots 1-2)
	uint16_t new_id[8] = {PC_ID,page[1],page[2],page[3],page[4],page[5],page[6],reset_counter};
	uint8_t error;
	error = Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK;
	if (error){
		return 0xffff;
	}
	error = Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)&new_id, 1) != HAL_OK;
	return 0;
}

#endif

HAL_StatusTypeDef Flash_Write(uint32_t address, uint64_t *data,
		uint32_t num_of_quadwords) {
	HAL_StatusTypeDef status;
	uint32_t i;
	// Unlock Flash
	  if (HAL_FLASH_Unlock() != HAL_OK)
	  {
		  return HAL_ERROR;
	  }

	// Write data in 128-bit (16-byte) chunks
	for (i = 0; i < num_of_quadwords; i++) {
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, address,
				(uint32_t) data);
		address += 16;
		data += 2;
		if (status != HAL_OK) {
			break;
		}
	}
	// Lock Flash
	if (HAL_FLASH_Lock()  != HAL_OK)
	{
		return HAL_ERROR;
	}

	Update_Cash_Mem();
	return status;
}

HAL_StatusTypeDef Flash_Erase(uint32_t bank, uint32_t page) {
	HAL_StatusTypeDef status;
	__IO uint32_t *reg_cr;
	// Unlock Flash
	 if (HAL_FLASH_Unlock() != HAL_OK)
		  {
			  return HAL_ERROR;
		  }

	/* Wait for last operation to be completed */
	status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

	if (status == HAL_OK) {
		/* Access to SECCR or NSCR depends on operation type */
		reg_cr = IS_FLASH_SECURE_OPERATION() ? &(FLASH->SECCR) : &(FLASH->NSCR);
		FLASH_PageErase(page, bank);
		/* Wait for last operation to be completed */
		status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

		/* If the erase operation is completed, disable the associated bits */
		CLEAR_BIT((*reg_cr),
				(FLASH_NSCR_PER) & (~(FLASH_NON_SECURE_MASK)));
	}
	// Lock Flash
	if (HAL_FLASH_Lock()  != HAL_OK)
		{
			return HAL_ERROR;
		}

	return status;
}

void Update_Cash_Mem(){
	ICACHE->CR &= ~ICACHE_CR_EN;
	ICACHE->CR |= ICACHE_CR_CACHEINV;
	ICACHE->CR |= ICACHE_CR_EN;
}

void Jump_To_Boot(void){

	uint32_t app_msp = *(volatile uint32_t *)(BOOT_START_ADDR + 0X00); //Stack pointer
	uint32_t app_reset_pc = *(volatile uint32_t *)(BOOT_START_ADDR + 0X04); //Reset Handler
	app_entry_t app_reset_handler = (app_entry_t)app_reset_pc;
	__disable_irq();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL  = 0;
	SCB->VTOR = BOOT_START_ADDR; //update table vector
	__set_MSP(app_msp); //apload the boot
	HAL_DeInit(); //stops resets/deletes all IRQ

	app_reset_handler(); //jump to boot
	while(1);
}



