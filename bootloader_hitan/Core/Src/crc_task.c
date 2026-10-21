#include "stm32u5xx_hal.h"
#include "crc_task.h"
#include "flash.h"

extern CRC_HandleTypeDef hcrc;

extern DMA_HandleTypeDef handle_GPDMA1_Channel4;

//Configure CRC oncr (polynomial,init value, reversals) in MX_CRC_Init() via CubeMX.

#define FLASH_BASE_ADDRS_MAIN_APP 	0x08000000
#define FLASH_END_ADDRS_MAIN_APP 	0x083FFFFF
#define CRC_TIMEOUT_MS 1000

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
//Helper: resets CRC to its INIT value
static inline void CRC_Reset(void) {
	__HAL_CRC_DR_RESET(&hcrc);
//	__HAL_RCC_CRC_FORCE_RESET();
//	__HAL_RCC_CRC_RELEASE_RESET();
}

//computer crc-8 of a flash,range [start addr, stop addr]. byte-aligned.
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

	uint32_t word_count = len / 4U;
	uint32_t tail_bytes = len % 4U;
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

void Clear_Header_File(void){
	Flash_Erase(HEADER_PAGE);
}
