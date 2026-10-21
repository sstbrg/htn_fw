/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2016 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian and Matthieu Verdy
*/
#ifndef __SX126x_HAL_CPP__
#define __SX126x_HAL_CPP__

#include "sx126x-hal.hpp"
#include <cstring>

SX126xHal::SX126xHal(SPI_HandleTypeDef *spiHandle, GPIO_TypeDef *nssPort, uint16_t nssPin,
                     GPIO_TypeDef *busyPort, uint16_t busyPin, GPIO_TypeDef *dio1Port, uint16_t dio1Pin,
                     GPIO_TypeDef *dio2Port, uint16_t dio2Pin, GPIO_TypeDef *dio3Port, uint16_t dio3Pin,
                     GPIO_TypeDef *rstPort, uint16_t rstPin, uint32_t freqChannel,
					 uint32_t deviceChannel, GPIO_TypeDef *antSwPort, uint16_t antSwPin,
                     RadioCallbacks_t *callbacks)
    : SX126x(callbacks), hspi(spiHandle), nssPort(nssPort), nssPin(nssPin), busyPort(busyPort), busyPin(busyPin),
      dio1Port(dio1Port), dio1Pin(dio1Pin), dio2Port(dio2Port), dio2Pin(dio2Pin), dio3Port(dio3Port), dio3Pin(dio3Pin),
      rstPort(rstPort), rstPin(rstPin), freqChannel(freqChannel), deviceChannel(deviceChannel),
      antSwPort(antSwPort), antSwPin(antSwPin) {}

SX126xHal::~SX126xHal(void) {}

void SX126xHal::SpiInit(void) {
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

/*
void SX126xHal::IoIrqInit(DioIrqHandler irqHandler) {
// 	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
//  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI11_IRQn, 0, 0); // Set the priority for EXTI11
    HAL_NVIC_EnableIRQ(EXTI11_IRQn); // Enable interrupt for EXTI line 11
}
*/
void SX126xHal::IoIrqInit(DioIrqHandler irqHandler) {
    assert_param(irqHandler != NULL);

    // Configure DIO1 pin with interrupt
    // Enable interrupt for the pin and set the priority
     HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0); // Set the priority for EXTI11
     HAL_NVIC_EnableIRQ(EXTI4_IRQn); // Enable interrupt for EXTI line 11
     this->dio1IrqHandler = irqHandler;
}

void SX126xHal::Reset(void) {
    	HAL_GPIO_WritePin(rstPort, rstPin, GPIO_PIN_RESET);
    	HAL_Delay(50);
    	HAL_GPIO_WritePin(rstPort, rstPin, GPIO_PIN_SET);
    	HAL_Delay(20);
}

void SX126xHal::Wakeup(void) {
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    uint8_t dummy[2] = {RADIO_GET_STATUS, 0};
    HAL_SPI_Transmit(hspi, dummy, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
    WaitOnBusy();
    __enable_irq();
    AntSwOn();
}

void SX126xHal::WriteCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size) {
	WaitOnBusy();
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi, (uint8_t *)&command, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi, buffer, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

void SX126xHal::ReadCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size) {
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi, (uint8_t *)&command, 1, HAL_MAX_DELAY);
    uint8_t dummy = 0;
    HAL_SPI_Transmit(hspi, &dummy, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(hspi, buffer, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

void SX126xHal::WriteRegister(uint16_t address, uint8_t *buffer, uint16_t size) {
	WaitOnBusy();
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    uint8_t header[3] = {RADIO_WRITE_REGISTER, (uint8_t)((address >> 8) & 0xFF), (uint8_t)(address & 0xFF)};
    HAL_SPI_Transmit(hspi, header, 3, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi, buffer, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

void SX126xHal::WriteReg(uint16_t address, uint8_t value) {
    WriteRegister(address, &value, 1);
}

void SX126xHal::ReadRegister(uint16_t address, uint8_t *buffer, uint16_t size) {
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    uint8_t header[3] = {RADIO_READ_REGISTER, (uint8_t)((address >> 8) & 0xFF), (uint8_t)(address & 0xFF)};
    HAL_SPI_Transmit(hspi, header, 3, HAL_MAX_DELAY);
    uint8_t dummy = 0;
    HAL_SPI_Transmit(hspi, &dummy, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(hspi, buffer, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

uint8_t SX126xHal::ReadReg(uint16_t address) {
    uint8_t value;
    ReadRegister(address, &value, 1);
    return value;
}

void SX126xHal::WriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size) {
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    uint8_t header[2] = {RADIO_WRITE_BUFFER, offset};
    HAL_SPI_Transmit(hspi, header, 2, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi, buffer, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

void SX126xHal::ReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size) {
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_RESET);
    uint8_t header[2] = {RADIO_READ_BUFFER, offset};
    HAL_SPI_Transmit(hspi, header, 2, HAL_MAX_DELAY);
    uint8_t dummy = 0;
    HAL_SPI_Transmit(hspi, &dummy, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(hspi, buffer, size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(nssPort, nssPin, GPIO_PIN_SET);
}

uint8_t SX126xHal::GetDioStatus(void) {
    return (HAL_GPIO_ReadPin(dio3Port, dio3Pin) << 3) | (HAL_GPIO_ReadPin(dio2Port, dio2Pin) << 2) |
           (HAL_GPIO_ReadPin(dio1Port, dio1Pin) << 1) | (HAL_GPIO_ReadPin(busyPort, busyPin));
}

uint8_t SX126xHal::GetDeviceType(void) {
        return SX1262;
}

uint8_t SX126xHal::GetFreqSelect(void) {
        return MATCHING_FREQ_868;
}

void SX126xHal::AntSwOn(void) {
    HAL_GPIO_WritePin(antSwPort, antSwPin, GPIO_PIN_SET);
}

void SX126xHal::AntSwOff(void) {
    HAL_GPIO_WritePin(antSwPort, antSwPin, GPIO_PIN_RESET);
}

// Method to invoke the handler
void SX126xHal::InvokeHandler(void) {
	if (this->dio1IrqHandler) { // Check if the handler is set
		(this->*dio1IrqHandler)(); // Call the member function
    } else {
    	printf("No IRQ handler assigned!");
    }
}

#endif // __SX126x_HAL_CPP__
