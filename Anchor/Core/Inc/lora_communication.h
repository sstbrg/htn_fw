
#ifndef LORA_INC_LORA_COMMUNICATION_H_
#define LORA_INC_LORA_COMMUNICATION_H_

#include "stm32u5xx_hal.h"


void WriteRegister_Spi(const SPI_HandleTypeDef* hspi, const uint8_t register_address,
									const uint8_t new_register_value);

uint8_t ReadRegister_Spi(const SPI_HandleTypeDef* hspi, uint8_t register_address);


void WriteBuffer_Spi(const SPI_HandleTypeDef* hspi, const uint8_t register_address,
									const uint8_t* buffer, const uint8_t buffer_lenght);


void ReadBuffer_Spi(const SPI_HandleTypeDef* hspi, const uint8_t register_address,
						 uint8_t* buffer, const uint8_t buffer_lenght);




#endif /* LORA_INC_LORA_COMMUNICATION_H_ */
