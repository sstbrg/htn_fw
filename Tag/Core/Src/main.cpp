/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "crc.h"
#include "flash.h"
#include "gpdma.h"
#include "i2c.h"
#include "icache.h"
#include "memorymap.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dw1000.h"
#include "Led.h"
#include "../Lora/Inc/Lora.hpp"
#include "Power.h"
#include "flash_handler.h"
#include "stm32u5xx_hal_usart.h"
#include "uart_communication.h"
#include "burn_mode.h"
#include "fota.h"
#include <string.h>
#include "BMI088.h"
#include "stm32u5xx_it.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


#define WITHOUT_WAKEUP

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
LoraRxInfo_t lora_rxInfo =
{
		.lora_wakeup_buffer = {0} , .buffer_length = 0, .rssi = 0,		// average rssi
				.snr = 0, .RxTOsCounter = 0, //Rx Time Outs Counter
				.new_data = false, .wakeup_detected = false, .no_lora = false, .tx_counter = 0, .freq = 0,
				.wakeup_cycle_enable = true};
extern DwmInfo_t dwmInfo;


uint8_t RxLen;
S_UART_MESSAGE msg;
S_UART_MESSAGE reply;

uint8_t HW_Version;
bool wakeup_switch = true;

void printStt(void)
{
	/*	uint8_t buff[100] = { 0 };
	 uint8_t len =
	 snprintf((char*) buff, sizeof(buff),
	 "*T%x:%s:%s:Dw%d:Lr%d(%d)#\n\r0x%08X &\r\n0x%08X\r\n----------\r\n0x%08X\r\nRxTO:%d\r\nTx:%d, %d\r\n",
	 SELF_ID, VERSION, HW_VER, dwmInfo.status,
	 lora_rxInfo.status, lora_msg_index, dwmInfo.statusReg,
	 (SYS_STATUS_ALL_RX_TO | SYS_STATUS_RXFCG
	 | SYS_STATUS_ALL_RX_ERR),
	 dwmInfo.statusReg
	 & (SYS_STATUS_ALL_RX_TO | SYS_STATUS_RXFCG
	 | SYS_STATUS_ALL_RX_ERR),
	 dwmInfo.RxToCounter, lora_rxInfo.no_lora,
	 lora_rxInfo.tx_counter);
	 HAL_UART_Transmit(&huart1, buff, len, 100);*/
}

void print(const char *s)
{
	//HAL_UART_Transmit(&huart3, (uint8_t*) s, strlen(s), 100);
}

void printP(const char *s, const char *p)
{
//	uint8_t buff[100] =
//	{ 0 };
//	uint8_t len = snprintf((char*) buff, sizeof(buff), s, p);
//
//	HAL_UART_Transmit(&huart1, buff, len, 100);
}

// Self-id flash page: slot 0 tag id, 1 TX antenna delay, 2 RX antenna delay.
// Rewrites the page with slots 1-2 replaced; returns 0 on success.
static uint8_t Save_Antenna_Delays(uint16_t tx, uint16_t rx) {
	uint16_t page[8];
	memcpy(page, (const void*)FLASH_SELF_ID_ADDRESS, sizeof(page));
	page[1] = tx;
	page[2] = rx;
	if (Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK)
		return 1;
	return Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)page, 1) != HAL_OK;
}

// UART_OPCODE_ANT_DELAY: {tx u16, rx u16} sets (0/0xFFFF = back to the default), empty = read.
// Reply {status, tx u16, rx u16} with the values in flash (0xFFFF = default in use).
static void Antenna_Delay_Command(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply) {
	static uint8_t r[8];
	uint8_t status = 0;
	(void)reply;
	if (msg->header.length >= sizeof(S_HEADER) + 4 + 1) {
		uint16_t tx, rx;
		memcpy(&tx, &msg->buff[0], 2);
		memcpy(&rx, &msg->buff[2], 2);
		status = Save_Antenna_Delays(tx, rx);
		dwApplyAntennaDelays();
	}
	const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;
	r[0] = msg->header.opcode;
	r[1] = sizeof(r);
	r[2] = status;
	memcpy(&r[3], &page[1], 2);
	memcpy(&r[5], &page[2], 2);
	r[7] = checksum(r, sizeof(r));
	UART_SendMessage(r, sizeof(r));
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	SCB->VTOR = 0x08200000;
	__enable_irq();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPDMA1_Init();
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_ICACHE_Init();
  MX_SPI3_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_SPI2_Init();
  MX_RTC_Init();
  MX_USART6_UART_Init();
  MX_USART3_UART_Init();
  MX_ADC4_Init();
  MX_FLASH_Init();
  MX_CRC_Init();
  MX_I2C2_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  CRC_File_Handler(); //create header file flash if it doesnt exist
  main_init();

  Led_show();
  BMI088_Init(&bmi,&hspi1,IMU_SPI1_CS_GPIO_Port,IMU_SPI1_CS_Pin,IMU_NCS_GYR_GPIO_Port, IMU_NCS_GYR_Pin);


  print("\n\n\n");
  print("----------------------\r\n");
  printP("	Tag Version: %s \r\n", VERSION);
  print("----------------------\r\n");
  print("\n\n\n");


	status_bits errors = {0};

	if (dwmInit())
	{
		dwConfig(&config);
	}

	HAL_TIM_Base_Start_IT(&htim4);
	Enable_Wakeup_Pin(PWR_WAKEUP_PIN1_HIGH_2);
	// Power up LEDs


	Lora_init();
	if(lora_rxInfo.no_lora == false)
	{
		#ifdef WITHOUT_WAKEUP
			RunTXStateMachine(errors);
			lora_rxInfo.wakeup_detected = true;
		#else
			Lora_Operation_RX(errors);
		#endif // WITHOUT_WAKEUP
	}

	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_SBF);  // Clear Standby flag
	__HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG1); // Clear wake-up flag
	errors.bits.no_LoRa  = lora_rxInfo.no_lora;
	errors.bits.no_UWB = dwmInfo.no_uwb;
	Led_Power_Up_State(dwmInfo.no_uwb ,lora_rxInfo.no_lora);

	#ifdef WITHOUT_WAKEUP
		PWR_SetTimeOut(PWR_Duration_NEVER);
	#else
		PWR_SetTimeOut(PWR_Duration_PowerUpSleepDuration);
	#endif



	/***********************
	 *
	 * 		Main Operational Loop
	 *
	 */
	while  (1)
	{
		refresh_WD();
		fota_poll();

		//	UWB

		if (dwmInfo.no_uwb == false && !fota_busy())
		{
			if (dwRange())
			{
				Link_Led(GREEN);
			}
		}
		//	LoRa
		if (lora_rxInfo.no_lora == false && !fota_busy()){
			if (lora_rxInfo.wakeup_detected == true){

				if (Lora_TXidle() && !PWR_NeedToSleep())
				{
					RunTXStateMachine(errors);
				}
			}
			else
			{
				Lora_Operation_RX(errors);
			}
		}

		//	UART
		RxLen = UART_GetMessage((uint8_t*) &msg, sizeof(msg));
		if ((RxLen != RX_EMPTY) && Validate_UART(&msg, RxLen))
		{
			PWR_SetTimeOut(PWR_Duration_LabDuration);

			switch (msg.header.opcode)
			{
			case UART_OPCODE_BURN: {
				UART_Reply_Burn_msg(&msg,&reply);
				break;
			}
			case UART_OPCODE_SET_ID:
			{
//				uint16_t new_id = msg.set_id_req.new_id;
				const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;	// keep the antenna delays
				uint16_t new_id[8] = {msg.set_id_req.new_id,page[1],page[2],0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};

				//uint64_t Data = msg.set_id_req.new_id;
				uint8_t error;
				error = Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK;
				if (error){
					UART_Reply_Set_Self_ID(&msg, &reply, error);
					break;
				}
				error = Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)&new_id, 1) != HAL_OK;
				if (error){
					UART_Reply_Set_Self_ID(&msg, &reply, error);
					break;
				}
				UART_Reply_Set_Self_ID(&msg, &reply, error);
				break;
			}
			case UART_OPCODE_STATUS:
			{
				dwmStatus();
				Lora_Status();
				UART_Reply_Status(&msg,&reply);
				break;
			}
			case UART_OPCODE_ANT_DELAY:
			{
				Antenna_Delay_Command(&msg, &reply);
				break;
			}
			case UART_OPCODE_LOOPBACK:
			{
				UART_SendMessage(&msg, msg.header.length);
				break;
			}

			case UART_OPCODE_READ_UWB_RAW:{
				UART_Reply_UWB_Raw(&msg, &reply);
				break;
			}
			case UART_OPCODE_RESET:
			{
				UART_Reply_Status(&msg,&reply);
				HAL_Delay(1000);
				HAL_NVIC_SystemReset();
				break;
			}

			case UART_OPCODE_UPDATE_FW:
			{
				UART_Reply_UPDATE_FW(&msg,&reply);
				HAL_Delay(50);

				write_bkup_sram();
				//*(uint32_t*)(SRAM_BKUP_REGISTER+4) = 0;
				//uint32_t read_reg = *(uint32_t*)SRAM_BKUP_REGISTER;
				if (Update_FW_Boot_Flag()){
					Jump_To_Boot();
				}
				break;
			}

			default:
				if (!fota_uart_command((uint8_t*) &msg, RxLen))
					UART_Reply_error(&msg, &reply);
				break;
			}
		}
		//	LED
		Led_Maintanence();

#ifndef WITHOUT_WAKEUP
		{	//	Low power handling
			if(Lora_TXidle() && PWR_NeedToSleep() && !fota_keep_awake()){
				lora_rxInfo.wakeup_cycle_enable = true;
				PWR_Sleep();
				wakeup_switch = false;
			}
		}
#endif // WITHOUT_WAKEUP
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV128;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 13;
  RCC_OscInitStruct.PLL.PLLP = 8;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* EXTI4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
  /* TIM4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
  /* GPDMA1_Channel0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
}

/* USER CODE BEGIN 4 */


VER_PIN RevC1[] = {
		{GPIOE, GPIO_PIN_1},	// VER1_0
		{GPIOE, GPIO_PIN_2},	// VER1_1
		{GPIOE, GPIO_PIN_3},	// VER1_2
};

VER_PIN RevC2[] = {
		{GPIOC, GPIO_PIN_6},	// VER2_0
		{GPIOC, GPIO_PIN_7},	// VER2_1
		{GPIOC, GPIO_PIN_8},	// VER2_2
};
void read_hw_version(void){
	HW_Version = 0;
	HW_Version |= READ_HW_PIN(RevC1[VER_R0],VER_R0) | READ_HW_PIN(RevC2[VER_R0],VER_R0);
	HW_Version |= READ_HW_PIN(RevC1[VER_R1],VER_R1) | READ_HW_PIN(RevC2[VER_R1],VER_R1);
	HW_Version |= READ_HW_PIN(RevC1[VER_R2],VER_R2) | READ_HW_PIN(RevC2[VER_R2],VER_R2);
}

void main_init(){
	write_clear_bkup_sram();
	read_hw_version();
	Init_Leds(HW_Version);
}

void write_clear_bkup_sram(void){
	*(uint32_t*)(SRAM_BKUP_REGISTER) = SRAM_BKUP_REGISTER_CLEAR;
}

void write_bkup_sram(void){
	*(uint32_t*)(SRAM_BKUP_REGISTER) = UPDATE_FW_MODE;
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08200000;
  MPU_InitStruct.LimitAddress = 0x083FDFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region 1 and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x20000000;
  MPU_InitStruct.LimitAddress = 0x2026FFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER1;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

  /** Initializes and configures the Attribute 1 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER1;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_WRITE_BACK|MPU_TRANSIENT
                              |MPU_RW_ALLOCATE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
