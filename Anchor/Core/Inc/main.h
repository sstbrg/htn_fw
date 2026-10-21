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

#include "BMI088.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "flash_handler.h"

//#include "uart_communication.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/*
 * 1.5.3	-	added Get Status command from pc: 0xFE on COM
 * C1.6.01	-	1.	changed spi2 clock config.
 * 				2.	added dwmInfo.uart_income == true to uart3 irq catch to stop dwScanMode()
 * C1.6.02 	-	added missing opcode parts after merge
 * C1.6.03	-	1.	changed self_id parsing. 16:2 - PC_ID, 1:0 - anchor_id.
 * 				2.	anchor id is read from gpio upon power up.
 * 				3.	changed pc_id value doesnt requires stm reset anymore.
 * C1.6.05	-	changed BackOff, TagImposter pc_id and anchor_id split and parsing to twr arrays
 * C1.6.06	-	1.changed anchor struct for twr messages
 * 				2.changed SELECTOR_STAT gpio direction to input.
 * C1.6.07	-	fixed DMA disappearing from ioc.
 * C1.6.08	-	fixed id parsing in lora
 * C1.6.09		1. added rtc
 * 				2. added iwdg for 2 sec
 * 				3. added more hardfault-s handling with nvic(reset)
 * 				4. fixe dma tx
 * 				5. set SELECTOR_STAT changed to input
 * 				6. fix hard-fult issue (UWB read overflow)
 * C1.6.10	-	1. dwScan (Backoff) change to msg struct instead of byte buff (Caused error little/big endian)
 *              2. some cleanup
 * C1.6.11  -   changed dw timeout to 6ms and added debug leds.
 * C1.6.12  -   1. Anchor BIT(Imposter mode) fixed lora freq and buffer size.
 * 				2. dw fixed CIR index
 * 				3. changed ranged frame_len comper size.
 * C1.6.13	-	fixed tag_imposter lora tx and rx sequence.
 * C1.6.14		added TC_PGDLY program to channel 2 value
 * C1.6.15		increased dw timeout
 * C2.0.0		New Era of hardware, REVC2 entered the chat
 * C2.0.1		configured shared ioc for Rev_C1 and Rev_C2
 * C2.0.2		1. 	fixed UART3 missing receive parts.
 * 				2.	moved to HSE clock
 * C2.0.03		added hw version to status
 * C2.0.04		fixed diagnostics data being erased
 * C2.0.05		added attention to no_lora status
 * C2.0.07		1. Clean code
 * 				2. added adc in ioc
 * C2.0.08		modifed dwRange polling approach
 * C2.0.09		returned lost wakeup seq
 * C2.0.10		added attention to no_dw at wakeup
 * C2.0.12		changed bw for tag ids
 * C2.0.13		1. fixed test of no_lora status
 *  			2. fixed missing tag_no_lora report to pc
 *  			3. changed lora id bw span between ids
 *  			4. added lora tx timeout
 * C2.0.14		added lora checksum to lora icd
 * C2.0.15		changed lora bit frequency source anchor id to self_if
 * C2.0.16		changed dw gain value.
 * C2.0.17		adjusted version to tag
 * C2.0.18		adjusted version to tag
 * C3.0.00		new world of updating over the wire
 * C3.0.05		bootloader application
 * C3.0.08		adjusted version to tag
 * C3.0.09		[added] rtc memory access like tag config and split flash to 2 banks.
 * C3.0.10		FIX id programming
 * C3.0.11		[changed] self id flash address
 * C3.0.13		adjusted version to bootloader
 * C3.0.14		adjusted version to bootloader
 * C3.0.15		[changed] Masserase function name
 * C3.0.16		[add] CRC & length validation
 * C3.0.17		adjusted version to bootloader
 * C3.0.18		[add] status command in bootloader
 * C3.0.19		[add] battery level to status message
 * C3.0.20		[fixed] header file address
 * C3.0.21		[ADD] CRC file handler to applications
 * C3.0.22		[FIX] calc CRC on all 254 pages
 * C3.0.23		[changed] Anchor Lora timeout duration
 * C3.0.24		[ADD] NVIC reset boot after 1 minute
 * C3.0.25		adjusted version to bootloader
 * C3.0.26		[changed] amount of pages to  erase, from 254 to 255(max 256)
 * C3.0.27		[added] UART_OPCODE_SEARCH_TWR can be requested as ONLY LORA.
 * C3.0.28		adjusted version to tag
 * C3.0.29		[fixed] missing crc in ioc
 * C3.0.30		adjusted version to tag
 * C3.0.31		[fixed] lora rx holding valid_Data of previous id request causing race condition.
 * C3.0.32		[added] computability with revD
 * C3.0.33		[added] disabled fine grain tx sequencing.
 * C3.0.34		merged Operational_diable_finegrain into Operational
 * C3.0.35		[added] dw internal power config according to hardware version(D = maxpower, C = 10db)
 * C3.0.36		[fixed] id changed twice crashing app
 * C3.0.40      [ADDED] anchor Battery Indicator by anchor id 3 f
 * C3.0.41		[ADDED] error bits to BIT Control(imposter) reply.
 * C3.0.42		[ADDED] (not used),when asking to enable diagnostics to enable accumem clockS.
 * T3.0.51		[changed] config to work with 110K, no backoff, main.c line 365
 * 				[changed] imposter tx delay to 5000.
 * 				[added] commented function to make sure the clocks are enabled as required
 * T3.0.53		[changed] uncommented function to make sure the clocks are enabled as required
 * T3.0.54		[added] NTM_NEW to testing. enabled gpiolcocks on demand. reset ALL_RX_GOOD registers each range.
 * C3.0.55		[Changed] PR1024 define name  to LONG_PREAM
 * C3.0.56		adjusted version to tag
 * C3.0.62		[fixed] ioc file generate, flash files and flash boot_address define. fixed MemoryManagmentTool setup
 * C3.0.63		adjusted version to tag.
 * C3.0.64		[fixed] upload bug where SRAM_BKUP_REGISTER would keep update flag value after reset
 * C3.0.65		[added] reset counter report
 * C3.0.66		[added] BackOFF(dwScanMode) ctrl bit. 1 = enable, 0  = disable. default enable
 */

#define VERSION "sC3.0.67"


#define IS_TAG false

#define LORA_RX_BUFFER_SIZE 6

//writing to rtc
#define SRAM_BKUP_REGISTER  0X40036400
#define SRAM_BKUP_REGISTER_CLEAR 0

//#define DEBUG_RESET_COUNTER_IN_FLASH True

typedef union {
    uint8_t all;

    struct {
        uint8_t checksum : 1;      //	0 (  1): checksum error
        uint8_t flash_error : 1;   //	1 (  2): flash error
        uint8_t no_LoRa : 1;       //	2 (  4): no Lora
        uint8_t no_UWB : 1;        //	3 (  8): no UWB
        uint8_t UWB_Backoff : 1;   //	4 ( 16): UWB backoff
        uint8_t no_tag_key : 1;    //	5 ( 32): no tag key
        uint8_t wrong_UWB_ID : 1;  //	6 ( 64): wrong uwb return index
        uint8_t no_IMU : 1;        //	7 (128): no IMU
    } bits;
} status_bits;

typedef struct {
    bool no_lora;
    uint8_t raw_buffer[LORA_RX_BUFFER_SIZE];
    uint8_t lora_msg_index;
    uint8_t buffer_length;
    int8_t rssi;  // average rssi
    int8_t snr;
    uint16_t tag_id;
    uint16_t tag_id_msg_valid; // used to checkout if the twr new id request is valid with tag_id_msg_valid anchor is already holding.
    uint8_t tag_soc;
    status_bits tag_errors;
    uint8_t new_data;
    int8_t TxCounter;
    uint8_t TagStatusUwb;
    uint32_t RxCounter;
    uint32_t freq;
    uint8_t anchor_id;
    uint16_t mine_pc_id;
    uint16_t self_id;
} LoraRxInfo_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t msg_index;
    uint16_t tag_id;
    uint8_t soc;
    status_bits tag_errors;
	uint8_t checksum;
} sLoRaMsg;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t anchor_id;
    uint16_t tag_scrambled_id;
    uint8_t spare;
	uint8_t checksum;
} sLoRaWakeupMsg;
#pragma pack(pop)

typedef struct {
    uint8_t anchor_id;
    dwt_rxdiag_t diagnostics;
    dwt_deviceentcnts_t counter;
    bool no_uwb;
    uint16_t tag_dest;
    double tof;
    uint32_t distance;
    uint8_t dw_ready;
    bool wrong_ID;
    uint8_t tag_no_lora;
    uint32_t statusReg;
    uint32_t auxData;
    uint32_t RxToCounter;
    bool detec_pc_id;
    uint16_t mine_pc_id;
    uint32_t backoff_timer;
    uint32_t TxCounter;
    uint32_t RxCounter;
    bool uart_income;
    bool tag_imposter;
} DwmInfo_t;

typedef struct {
    uint8_t rf_info[RX_FINFO_LEN];
    uint8_t rf_fqual[RX_FQUAL_LEN];
    uint8_t rf_ttcki_id[RX_TTCKI_LEN];
    uint8_t rf_ttcko_id[RX_TTCKO_LEN];
} DwmExtra_data;

extern uint32_t _1ms_tick;
enum tag_wakeup {
    /*0*/ WAKEUP_START,
    /*1*/ WAKEUP_WAIT_FOR_TX_IDLE
};
typedef struct {
    GPIO_TypeDef *GPIO_Port;
    uint16_t GPIO_Pin;
} VER_PIN;

#define PIN(LED) Leds_GPIO[LED].GPIO_Pin
#define READ_HW_PIN(VER_PIN, BIT) (!HAL_GPIO_ReadPin(VER_PIN.GPIO_Port, VER_PIN.GPIO_Pin) << BIT)
enum {
    VER_R0,
    VER_R1,
    VER_R2,
};

void main_init();
void write_clear_bkup_sram(void);
void write_bkup_sram(void);
void SetAnchor_cable_id();
void SetAnchor_self_id();
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

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
#define RRC_I2C2_SCL_ID1_Pin GPIO_PIN_13
#define RRC_I2C2_SCL_ID1_GPIO_Port GPIOB
#define RRC_I2C2_SDA_ID0_Pin GPIO_PIN_14
#define RRC_I2C2_SDA_ID0_GPIO_Port GPIOB
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
void print(const char *s);
void printP(const char *s, const char *p);
void read_hw_version(void);

typedef struct{
	uint32_t crc;
	uint32_t len;
}HEADER_FILE;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
