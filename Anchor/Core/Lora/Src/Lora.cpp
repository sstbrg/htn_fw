#include <Lora.hpp>
#include <stm32u5xx_hal_def.h>
#include "uart_communication.h"
#include "main.h"

/*!
 * Radio events function pointer
 */
static RadioCallbacks_t RadioEvents = {
    .txDone = &OnTxDone,
    .rxDone = &OnRxDone,
    .rxPreambleDetect = NULL,
    .rxSyncWordDone = NULL,
    .rxHeaderDone = NULL,
    .txTimeout = &OnTxTimeout,
    .rxTimeout = &OnRxTimeout,
    .rxError = &OnRxError,
    .cadDone = NULL,
};


RadioConfigurations_t radioConfiguration;
RadioFlags_t radioFlags = {
		.wakeup_seq = false,
	    .rxDone = false,
	    .rxError = false,
	    .rxTimeout = false,
	    .txDone = false,
	    .txTimeout = true,
		.RxOrTx = Rx,
};

extern LoraRxInfo_t lora_rxInfo;
extern uint8_t cable_id;
extern uint8_t cable_id;
DwmInfo_t * dwmInfoPtr;

SX126xHal Radio(&hspi2, SX_SPI2_CS_GPIO_Port, SX_SPI2_CS_Pin,
				SX_BUSY_GPIO_Port, SX_BUSY_Pin, SX_DIO1_GPIO_Port, SX_DIO1_Pin,
				NULL, 0, NULL, 0, // Represent DIO2 and DIO3
				SX_RESET_GPIO_Port, SX_RESET_Pin, SX_Mode_FRx_Pin, SX_Mode_SX126x_Pin,
				SX_ANT_SW_GPIO_Port, SX_ANT_SW_Pin, &RadioEvents);




void Lora_init(LoraRxInfo_t* pLoraRxInfo, DwmInfo_t * inDwmInfoPtr)
{
	dwmInfoPtr = inDwmInfoPtr;
	Radio.Init();
	SetConfiguration(&radioConfiguration , inDwmInfoPtr->tag_dest);
	ConfigureGeneralRadio(&Radio, &radioConfiguration);

	Lora_Status();
	if(lora_rxInfo.no_lora){
		return;
	}

	lora_rxInfo.buffer_length = LORA_RX_BUFFER_SIZE;
	lora_rxInfo.rssi = 0;
	lora_rxInfo.snr = 0;
	lora_rxInfo.TxCounter = 0;
	lora_rxInfo.new_data = LR_NO_VALID_DATA;
	lora_rxInfo.freq = radioConfiguration.rfFrequency;
	}

void Lora_Status(){
	__disable_irq();
	uint8_t chip_mode = Radio.GetStatus().Value;
	lora_rxInfo.no_lora = !((chip_mode != 0) && (chip_mode != 0xFF));
//	lora_rxInfo.no_lora = (Radio.ReadReg(RADIO_GET_STATUS) != 1? true:false);	//alive or not, 1 = alive, else = dead
	__enable_irq();
}

RadioStatus_t status;

void Lora_Operation_RX(){
	HAL_GPIO_WritePin(SX_LED_TX_GPIO_Port, SX_LED_TX_Pin, GPIO_PIN_RESET);
	sLoRaMsg* p_lora_msg;
	lora_rxInfo.rssi = 0;
	lora_rxInfo.snr = 0;


	if(radioFlags.rxDone == true){
		radioFlags.rxDone = false;
		Radio.GetPayload(lora_rxInfo.raw_buffer, &lora_rxInfo.buffer_length, LORA_RX_BUFFER_SIZE);

		p_lora_msg = (sLoRaMsg*)lora_rxInfo.raw_buffer;
		uint8_t checksum_ = checksum((uint8_t*)&lora_rxInfo.raw_buffer, sizeof(lora_rxInfo.raw_buffer)-1);// crc not including crc itself
		if(p_lora_msg->checksum == checksum_)
			if(p_lora_msg->tag_id == lora_rxInfo.tag_id){ //check for Lora Tag ID
				lora_rxInfo.lora_msg_index = p_lora_msg->msg_index;
				lora_rxInfo.tag_soc = p_lora_msg->soc;
				lora_rxInfo.tag_errors.all = p_lora_msg->tag_errors.all;
				lora_rxInfo.tag_id_msg_valid = lora_rxInfo.tag_id;
				lora_rxInfo.new_data = VALID_DATA;

				lora_rxInfo.RxCounter++;
				GetRssiSnr(&lora_rxInfo.rssi, &lora_rxInfo.snr);

				HAL_GPIO_TogglePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin);
			}
	}


	if(radioFlags.rxTimeout == true){
		radioFlags.rxTimeout = false;
	}

	ConfigureRadioRx(&Radio, &radioConfiguration);
	Radio.ClearIrqStatus(0xFFFF);
	Radio.SetRx(radioConfiguration.rxTimeout);
	status = Radio.GetStatus();
}

void Lora_Operation_TX( void )
{
	HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET);

	radioFlags.txTimeout = false;
	radioFlags.txDone = false;
	uint16_t srambeld_id = scramble(dwmInfoPtr->tag_dest);
	sLoRaWakeupMsg MessageToSend;
	MessageToSend.anchor_id = lora_rxInfo.self_id;
	MessageToSend.tag_scrambled_id	=	((uint16_t)(srambeld_id & 0xFF00) | (uint8_t)(srambeld_id & 0xFF));
	MessageToSend.spare = 0;
	MessageToSend.checksum = checksum((uint8_t*)&MessageToSend, sizeof(MessageToSend)-1);// crc not including crc itself

	PrepareBuffer(&Radio, &MessageToSend);
	ConfigureRadioTx(&Radio, &radioConfiguration);
	Radio.SetTx(radioConfiguration.txTimeout);
	HAL_GPIO_TogglePin(SX_LED_TX_GPIO_Port, SX_LED_TX_Pin); // Turn LED On
}

uint16_t scramble(uint16_t self_id) {
    self_id ^= (self_id >> 8);         // Step 1: XOR high and low bytes
    self_id *= 0xD4BB;                 // Step 2: Multiply by 0xD4BB
    self_id ^= 0x1234;                 // Step 3: XOR with fixed mask
    self_id *= 0x1B51;                 // Step 4: Multiply by 0x1B51
    self_id ^= 0x5678;                 // Step 5: XOR with fixed mask
    return self_id;
}

void TAG_Lora_Operation_TX(status_bits errors) {
	HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET);
	static uint8_t lora_msg_index = 0;
	radioFlags.txDone = false;
	radioFlags.txTimeout = false;

	if (++lora_msg_index == MAX_INDEX)
		lora_msg_index = 0; //0 = Daniel, //1 = Chen

	sLoRaMsg messageToSend;
	messageToSend.msg_index = lora_msg_index;
	messageToSend.tag_id = lora_rxInfo.self_id;
	messageToSend.soc = 0;
	messageToSend.tag_errors.all = errors.all;
	messageToSend.checksum = checksum((uint8_t*)&messageToSend, sizeof(messageToSend)-1);// crc not including crc itself


	TAG_PrepareBuffer(&Radio, &messageToSend);
	ConfigureRadioTx(&Radio, &radioConfiguration);
	Radio.SetTx(radioConfiguration.txTimeout);

	HAL_GPIO_TogglePin(SX_LED_TX_GPIO_Port, SX_LED_TX_Pin); // Turn LED On
}

void ConfigureRadioTx(SX126xHal *radio, RadioConfigurations_t *config){
    radio->SetDioIrqParams(config->irqTx, config->irqTx, IRQ_RADIO_NONE, IRQ_RADIO_NONE);
	radioFlags.RxOrTx = Tx;
}

void ConfigureRadioRx(SX126xHal *radio, RadioConfigurations_t *config){
    radio->SetDioIrqParams(config->irqRx, config->irqRx, IRQ_RADIO_NONE, IRQ_RADIO_NONE);
	radioFlags.RxOrTx = Rx;
}

void PrepareBuffer(SX126xHal *radio, const sLoRaWakeupMsg *messageToSend){
    radio->SetPayload((uint8_t*)messageToSend, sizeof(*messageToSend));
}

void TAG_PrepareBuffer (SX126xHal *radio, const sLoRaMsg *messageToSend){
    radio->SetPayload((uint8_t*)messageToSend, sizeof(*messageToSend));
}


void OnRxDone( void )
{
    radioFlags.rxDone= true;


}

void OnRxTimeout( void )
{
    radioFlags.rxTimeout = true;
}

void OnRxError( IrqErrorCode_t errCode )
{

    radioFlags.rxError = true;
}

void OnTxDone( void )
{
    radioFlags.txDone = true;
    HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET); // Turn LED On
}

void OnTxTimeout( void )
{
    radioFlags.txTimeout = true;
    HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET); // Turn LED On
}

void SetConfiguration(RadioConfigurations_t *config ,uint16_t tag_dest){
    config->irqRx = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT;
    config->irqTx = IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT;
    config->rfFrequency =  RF_FREQ_TAG(tag_dest); // 904750000
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

void ChangeConfigurations(RadioConfigurations_t *config, RadioLoRaBandwidths_t bw, RadioLoRaCodingRates_t cr, RadioLoRaSpreadingFactors_t sf){
	config->modParams.Params.LoRa.Bandwidth = bw;
	config->modParams.Params.LoRa.CodingRate = cr;
    config->modParams.Params.LoRa.SpreadingFactor = sf;
}
void ConfigureGeneralRadio(SX126xHal *radio, RadioConfigurations_t *config){
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
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(GPIO_Pin);

  /* NOTE: This function should not be modified, when the callback is needed,
           the HAL_GPIO_EXTI_Falling_Callback could be implemented in the user file
   */
  if(GPIO_Pin == SX_DIO1_Pin){
	  Radio.InvokeHandler();
  }
}

void GetRssiSnr(int8_t *rssi, int8_t *snr)
{
    PacketStatus_t pkt_stat;
    Radio.GetPacketStatus(&pkt_stat);
    *rssi = pkt_stat.Params.LoRa.RssiPkt;
    *snr = pkt_stat.Params.LoRa.SnrPkt;

}

int Lora_TXIdle(void) {
	return ((radioFlags.txDone || radioFlags.txTimeout) && !lora_rxInfo.no_lora);
}

int Lora_RXIdle(void) {
	return ((radioFlags.rxDone || radioFlags.rxTimeout || radioFlags.rxError) && !lora_rxInfo.no_lora);
}

void Update_Lora_Config_Imposter(LoraRxInfo_t* pLoraRxInfo){
	Update_Lora_Config(pLoraRxInfo, pLoraRxInfo->self_id);
}

void Update_Lora_Config_Anchor(LoraRxInfo_t* pLoraRxInfo, DwmInfo_t * inDwmInfoPtr){
	Update_Lora_Config(pLoraRxInfo, inDwmInfoPtr->tag_dest);
}

void Update_Lora_Config(LoraRxInfo_t* pLoraRxInfo, uint16_t tag_dest){
	SetConfiguration(&radioConfiguration , tag_dest);
	ConfigureGeneralRadio(&Radio, &radioConfiguration);
	pLoraRxInfo -> freq = radioConfiguration.rfFrequency;
}

//---------------------
uint16_t IrqStatus = 0;


void  Lora_Debug()
{
	IrqStatus = Radio.GetIrqStatus();
	status = Radio.GetStatus();
	Radio.ClearIrqStatus(0xFFFF);
}
