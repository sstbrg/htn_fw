#include <string.h>
#include "flash_handler.h"
#include "main.h"

HAL_StatusTypeDef Flash_Write(uint32_t address, uint64_t *data, uint32_t num_of_quadwords)
{
	HAL_StatusTypeDef status;
	uint32_t i;

	// Unlock Flash
	HAL_FLASH_Unlock();

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
	}
	// Lock Flash
	HAL_FLASH_Lock();

	Update_Cash_Mem();	// reads of the page go through ICACHE: show the new data now, not after a reset
	return status;
}

HAL_StatusTypeDef Flash_Erase(uint32_t bank, uint32_t page){

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
		FLASH_PageErase(page, bank);
		/* Wait for last operation to be completed */
		status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

		/* If the erase operation is completed, disable the associated bits */
		CLEAR_BIT((*reg_cr), (FLASH_NSCR_PER) & (~(FLASH_NON_SECURE_MASK)));
	}
	// Lock Flash
	HAL_FLASH_Lock();

	return status;
}

// Self-id page slot write (0 id, 1/2 antenna delays, 3 crystal trim): page copied, one slot replaced.
// Returns 0 on success; writes nothing when the slot already holds the value.
uint8_t Flash_SelfIdSet(uint8_t slot, uint16_t value) {
	uint16_t page[8];
	memcpy(page, (const void*)FLASH_SELF_ID_ADDRESS, sizeof(page));
	if (slot >= 8)
		return 1;
	if (page[slot] == value)
		return 0;
	page[slot] = value;
	if (Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK)
		return 1;
	return Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)page, 1) != HAL_OK;
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

//	NVIC_DisableIRQ(EXTI4_IRQn);
//	NVIC_DisableIRQ(TIM4_IRQn);
//
//	NVIC_ClearPendingIRQ(EXTI4_IRQn);
//	NVIC_ClearPendingIRQ(TIM4_IRQn);


	__set_MSP(app_msp); //apload the boot

	HAL_DeInit();

	app_reset_handler(); //jump to boot
	while(1);
	//HAL_NVIC_SystemReset();
}
