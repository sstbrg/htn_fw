/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <usart.h>

#include "BMI088.h"
#include "cachel1_armv7.h"
#include "deca_device_api.h"
#include "flash_handler.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/*
 * C1.6.03	-	1.changed twr id order in msg, big-endian to little-endian.
 * 				2.placed twr msg into struct format
 * C1.6.04	-	1. set SELECTOR_STAT to input
 * 				2. added rtc
 * 				3. added iwdg to 2sec
 * 				4. fix hard-fult issue (UWB read overflow)
 * C1.6.10  -   dwRange Cleanup
 * C1.6.11  -   removed watch dog and added leds debug
 * C1.6.12	-	changed frame_len checking to fixed size.(anchor size)
 * C1.6.14		added TC_PGDLY program to channel 2 value
 * C1.6.15		increased dw timeout
 * C2.0.1		1.	configured shared ioc for Rev_C1 and Rev_C2
 * 				2. WD based on TIM4
 *	C2.0.2		1. 	fixed UART3 missing receive parts.
 *				2.	added reading main_init to read HW_version
 *				3.	added leds init moved to main_init
 *				4.	added daul uart
 *	C2.0.03		fixed missing UART3 parts
 *	C2.0.04		changed clock source to HSE
 *	C2.0.05		1. changed BIG_ANT define to PATCH_ANT
 *				2. added status sending hw version
 *	C2.0.06		1. Clean code
 *				2. added adc in ioc
 *	C2.0.07		added attention to no_lora status
 * 	C2.0.08		modifed dwRange polling approach
 * 	C2.0.12		1. changed bw for tag ids
 * 				2. fixed lora icd anchor id
 *  C2.0.13		1. fixed test of no_lora status
 *  			2. changed lora id bw span between ids
 *  			3. added lora tx timeout
 * C2.0.14		added lora checksum to lora icd
 * C2.0.15		1. raised version to match anchor before merge
 * 				------ after merge added*****
 * 				2. fixed bug return radio flags. tx timeout to false after transmit lora
 * 				3. Lora battery led blink at 25% at switch on.
 * C2.0.16		changed dw gain value.
 * C2.0.17		fixed lora cant wake up on repeat
 * C2.0.18		fixed lora tx freezes in "without_wakeup"
 * ---------------------------------------------------
 * C3.0.00		new world of updating over the wire
 * C3.0.05		bootloader application
 * C3.0.07		fixed bootloader uart issue, caused tag to reset after X time.
 * C3.0.08		[fixed] crashing, it was htim4 DIY watchdog
 * C3.0.09		adjusted version to anchor
 * C3.0.10		FIX id programming
 * C3.0.11		[changed] self id flash address
 * C3.0.12		adjusted version to bootloader
 * C3.0.13		adjusted version to bootloader
 * C3.0.14		adjusted version to bootloader
 * C3.0.15		[changed] Masserase function name
 * C3.0.16		[add] CRC & length validation
 * C3.0.17		adjusted version to bootloader
 * C3.0.18		[add] status command in bootloader
 * C3.0.19		[add] battery level to status message
 * C3.0.20		adjusted version to bootloader and anchor
 * C3.0.21		[ADD] CRC file handler to applications
 * C3.0.22		[FIX] calc CRC on all 254 pages
 * C3.0.23		[changed] Anchor Lora timeout duration
 * C3.0.24		[ADD] nvic reset boot after 1 minute
 * C3.0.25		adjusted version to bootloader
 * C3.0.26		[changed] amount of pages to  erase, from 254 to 255(max 256)
 * C3.0.27		[changed] PWR_Duration_MyMessage, increased to 15min
 * C3.0.28		[fixed] tag id writing to flash
 * C3.0.29		adjusted version to anchor
 * C3.0.30		1. [fixed] tag rrc missing stm internal pull-up on i2c lines.
 * 				2. [changed] run to sleep time to 15min.
 * C3.0.31		adjusted version to anchor
 * C3.0.32		[added] computability with revD
 * C3.0.33		[added] disabled fine grain tx sequencing.
 * C3.0.34		merged Operational_diable_finegrain into Operational
 * C3.0.35		[added] dw internal power config according to hardware version(D = maxpower, C = 10db)Operational
 * C3.0.36		[changed] changed
 * C3.0.40      adjusted version to anchor
 * C3.0.41      adjusted version to anchor
 * C3.0.42      adjusted version to anchor
 * C3.0.55		[changed] DW configurations for rev D
 * C3.0.56		[changed] dw rx buffer mask set to 1023
 * C3.0.57		[changed] MCU_STD_BYPS_Pin to SET, enabling RF chain.
 * C3.0.58      [removed] #pragma GCC optimize ("O3") from deca_spi
 * C3.0.59 	`	[changed] timing. increases, and txantena delay
 * C3.0.60 		[changed] manually spi source clock
 * C3.0.61 		[changed] back timings delays
 * C3.0.62		[fixed] ioc file generate and flash boot_address define. fixed MemoryManagmentTool setup
 * C3.0.63		[fixed] false positive error on lora status.
 * C3.0.64		[fixed] upload bug where SRAM_BKUP_REGISTER would keep update flag value after reset
 * C3.0.65		adjusted version to anchor
 * C3.0.66		adjusted version to anchor
 * C3.0.67		adjusted version to anchor
 * tag3.1.0	[added] FOTA over UWB (fota.c): FOTA flasher mode via UART opcodes 0x40-0x47,
 * 				FOTA target in dwRange. Image received into RAM, CRC checked, written from RAM.
 *
 */

#define VERSION "tag3.1.0"

#define IS_TAG true

#pragma pack(push, 1)
typedef struct {
    uint16_t anchor_ids; // PC_ID (14 msb bits) + ANCHOR_ID (2 lsb bits)
    uint16_t scrambeld_self_id;
    uint8_t spare;
    uint8_t checksum;
} sLoRaWakeupMsg;
#pragma pack(pop)

#define LORA_RX_BUFFER_SIZE sizeof(sLoRaWakeupMsg)

typedef struct {
	sLoRaWakeupMsg lora_wakeup_buffer;
    uint8_t buffer_length;
    int8_t rssi;  // average rssi
    int8_t snr;
    int8_t RxTOsCounter;  // Rx Time Outs Counter
    bool new_data;
    bool wakeup_detected;
    bool no_lora;
    uint32_t tx_counter;
    uint32_t rx_counter;
    uint32_t freq;
    uint16_t mine_id;
    bool wakeup_cycle_enable;
} LoraRxInfo_t;

typedef struct {
    dwt_rxdiag_t disagnostics;
    uint8_t no_uwb;
    uint8_t anchor_dest;
    bool dw_ready;
    bool tag_no_lora;
    uint32_t statusReg;
    uint32_t RxToCounter;
    uint32_t backoff_timer;
    uint32_t TxCounter;
    uint32_t RxCounter;
    bool uart_income;
} DwmInfo_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t checksum : 1;     //	0: checksum error
        uint8_t flash_error : 1;  //	1: flash error
        uint8_t no_LoRa : 1;      //	2: no Lora
        uint8_t no_UWB : 1;       //	3: no UWB
        uint8_t UWB_Backoff : 1;  //	4: UWB backoff
        uint8_t no_tag_key : 1;   //	5: no tag key
        uint8_t res7 : 1;         //	7: RESu
        uint8_t no_IMU : 1;       //	5: no IMU
    } bits;
} status_bits;


typedef struct{
	GPIO_TypeDef* GPIO_Port;
	uint16_t GPIO_Pin;
}VER_PIN;

#define PIN(LED)	Leds_GPIO[LED].GPIO_Pin
#define READ_HW_PIN(VER_PIN,BIT)	(!HAL_GPIO_ReadPin(VER_PIN.GPIO_Port, VER_PIN.GPIO_Pin) << BIT)
enum
{
	VER_R0,
	VER_R1,
	VER_R2,
};

#define SRAM_BKUP_REGISTER  0X40036400
#define SRAM_BKUP_REGISTER_CLEAR 0

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define UWB_TIMEOUT (10 * 60 * 1000)  // 10 minutes

// FLASH_FREE_ADDRESS  	(FLASH_KEY_ADDRESS + 2*QUAD_WORD)

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VER1_R1_Pin GPIO_PIN_2
#define VER1_R1_GPIO_Port GPIOE
#define VER1_R2_Pin GPIO_PIN_3
#define VER1_R2_GPIO_Port GPIOE
#define SX_DIO1_Pin GPIO_PIN_4
#define SX_DIO1_GPIO_Port GPIOE
#define SX_DIO1_EXTI_IRQn EXTI4_IRQn
#define LED_R_Pin GPIO_PIN_5
#define LED_R_GPIO_Port GPIOE
#define LED_G_Pin GPIO_PIN_6
#define LED_G_GPIO_Port GPIOE
#define SX_BUSY_Pin GPIO_PIN_13
#define SX_BUSY_GPIO_Port GPIOC
#define LED_B_Pin GPIO_PIN_8
#define LED_B_GPIO_Port GPIOF
#define FTDI_UART6_NPWREN_Pin GPIO_PIN_1
#define FTDI_UART6_NPWREN_GPIO_Port GPIOC
#define FTDI_UART6_RX_Pin GPIO_PIN_2
#define FTDI_UART6_RX_GPIO_Port GPIOC
#define FTDI_UART6_TX_Pin GPIO_PIN_3
#define FTDI_UART6_TX_GPIO_Port GPIOC
#define IMU_INT_ACC_Pin GPIO_PIN_1
#define IMU_INT_ACC_GPIO_Port GPIOA
#define IMU_INT_GYR_Pin GPIO_PIN_2
#define IMU_INT_GYR_GPIO_Port GPIOA
#define IMU_SPI1_CS_Pin GPIO_PIN_4
#define IMU_SPI1_CS_GPIO_Port GPIOA
#define IMU_SPI1_SCK_Pin GPIO_PIN_5
#define IMU_SPI1_SCK_GPIO_Port GPIOA
#define IMU_SPI1_MISO_Pin GPIO_PIN_6
#define IMU_SPI1_MISO_GPIO_Port GPIOA
#define IMU_SPI1_MOSI_Pin GPIO_PIN_7
#define IMU_SPI1_MOSI_GPIO_Port GPIOA
#define IMU_NCS_GYR_Pin GPIO_PIN_0
#define IMU_NCS_GYR_GPIO_Port GPIOB
#define SELECTOR_STAT_Pin GPIO_PIN_10
#define SELECTOR_STAT_GPIO_Port GPIOB
#define RRC_I2C2_SCL_Pin GPIO_PIN_13
#define RRC_I2C2_SCL_GPIO_Port GPIOB
#define RRC_I2C2_SDA_Pin GPIO_PIN_14
#define RRC_I2C2_SDA_GPIO_Port GPIOB
#define FTDI_UART3_TX_Pin GPIO_PIN_8
#define FTDI_UART3_TX_GPIO_Port GPIOD
#define FTDI_UART3_RX_Pin GPIO_PIN_9
#define FTDI_UART3_RX_GPIO_Port GPIOD
#define FTDI_UART3_NPWREN_Pin GPIO_PIN_10
#define FTDI_UART3_NPWREN_GPIO_Port GPIOD
#define VER2_R0_Pin GPIO_PIN_6
#define VER2_R0_GPIO_Port GPIOC
#define VER2_R1_Pin GPIO_PIN_7
#define VER2_R1_GPIO_Port GPIOC
#define VER2_R2_Pin GPIO_PIN_8
#define VER2_R2_GPIO_Port GPIOC
#define SX_LED_RX_Pin GPIO_PIN_9
#define SX_LED_RX_GPIO_Port GPIOC
#define SX_LED_TX_Pin GPIO_PIN_8
#define SX_LED_TX_GPIO_Port GPIOA
#define SX_OPT_Pin GPIO_PIN_9
#define SX_OPT_GPIO_Port GPIOA
#define SX_Mode_SX126x_Pin GPIO_PIN_10
#define SX_Mode_SX126x_GPIO_Port GPIOA
#define SX_Mode_FRx_Pin GPIO_PIN_11
#define SX_Mode_FRx_GPIO_Port GPIOA
#define SX_RESET_Pin GPIO_PIN_12
#define SX_RESET_GPIO_Port GPIOA
#define DW_PA_BYPASS_Pin GPIO_PIN_15
#define DW_PA_BYPASS_GPIO_Port GPIOA
#define DW_SPI3_SCK_Pin GPIO_PIN_10
#define DW_SPI3_SCK_GPIO_Port GPIOC
#define DW_SPI3_MISO_Pin GPIO_PIN_11
#define DW_SPI3_MISO_GPIO_Port GPIOC
#define DW_SPI3_MOSI_Pin GPIO_PIN_12
#define DW_SPI3_MOSI_GPIO_Port GPIOC
#define DW_RESET_Pin GPIO_PIN_0
#define DW_RESET_GPIO_Port GPIOD
#define SX_SPI2_SCK_Pin GPIO_PIN_1
#define SX_SPI2_SCK_GPIO_Port GPIOD
#define DW_CS_Pin GPIO_PIN_2
#define DW_CS_GPIO_Port GPIOD
#define SX_SPI2_MISO_Pin GPIO_PIN_3
#define SX_SPI2_MISO_GPIO_Port GPIOD
#define SX_SPI2_MOSI_Pin GPIO_PIN_4
#define SX_SPI2_MOSI_GPIO_Port GPIOD
#define DW_WAKEUP_Pin GPIO_PIN_10
#define DW_WAKEUP_GPIO_Port GPIOG
#define SX_ANT_SW_Pin GPIO_PIN_6
#define SX_ANT_SW_GPIO_Port GPIOB
#define SX_SPI2_CS_Pin GPIO_PIN_7
#define SX_SPI2_CS_GPIO_Port GPIOB
#define DW_IRQ_Pin GPIO_PIN_8
#define DW_IRQ_GPIO_Port GPIOB
#define MCU_STD_N_BYPS_Pin GPIO_PIN_9
#define MCU_STD_N_BYPS_GPIO_Port GPIOB
#define VER1_R0_Pin GPIO_PIN_1
#define VER1_R0_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */
/* Private defines -----------------------------------------------------------*/
#define VBUS_SENSE_Pin GPIO_PIN_2
#define VBUS_SENSE_GPIO_Port GPIOC
#define UCPD_FLT_Pin GPIO_PIN_14
#define UCPD_FLT_GPIO_Port GPIOB
#define UCPD_DBn_Pin GPIO_PIN_5
#define UCPD_DBn_GPIO_Port GPIOB

void main_init();
void read_hw_version(void);
void write_clear_bkup_sram(void);
void write_bkup_sram(void);

typedef struct{
	uint32_t crc;
	uint32_t len;
}HEADER_FILE;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
