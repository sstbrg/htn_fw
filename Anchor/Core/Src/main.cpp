/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "../Lora/Inc/Lora.hpp"
#include "uart_communication.h"
#include "Led.h"
#include "flash_handler.h"
#include "burn_mode.h"
#include "fota.h"
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t cable_id = 0;
uint8_t HW_Version;

uint16_t reset_counter = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void SetAnchor_self_id(){
//	 lora_rxInfo.self_id = ((uint16_t)(((lora_rxInfo.mine_pc_id << 2) & 0xfc) | lora_rxInfo.anchor_id) << 8) | ((uint8_t)((lora_rxInfo.mine_pc_id >> (8+2)) & 0xff));
	lora_rxInfo.self_id = (PC_ID << 2) | lora_rxInfo.anchor_id;
}
void SetAnchor_cable_id(){
	// Anchor id range: 0 ~ 3
	 cable_id = ((HAL_GPIO_ReadPin(RRC_I2C2_SCL_ID1_GPIO_Port, RRC_I2C2_SCL_ID1_Pin)<<1) | HAL_GPIO_ReadPin(RRC_I2C2_SDA_ID0_GPIO_Port, RRC_I2C2_SDA_ID0_Pin));
	 dwmInfo.anchor_id = cable_id;
	 lora_rxInfo.anchor_id = cable_id;
	 SetAnchor_self_id();
}

int gDebug = false;
uint8_t BackOFF_CTRL = true; //BackOFF_CTRL = true -  enables dwScanMode, BackOFF_CTRL = false  - disables dwScanMode

LoraRxInfo_t lora_rxInfo = {.no_lora = true, .lora_msg_index = 0, .buffer_length = 0,
		.rssi = 0,		// average rssi
		.snr = 0,
		.tag_id =0,
		.tag_id_msg_valid = 0,
		.new_data = LR_NO_VALID_DATA, .TxCounter = 0, .TagStatusUwb = 0,
		.RxCounter = 0,
		.anchor_id = 0,
		.mine_pc_id = PC_ID,
		.self_id = 0};

DwmInfo_t dwmInfo = { .anchor_id = 0, .no_uwb = true,  .tag_dest = 0, .tof =
		0, .distance = 0, .dw_ready = DW_NO_VALID_DATA, .wrong_ID=false, .tag_no_lora = 0,
		.RxToCounter = 0, .detec_pc_id = true, .mine_pc_id = PC_ID,
		.backoff_timer = 0, .TxCounter = 0, .RxCounter = 0,
		.uart_income = false, .tag_imposter = false };


extern RadioFlags_t radioFlags;
extern Fault_Counter fault_counter;


status_bits errors = {0};
enum tag_wakeup wakeup_state;

void printStt(void) {
//	uint8_t buff[100] = { 0 };
//	uint8_t len = snprintf((char*) buff, sizeof(buff),
//			"A%d:%s:%s:Dw%d:Lr%d\n\r", dwmInfo.anchor_id, VERSION, HW_VER,
//			dwmInfo.status, lora_rxInfo.status);
//
//	HAL_UART_Transmit(&huart1, buff, len, 100);
}

void print(const char *s) {
	//HAL_UART_Transmit(&huart1, (uint8_t*) s, strlen(s), 100);
}

void printP(const char *s, const char *p) {
//	uint8_t buff[100] = { 0 };
//	uint8_t len = snprintf((char*) buff, sizeof(buff), s, p);
//
//	HAL_UART_Transmit(&huart1, buff, len, 100);
}
// Self-id flash page: slot 0 PC_ID, 1 TX antenna delay, 2 RX antenna delay, 7 reset counter.
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

// UART_OPCODE_READ_CIR_WINDOW: the CIR window latched with the last valid range (same exchange as
// DIAGNOSTIC_STATUS). Reply {fp_index u16 (Q10.6), start u16, n u8, n x (i16 re, i16 im)}; n = 0 before
// the first range.
static void Cir_Window_Command(S_UART_MESSAGE *msg) {
	static uint8_t r[7 + 4 * CIR_WINDOW_N + 1];
	uint8_t n = dwmInfo.cir_n;
	uint8_t len = 7 + 4 * n + 1;
	r[0] = msg->header.opcode;
	r[1] = len;
	memcpy(&r[2], &dwmInfo.cir_fp_index, 2);
	memcpy(&r[4], &dwmInfo.cir_start, 2);
	r[6] = n;
	memcpy(&r[7], &dwmInfo.cir_buf[1], 4 * n);	// skip the accumulator's dummy octet
	r[len - 1] = checksum(r, len);
	UART_SendMessage(r, len);
}

// UART_OPCODE_SET_LINK_FEATURES (doc 9I): LinkCfg_t bytes {features, k_max, lora_target_dbm,
// lora_band_db, uwb_target_dbm, uwb_band_db, profile_max, profile_override, step_interval_ds,
// lora_owner}; a shorter payload sets the leading fields, empty = read. Reply: the configuration.
static void Link_Features_Command(S_UART_MESSAGE *msg) {
	static uint8_t r[2 + sizeof(LinkCfg_t) + 1];
	if (msg->header.length > sizeof(S_HEADER) + 1)
		Link_SetConfig(msg->buff, msg->header.length - sizeof(S_HEADER) - 1);
	r[0] = msg->header.opcode;
	r[1] = sizeof(r);
	memcpy(&r[2], &link_cfg, sizeof(LinkCfg_t));
	r[sizeof(r) - 1] = checksum(r, sizeof(r));
	UART_SendMessage(r, sizeof(r));
}

// UART_OPCODE_XTAL_TRIM: {trim u8} 0..31 saves and applies it, 0xFF clears it (back to the chip's OTP
// trim, or mid-range), empty = read. Self-id page slot 3. Reply {status, saved (0xFF = none), active}.
static void Xtal_Trim_Command(S_UART_MESSAGE *msg) {
	static uint8_t r[6];
	uint8_t status = 0;
	if (msg->header.length >= sizeof(S_HEADER) + 1 + 1) {
		uint8_t t = msg->buff[0];
		if (t <= FS_XTALT_MASK || t == 0xFF) {
			uint16_t page[8];
			memcpy(page, (const void*)FLASH_SELF_ID_ADDRESS, sizeof(page));
			page[3] = (t == 0xFF) ? 0xFFFF : t;
			if (Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK
					|| Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)page, 1) != HAL_OK)
				status = 1;
			if (t == 0xFF) {	// back to what the driver loads at init
				dwmInit();
				dwConfig(&config);
			} else {
				dwt_setxtaltrim(t);
			}
		} else {
			status = 2;		// out of range
		}
	}
	uint16_t saved = ((const uint16_t*)FLASH_SELF_ID_ADDRESS)[3];
	r[0] = msg->header.opcode;
	r[1] = sizeof(r);
	r[2] = status;
	r[3] = (saved <= FS_XTALT_MASK) ? (uint8_t)saved : 0xFF;
	r[4] = dwt_read8bitoffsetreg(FS_CTRL_ID, FS_XTALT_OFFSET) & FS_XTALT_MASK;
	r[5] = checksum(r, sizeof(r));
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
  MX_CRC_Init();
  MX_I2C2_Init();
  MX_FLASH_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
  CRC_File_Handler(); //create header file flash if it doesnt exist
	main_init(); //Init struct

	print("----------------------\r\n");
	printP(" Anchor Version: %s \r\n", VERSION);
	print("----------------------\r\n");

	HAL_TIM_Base_Start_IT(&htim4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	Fault_BackupRegister_read(&fault_counter); //for debug operation
//	wakeup_watchdog(&fault_counter);

	BMI088_Init(&bmi,&hspi1,IMU_SPI1_CS_GPIO_Port,IMU_SPI1_CS_Pin,IMU_NCS_GYR_GPIO_Port, IMU_NCS_GYR_Pin);

  	if (!dwmInit()) { // init failed
  		print("Init Failed\r\n");
  	} else {
  		print("Init DW1000 Succeed\r\n");
  		dwConfig(&config);
  	}

  	Lora_init(&lora_rxInfo, &dwmInfo);
  	if (lora_rxInfo.no_lora == 0) {
  		print("LORA Init Succeed\r\n");
  		Lora_Operation_RX();
  	} else {
  		print("LORA Init Failed\r\n");
  	}
  	errors.bits.no_LoRa  = lora_rxInfo.no_lora;
  	errors.bits.no_UWB = dwmInfo.no_uwb;
	dwmStatus();

	S_UART_MESSAGE msg = { 0 };
	S_UART_MESSAGE reply = { 0 };
	while (1) {
		refresh_WD();
		fota_poll();
		(void)Backoff_Left();	// clears an expired deadline every pass (wrap safety)
		if (!fota_busy())
			dwRecover();

		uint8_t RxLen = UART_GetMessage((uint8_t*) &msg, sizeof(msg));
		if ((RxLen != RX_EMPTY) && Validate_UART(&msg, RxLen)) {
			switch (msg.header.opcode) {
			case UART_OPCODE_BURN: {
				UART_Reply_Burn_msg(&msg,&reply);
				break;
			}

			case UART_OPCODE_SEARCH_TWR: {
				if(dwmInfo.tag_imposter){
					dwmInfo.tag_imposter = false;
				}


				lora_rxInfo.anchor_id = dwmInfo.anchor_id;
				dwmInfo.tag_dest = lora_rxInfo.tag_id = msg.op_twr_req.tag_id & 0x7FFF;

				uint8_t only_lora = (msg.op_twr_req.tag_id & 0x8000? 1:0);

				// optional third payload byte: k_max for this round (9F), 0 = anchor decides
				uint8_t k_req = (msg.header.length >= sizeof(S_HEADER) + 3 + 1) ? msg.buff[2] : 0;
				if ((Backoff_Left() == 0) && (!dwmInfo.no_uwb) && !only_lora) {
					dwmInfo.backoff_timer = 0;
					dwRangeRound(k_req);
				}
				if (!lora_rxInfo.no_lora && (lora_rxInfo.freq != RF_FREQ_TAG(lora_rxInfo.tag_id))) {
					Update_Lora_Config_Anchor(&lora_rxInfo, &dwmInfo);
				}
				UART_Reply_Search_Twr(&msg, &reply);
				break;
			}

			case UART_OPCODE_SET_ID: //PC id
				{
					const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;	// keep the antenna delays
					uint16_t new_id[8] = {msg.set_id_req.new_id,page[1],page[2],page[3],0xFFFF,0xFFFF,0xFFFF,reset_counter};	// keep delays and crystal trim
					uint8_t error;
					error = Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK;
					if (error){
						UART_Reply_Set_Self_ID(&msg, &reply, error);
						break;
					}
					error = Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)&new_id, 1) != HAL_OK;
					if(!error){ // updated self_id if writing to memory was successful
						 SetAnchor_self_id();
						 dwmInfo.mine_pc_id = PC_ID;	// UWB frames and backoff judging use the new id too
						 dwSetFrameIds();
					}
					UART_Reply_Set_Self_ID(&msg, &reply, error);
					break;
				}
				case UART_OPCODE_BACKOFF_CTRL: // used for diagnostics
				{
					if(msg.op_backoff_ctrl_req.BackOFF_CTRL == 0){
						BackOFF_CTRL = false;
						dwmInfo.backoff_timer = 0;	// release an armed backoff: poll on the next SEARCH_TWR
					}
					else if (msg.op_backoff_ctrl_req.BackOFF_CTRL == 1){
						BackOFF_CTRL = true;
					}
					// optional window: {ctrl, top, min}; backoff = random() % top + min. The bytes are in the old
					// 2.05 ms ticks (anc3.1.5), so the host's settings keep their real timing on the 1 ms tick.
					if (msg.header.length >= sizeof(S_HEADER) + 3 + 1 && msg.buff[1] != 0) {
						backoff_top_ms = OLD_TICKS_TO_MS(msg.buff[1]);
						backoff_min_ms = OLD_TICKS_TO_MS(msg.buff[2]);
					}

					UART_Reply_BackOFF_CTRL(&msg, &reply);
					break;
				}
#ifdef DEBUG_RESET_COUNTER_IN_FLASH
			case UART_OPCODE_RESET_RESET_COUNTER:
				{
					reset_counter = 0;
					const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;	// keep antenna delays (slots 1-2)
					uint16_t new_id[8] = {dwmInfo.mine_pc_id,page[1],page[2],page[3],page[4],page[5],page[6],reset_counter};
					uint8_t error;
					error = Flash_Erase(FLASH_SELF_ID_USER_BANK, FLASH_SELF_ID_PAGE) != HAL_OK;
					if (error){
						UART_Reply_Reset_Counter(&msg, &reply, error);
						break;
					}
					error = Flash_Write(FLASH_SELF_ID_ADDRESS, (uint64_t*)&new_id, 1) != HAL_OK;

					UART_Reply_Reset_Counter(&msg, &reply, error);
					break;
				}
			case UART_OPCODE_READ_RESET_COUNTER:
				{
					UART_Reply_Reset_Counter(&msg, &reply, 0);
					break;
				}
#endif
			case UART_OPCODE_WAKEUP_MODE: {

				radioFlags.wakeup_seq = true;
				dwmInfo.tag_dest = lora_rxInfo.tag_id = msg.op_twr_req.tag_id & 0x7FFF;

			  	if ((Backoff_Left() == 0) && (!dwmInfo.no_uwb)) {
			  		dwmInfo.backoff_timer = 0;
			  		dwRange();
			  	}
			  	UART_Reply_Wake_up(&msg, &reply);
			  	break;
			  }

  			case UART_OPCODE_STATUS: {
  				dwmStatus();
  				Lora_Status();
  				UART_Reply_Status(&msg, &reply);
  				break;
  			}

  			case UART_OPCODE_DIAGNOSTIC_STATUS: {
  				if(msg.op_sttd_req.UWB_enable_EVC == 1){
  					/*enable counter EVC ( error counters)
  			  		* Once its enabled it *CAN'T be* disabled.
  			  		*/
  			  		dwt_configeventcounters(true);
  			  		//Enable Accumulator reading
//  			  		dwt_configenable_user_accumem_read(true);
  				}else if(msg.op_sttd_req.UWB_enable_EVC == 0xff){
  			  		/*only way to reset EVC is to reset
  			  		 * it and to use dw again must reset STM
  			  		 */
  			  		UART_Reply_Status_UWB_Diag(&msg, &reply);
//  					dwt_configenable_user_accumem_read(false);

  			  		dwt_softreset();
  			  	  	HAL_NVIC_SystemReset();
  			  	}

  			  	dwmDiagStatus();
  			  	UART_Reply_Status_UWB_Diag(&msg, &reply);
  			  	break;
  			}
  			case UART_OPCODE_ANT_DELAY: {
  				Antenna_Delay_Command(&msg, &reply);
  				break;
  			}
  			case UART_OPCODE_XTAL_TRIM: {
  				Xtal_Trim_Command(&msg);
  				break;
  			}
  			case UART_OPCODE_READ_CIR_WINDOW: {
  				Cir_Window_Command(&msg);
  				break;
  			}
  			case UART_OPCODE_SET_LINK_FEATURES: {
  				Link_Features_Command(&msg);
  				break;
  			}
  			case UART_OPCODE_LOOPBACK: {
  				UART_SendMessage(&msg, msg.header.length);
  				break;
  			}

  			case UART_OPCODE_RESET: {
  				UART_Reply_Status(&msg, &reply);
  				HAL_Delay(1000);
  				HAL_NVIC_SystemReset();
  				break;
  			}
  			case UART_OPCODE_READ_UWB_RAW:{
  			  	dwmStatus();
  				UART_Reply_UWB_Raw(&msg, &reply);
  				break;
  			}
  			case UART_OPCODE_WRITE_UWB_RAW:{
  				dwt_write8bitoffsetreg(msg.op_write_uwb_raw_req.reg_id, msg.op_write_uwb_raw_req.reg_offset,msg.op_write_uwb_raw_req.buffer[0]);
  				if (msg.op_write_uwb_raw_req.reg_id == LDE_IF_ID && msg.op_write_uwb_raw_req.reg_offset == LDE_CFG1_OFFSET)
  					lde_cfg1 = msg.op_write_uwb_raw_req.buffer[0];	// survives dwConfig() re-runs
  				UART_Reply_Write_UWB_Raw(&msg, &reply);
  				break;
  			}

  			case UART_OPCODE_BIT_CONTROL:{
  				dwmInfo.tag_imposter = msg.op_bit_req.tag_imposter;
  				if (!lora_rxInfo.no_lora && (lora_rxInfo.freq != RF_FREQ_TAG(lora_rxInfo.self_id))) {
  					Update_Lora_Config_Imposter(&lora_rxInfo);
  				}
  				UART_Reply_BIT_Control(&msg, &reply);
  				break;
  			}

			case UART_OPCODE_UPDATE_FW:
			{
				UART_Reply_UPDATE_FW(&msg,&reply);
				write_bkup_sram();
				//*(uint32_t*)(SRAM_BKUP_REGISTER+4) = 0;
				uint32_t read_reg = *(uint32_t*)SRAM_BKUP_REGISTER;
				HAL_Delay(50); //give uart time to finish its reply
				if (Update_FW_Boot_Flag()){
					Jump_To_Boot();
				}
			}

  			default:
  				if (!fota_uart_command((uint8_t*) &msg, RxLen))
  					UART_Reply_error(&msg, &reply);
  				break;
  			}
  		}
  		else if(!dwmInfo.no_uwb && !fota_busy()){
  			if(dwmInfo.tag_imposter){
  				dwTagMode();
  			}else{
  				if(BackOFF_CTRL == true){
  					dwScanMode();
  				}
  			}
  		}
#ifndef TIMING_TESTS_USING_LED_GPIO
  		Led_Maintanence();
#endif // TIMING_TESTS_USING_LED_GPIO

  		if (gDebug && !lora_rxInfo.no_lora)
  		{
  			Lora_Debug();
  		}
		if (Lora_RXIdle()) {
			if (radioFlags.wakeup_seq) {
				switch (wakeup_state) {
					case WAKEUP_START:
						if (lora_rxInfo.freq != RF_FREQ_TAG(dwmInfo.tag_dest)) {
							Update_Lora_Config_Anchor(&lora_rxInfo, &dwmInfo);
						}
						Lora_Operation_TX();
						wakeup_state =  WAKEUP_WAIT_FOR_TX_IDLE;
						break;
					case WAKEUP_WAIT_FOR_TX_IDLE:
						if (Lora_TXIdle()) {
							radioFlags.wakeup_seq = false;
							wakeup_state = WAKEUP_START;
						}
						break;
				}
			}
			else if(dwmInfo.tag_imposter) {
				if(Lora_TXIdle()){
					TAG_Lora_Operation_TX(errors);
				}
	  		}
			else if (Lora_TXIdle()){
				Lora_Operation_RX();
			}
		}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
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
  HAL_NVIC_SetPriority(EXTI4_IRQn, 1, 0);	// below TIM4: the watchdog preempts a radio handler stuck on BUSY
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
  /* TIM4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
  /* GPDMA1_Channel0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
}

/* USER CODE BEGIN 4 */
void main_init() {
	SetAnchor_cable_id();
	// per-anchor seed for the backoff random(): an unseeded sequence makes anchors that hear
	// the same foreign frame pick the same backoff and collide round after round
	srandom((unsigned)(PC_ID ^ (dwmInfo.anchor_id << 8) ^ HAL_GetTick() ^ HAL_GetUIDw0()));
	write_clear_bkup_sram();
	read_hw_version();
	Init_Leds(HW_Version);
	lora_rxInfo.new_data = LR_NO_VALID_DATA;
	lora_rxInfo.TxCounter = 0;
	lora_rxInfo.buffer_length = 0;
	lora_rxInfo.lora_msg_index = 0;
	lora_rxInfo.rssi = 0;
	lora_rxInfo.snr = 0;

	dwmInfo.tof = 0;
	dwmInfo.dw_ready = DW_NO_VALID_DATA;
	dwmInfo.tag_dest = 33;
	dwmInfo.distance = 0;
	Link_Init();	// after srandom(): the first sequence number is random

	#ifdef DEBUG_RESET_COUNTER_IN_FLASH
		reset_counter = Flash_Read_ResetCounter();
		if(reset_counter == 0xffff)
			reset_counter = Flash_Reset_ResetCounter();
		Flash_Increase_ResetCounter();
	#endif

}

void write_clear_bkup_sram(void){
	*(uint32_t*)(SRAM_BKUP_REGISTER) = SRAM_BKUP_REGISTER_CLEAR;
}

void write_bkup_sram(void){
	*(uint32_t*)(SRAM_BKUP_REGISTER) = UPDATE_FW_MODE;
}


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
	HAL_NVIC_SystemReset();	// a hang with interrupts off would bypass the TIM4 watchdog
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
