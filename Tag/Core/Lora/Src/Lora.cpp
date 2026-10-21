#include "../Inc/Lora.hpp"

#include <stm32u5xx_hal_def.h>

#include "dw1000.h" //used only for TAG_ID
#include "main.h"
#include "flash_handler.h"
#include "uart_communication.h"
#include "Led.h"
#include "Power.h"

#define print(...)


/*!
 * Radio events function pointer
 */
static RadioCallbacks_t RadioEvents = { .txDone = &OnTxDone,
		.rxDone = &OnRxDone, .rxPreambleDetect = NULL, .rxSyncWordDone = NULL,
		.rxHeaderDone = NULL, .txTimeout = &OnTxTimeout, .rxTimeout =
				&OnRxTimeout, .rxError = &OnRxError, .cadDone = NULL, };

volatile AppStates_t TxState = SEND_PACKET;
volatile AppStates_t RxState = RECEIVE_PACKET;

RadioConfigurations_t radioConfiguration;
RadioFlags_t radioFlags = { .txDone = false, .txTimeout = true, };

extern LoraRxInfo_t lora_rxInfo;
uint8_t lora_msg_index = 0;

SX126xHal Radio(&hspi2, SX_SPI2_CS_GPIO_Port, SX_SPI2_CS_Pin,
SX_BUSY_GPIO_Port, SX_BUSY_Pin, SX_DIO1_GPIO_Port, SX_DIO1_Pin,
NULL, 0, NULL, 0, // Represent DIO2 and DIO3
		SX_RESET_GPIO_Port, SX_RESET_Pin, SX_Mode_FRx_Pin, SX_Mode_SX126x_Pin,
		SX_ANT_SW_GPIO_Port, SX_ANT_SW_Pin, &RadioEvents);

void Lora_init() {

	Radio.Init();
	SetConfiguration(&radioConfiguration, SELF_ID);
	ConfigureGeneralRadio(&Radio, &radioConfiguration);

	Lora_Status();
	if (lora_rxInfo.no_lora == false) {
		print("Tag LORA Init Succeed\r\n");
	} else {
		print("Tag LORA Init Failed\r\n");
		return;
	}

	lora_rxInfo.buffer_length = RX_BUFFER_SIZE;
	lora_rxInfo.rssi = 0;
	lora_rxInfo.snr = 0;
	lora_rxInfo.new_data = false;
	lora_rxInfo.freq = radioConfiguration.rfFrequency;
}

void Lora_Status() {
	uint8_t chip_mode = 0;
	/*
	 * if the lora in wakeup cycle mode first spi read is failed.
	 * so need to read the status twice. first wake the lora, second gives true value.
	 */
	if (lora_rxInfo.wakeup_cycle_enable == false){
		Radio.GetStatus().Value;
		/*
		 *  // 2ms to wait for lora chip wakeup when its in sleep cycle mode
		 *  in order to truly check its status, else the value might be 0 = not alive.
		 */
		uint32_t ms_time_tick = _1ms_tick+2;

		while((ms_time_tick > _1ms_tick));//|| (hspi2.State != HAL_SPI_STATE_READY)); // wait for 1ms to wakeup lora
	}

	chip_mode = Radio.GetStatus().Value;
	lora_rxInfo.no_lora = !((chip_mode != 0) && (chip_mode != 0xFF));
//	lora_rxInfo.no_lora = (Radio.ReadReg(RADIO_GET_STATUS) != 1? true:false);	//alive or not, 1 = alive, else = dead
}


void Lora_Operation_RX(status_bits errors) {
	HAL_GPIO_WritePin(SX_LED_TX_GPIO_Port, SX_LED_TX_Pin, GPIO_PIN_RESET); // Turn LED ON
	if (radioFlags.rxDone == true) {
		radioFlags.rxDone = false;
		Radio.GetPayload((uint8_t*)&lora_rxInfo.lora_wakeup_buffer, &lora_rxInfo.buffer_length , sizeof(lora_rxInfo.lora_wakeup_buffer));

		uint8_t checksums_= checksum((uint8_t*)&lora_rxInfo.lora_wakeup_buffer, sizeof(lora_rxInfo.lora_wakeup_buffer));
		uint16_t scramble_test = scramble(SELF_ID);
		if(checksums_ == lora_rxInfo.lora_wakeup_buffer.checksum){
			if (lora_rxInfo.lora_wakeup_buffer.scrambeld_self_id == scramble_test){ //Unicast mode
				HAL_GPIO_TogglePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin); // Turn LED On
				lora_rxInfo.wakeup_detected = true;
				PWR_SetTimeOut(PWR_Duration_MyMessage);
				RunTXStateMachine(errors);
				// Cleanup
				radioFlags.rxTimeout = false;
				lora_rxInfo.wakeup_cycle_enable = false;
				return ;
			} else {
				lora_rxInfo.new_data = false;
				ConfigureRadioRx(&Radio, &radioConfiguration);
				Radio.ClearIrqStatus(0xFFFF);
				Radio.SetRxDutyCycle(64000,64000); // 1280000 * 15.625us = 20sec Sleep, 20Sec Rx
			}
		}
	}
	if (lora_rxInfo.wakeup_cycle_enable == true || radioFlags.rxTimeout){
		lora_rxInfo.wakeup_cycle_enable = false;
		ConfigureRadioRx(&Radio, &radioConfiguration);
		Radio.ClearIrqStatus(0xFFFF);
		Radio.SetRxDutyCycle(64000,64000); // 1280000 * 15.625us = 20sec Sleep, 20Sec Rx
//		Radio.SetRx(radioConfiguration.rxTimeout);
	}
	// Cleanup
	radioFlags.rxTimeout = false;
}

uint16_t scramble(uint16_t self_id) {
    self_id ^= (self_id >> 8);         // Step 1: XOR high and low bytes
    self_id *= 0xD4BB;                 // Step 2: Multiply by 0xD4BB
    self_id ^= 0x1234;                 // Step 3: XOR with fixed mask
    self_id *= 0x1B51;                 // Step 4: Multiply by 0x1B51
    self_id ^= 0x5678;                 // Step 5: XOR with fixed mask
    return self_id;
}

void RunTXStateMachine(status_bits errors) {
	radioFlags.txDone = false;
	radioFlags.txTimeout = false;
	if (++lora_msg_index == MAX_INDEX)
		lora_msg_index = 0;

	uint8_t soc_battery = Get_Battery_Soc();

	sLoRaMsg messageToSend;
	messageToSend.msg_index = lora_msg_index;
	messageToSend.tag_id = SELF_ID;
	messageToSend.soc = soc_battery;
	messageToSend.errors.all = errors.all;
	messageToSend.checksum = checksum((uint8_t*)&messageToSend, sizeof(messageToSend)-1);// crc not including crc itself


	PrepareBuffer(&Radio, &messageToSend);
	ConfigureRadioTx(&Radio, &radioConfiguration);
	Radio.SetTx(radioConfiguration.txTimeout);
	HAL_GPIO_TogglePin(SX_LED_TX_GPIO_Port, SX_LED_TX_Pin); // Turn TXLED On
	HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET); // Turn RX LED Off
}

void ConfigureRadioTx(SX126xHal *radio, RadioConfigurations_t *config) {
	radio->SetDioIrqParams(config->irqTx, config->irqTx, IRQ_RADIO_NONE,
			IRQ_RADIO_NONE);
}

void ConfigureRadioRx(SX126xHal *radio, RadioConfigurations_t *config) {
	radio->SetDioIrqParams(config->irqRx, config->irqRx, IRQ_RADIO_NONE,
			IRQ_RADIO_NONE);
}

void PrepareBuffer(SX126xHal *radio, const sLoRaMsg * messageToSend) {
	radio->SetPayload((uint8_t*)messageToSend,sizeof(*messageToSend));
}

/*****************************
 *
 * 	Call backs
 *
 */

void OnRxDone(void) {
	radioFlags.rxDone = true;
}

void OnRxTimeout(void) {
	radioFlags.rxTimeout = true;
}

void OnRxError(IrqErrorCode_t errCode) {
	radioFlags.rxError = true;
}

void OnTxDone(void) {
	radioFlags.txDone = true;
}

void OnTxTimeout(void) {
	print("OnTxError \n\r");
	radioFlags.txTimeout = true;
}

void SetConfiguration(RadioConfigurations_t *config, uint16_t tagId) {
	config->irqRx = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT;
	config->irqTx = IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT;


	config->rfFrequency = RF_FREQ_TAG(tagId); //RF_FREQUENCY_868Mhz;// RF_FREQUENCY_START +  (tagId % RF_FREQUENCY_NUM_OF_CHANELS)* RF_FREQUENCY_BW;
	config->rxTimeout = RX_TIMEOUT_US;
	config->txTimeout = TX_TIMEOUT_US;

	config->txPower = TX_OUTPUT_POWER;
	config->txRampTime = RADIO_RAMP_10_US;
	config->packetType = PACKET_TYPE_LORA;
	config->modParams.PacketType = PACKET_TYPE_LORA;
	config->modParams.Params.LoRa.Bandwidth = LORA_BANDWIDTH;
	config->modParams.Params.LoRa.CodingRate = LORA_CODINGRATE;
	config->modParams.Params.LoRa.LowDatarateOptimize = LORA_LOWDATARATEOPTIMIZE;
	config->modParams.Params.LoRa.SpreadingFactor = LORA_SPREADING_FACTOR;
	config->packetParams.PacketType = PACKET_TYPE_LORA;
	config->packetParams.Params.LoRa.CrcMode = LORA_CRC_MODE;
	config->packetParams.Params.LoRa.HeaderType = LORA_HEADER_TYPE;
	config->packetParams.Params.LoRa.InvertIQ = LORA_IQ;
	config->packetParams.Params.LoRa.PayloadLength = BUFFER_SIZE;
	config->packetParams.Params.LoRa.PreambleLength = LORA_PREAMBLE_LENGTH;
}

void ChangeConfigurations(RadioConfigurations_t *config,
		RadioLoRaBandwidths_t bw, RadioLoRaCodingRates_t cr,
		RadioLoRaSpreadingFactors_t sf) {
	config->modParams.Params.LoRa.Bandwidth = bw;
	config->modParams.Params.LoRa.CodingRate = cr;
	config->modParams.Params.LoRa.SpreadingFactor = sf;
}
void ConfigureGeneralRadio(SX126xHal *radio, RadioConfigurations_t *config) {
	radio->SetPacketType(config->packetType);
	radio->SetPacketParams(&config->packetParams);
	radio->SetModulationParams(&config->modParams);
	radio->SetRfFrequency(config->rfFrequency);
	radio->SetTxParams(config->txPower, config->txRampTime);
	radio->SetInterruptMode();
}

/**
 *
 * @brief  EXTI line falling detection callback.
 * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
 * @retval None
 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
	/* NOTE: This function should not be modified, when the callback is needed,
	 the HAL_GPIO_EXTI_Falling_Callback could be implemented in the user file
	 */
	if (GPIO_Pin == SX_DIO1_Pin) {
		Radio.InvokeHandler();
	}
}

void GetRssiSnr(int8_t *rssi, int8_t *snr) {
	PacketStatus_t pkt_stat;
	Radio.GetPacketStatus(&pkt_stat);
	*rssi = pkt_stat.Params.LoRa.RssiPkt;
	*snr = pkt_stat.Params.LoRa.SnrPkt;

}

int Lora_TXidle(void) {
	return ((radioFlags.txDone || radioFlags.txTimeout) && !lora_rxInfo.no_lora);
}
