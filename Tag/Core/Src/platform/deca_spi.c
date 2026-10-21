/*! ----------------------------------------------------------------------------
 * @file    deca_spi.c
 * @brief   SPI access functions
 *
 * @attention
 *
 * Copyright 2015 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */
#include "main.h"
#include "../Inc/platform/deca_spi.h"
#include "stm32u5xx_hal_def.h"
#include "../Inc/decadriver/deca_device_api.h"
#include "../Inc/platform/port.h"

extern  SPI_HandleTypeDef hspi3;    /*clocked from 72MHz*/

/****************************************************************************//**
 *
 *                              DW1000 SPI section
 *
 *******************************************************************************/
/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(/*SPI_TypeDef* SPIx*/)
{
    return 0;
} // end openspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void)
{
    return 0;
} // end closespi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospi()
 *
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success
 */
//#pragma GCC optimize ("O3")
int writetospi(uint16_t headerLength,
               const    uint8_t *headerBuffer,
               uint32_t bodyLength,
               const    uint8_t *bodyBuffer)
{
    decaIrqStatus_t  stat ;
    stat = decamutexon() ;

    while (HAL_SPI_GetState(&hspi3) != HAL_SPI_STATE_READY);

    HAL_GPIO_WritePin(DW_CS_GPIO_Port, DW_CS_Pin, GPIO_PIN_RESET);

    HAL_SPI_Transmit(&hspi3, (uint8_t *)&headerBuffer[0], headerLength, HAL_MAX_DELAY);//HAL_MAX_DELAY);    /* Send header in polling mode */
    HAL_SPI_Transmit(&hspi3, (uint8_t *)&bodyBuffer[0], bodyLength, HAL_MAX_DELAY);//HAL_MAX_DELAY);        /* Send data in polling mode */

    HAL_GPIO_WritePin(DW_CS_GPIO_Port, DW_CS_Pin, GPIO_PIN_SET);

    decamutexoff(stat);

    return 0;
} // end writetospi()


/*! ------------------------------------------------------------------------------------------------------------------
 * Function: readfromspi()
 *
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data may be found,
 * or returns 0
 */
//#pragma GCC optimize ("O3")


//#pragma GCC optimize ("O3")
/*
 * DW1000 SPI read body: register-level receive on SPI3 with interrupts off.
 *
 * HAL_SPI_Receive() in a -O0 build cannot drain SPI3's 8-byte RX FIFO at
 * 20 MHz in master receive-only mode: the FIFO overruns after ~23 bytes, the
 * transfer stalls and the rest of the buffer stays zero (any frame or register
 * read longer than ~23 bytes came back truncated). This loop is compiled -O2,
 * runs with interrupts masked and has its own spin timeout instead of the HAL
 * tick (which does not advance with interrupts off).
 */
__attribute__((optimize("O2")))
static void dw_spi_rx(uint8_t *buf, uint32_t len)
{
	SPI_TypeDef *spi = hspi3.Instance;
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	while (len) {
		uint32_t n = len > 1023u ? 1023u : len;	// SPI3 TSIZE is 10 bits
		uint32_t spins = 0;
		CLEAR_BIT(spi->CR1, SPI_CR1_SPE);
		MODIFY_REG(spi->CFG2, SPI_CFG2_COMM, SPI_CFG2_COMM_1);	// simplex receiver
		MODIFY_REG(spi->CR2, SPI_CR2_TSIZE, n);
		SET_BIT(spi->CR1, SPI_CR1_SPE);
		SET_BIT(spi->CR1, SPI_CR1_CSTART);
		for (uint32_t i = 0; i < n;) {
			if (spi->SR & SPI_SR_RXP) {
				buf[i++] = *(volatile uint8_t *)&spi->RXDR;
				spins = 0;
			} else if (++spins > 100000u) {
				break;	// leave the rest zero, as before
			}
		}
		spins = 0;
		while (!(spi->SR & SPI_SR_EOT) && ++spins < 100000u) {
		}
		spi->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_OVRC | SPI_IFCR_UDRC | SPI_IFCR_MODFC | SPI_IFCR_SUSPC;
		CLEAR_BIT(spi->CR1, SPI_CR1_SPE);
		buf += n;
		len -= n;
	}
	__set_PRIMASK(primask);
}

int readfromspi(uint16 headerLength, const uint8 *headerBuffer, uint32 readlength, uint8 *readBuffer)
{
	decaIrqStatus_t stat;
	stat = decamutexon();

	uint8_t headBuf[headerLength]; //make a copy
	for (int i = 0; i < headerLength; ++i)
	{
		headBuf[i] = headerBuffer[i];
	}

	for (int i = 0; i < readlength; ++i)
	{
		readBuffer[i] = 0;
	}

	HAL_GPIO_WritePin(DW_CS_GPIO_Port, DW_CS_Pin, GPIO_PIN_RESET);

	HAL_SPI_Transmit(&hspi3, headBuf, headerLength, 5);
//	HAL_SPI_Transmit(&hspi3, headBuf, headerLength, HAL_MAX_DELAY);
	while(HAL_SPI_GetState(&hspi3) == HAL_SPI_STATE_BUSY) {}

	dw_spi_rx(readBuffer, readlength);

	HAL_GPIO_WritePin(DW_CS_GPIO_Port, DW_CS_Pin, GPIO_PIN_SET);

	decamutexoff(stat);

	return 0;
}

/****************************************************************************//**
 *
 *                              END OF DW1000 SPI section
 *
 *******************************************************************************/

