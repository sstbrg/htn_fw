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

/* LoRa TX power under the anchor's control (doc section 9D/9J, rule zero). Louder takes effect at
 * once; quieter only after request (seq) -> ack -> confirm. No owner poll for LINK_REVERT_MS after
 * a quieter step: back to the previous power; for LINK_SILENCE_MS: maximum. A legacy poll, or polls
 * from two stations, mean maximum at once. Applied when the next packet starts. */
#define LINK_NO_OWNER 0xFFFF
static int8_t lora_radio_dbm = TX_OUTPUT_POWER;	// in the radio now
static int8_t lora_cur = TX_OUTPUT_POWER;		// what the next packet uses
static int8_t lora_prev = TX_OUTPUT_POWER;		// before the last quieter step
static int8_t pend_dbm = TX_OUTPUT_POWER;
static uint8_t pend_seq = 0, pend_valid = 0;
static uint8_t last_seq = 0;
static int8_t last_req = TX_OUTPUT_POWER;
static uint16_t owner_src = LINK_NO_OWNER;		// the anchor whose requests count
static uint32_t owner_tick = 0;
static uint32_t lora_tx_tick = 0;	// last SetTx, for the lost-interrupt recovery


SX126xHal Radio(&hspi2, SX_SPI2_CS_GPIO_Port, SX_SPI2_CS_Pin,
SX_BUSY_GPIO_Port, SX_BUSY_Pin, SX_DIO1_GPIO_Port, SX_DIO1_Pin,
NULL, 0, NULL, 0, // Represent DIO2 and DIO3
		SX_RESET_GPIO_Port, SX_RESET_Pin, SX_Mode_FRx_Pin, SX_Mode_SX126x_Pin,
		SX_ANT_SW_GPIO_Port, SX_ANT_SW_Pin, &RadioEvents);

void Lora_LinkMax(void) {
	lora_cur = lora_prev = TX_OUTPUT_POWER;
	pend_valid = 0;
	owner_src = LINK_NO_OWNER;
}

// Extended poll from `src`; returns the ack for its response (0 = not this anchor's request).
uint8_t Lora_LinkRequest(uint16_t src, uint8_t seq, int8_t req, uint8_t confirm, bool multi) {
	uint32_t now = HAL_GetTick();
	if (multi) {			// 9K: two stations, no adaptation
		Lora_LinkMax();
		return 0;
	}
	if (req == LINK_LORA_KEEP)
		return 0;
	if (req > TX_OUTPUT_POWER) req = TX_OUTPUT_POWER;
	if (req < LINK_LORA_MIN_DBM) req = LINK_LORA_MIN_DBM;
	if (owner_src != LINK_NO_OWNER && owner_src != src && now - owner_tick <= LINK_SILENCE_MS) {
		if (req > lora_cur) {	// another anchor: only louder counts
			lora_cur = lora_prev = req;
			pend_valid = 0;
		}
		return 0;
	}
	if (owner_src != src) {	// a new owner starts from scratch
		owner_src = src;
		last_seq = seq ^ 0xFF;
	}
	owner_tick = now;
	if (seq != last_seq || req != last_req) {	// a new request (a repeat is a keep-alive)
		last_seq = seq;
		last_req = req;
		if (req >= lora_cur) {	// louder or the same: at once
			lora_cur = lora_prev = req;
			pend_valid = 0;
		} else {				// quieter: wait for the confirm
			pend_seq = seq;
			pend_dbm = req;
			pend_valid = 1;
		}
	}
	if (pend_valid && confirm != 0 && confirm == pend_seq) {
		lora_prev = lora_cur;
		lora_cur = pend_dbm;
		pend_valid = 0;
	}
	return seq;
}

bool Lora_LinkPending(void) {
	return pend_valid;
}

// Power for the next packet, after the silence reverts.
int8_t Lora_LinkPower(void) {
	if (owner_src != LINK_NO_OWNER) {
		uint32_t quiet = HAL_GetTick() - owner_tick;
		if (quiet > LINK_SILENCE_MS)
			Lora_LinkMax();
		else if (quiet > LINK_REVERT_MS && lora_cur < lora_prev)
			lora_cur = lora_prev;
	}
	return lora_cur;
}

static void Lora_ApplyTxPower(void) {
	int8_t dbm = Lora_LinkPower();
	if (dbm != lora_radio_dbm) {
		radioConfiguration.txPower = dbm;
		Radio.SetTxParams(dbm, radioConfiguration.txRampTime);
		lora_radio_dbm = dbm;
	}
}

void Lora_init() {

	Radio.Init();
	SetConfiguration(&radioConfiguration, SELF_ID);
	ConfigureGeneralRadio(&Radio, &radioConfiguration);
	lora_radio_dbm = radioConfiguration.txPower;
	Lora_LinkMax();

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
		uint32_t t0 = _1ms_tick;	// wrap-safe; 5 ms on the 1 ms tick (2 old 2.05 ms ticks)

		while (_1ms_tick - t0 < 5);	// wait for the lora chip to wake up
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
	Lora_ApplyTxPower();
	lora_tx_tick = HAL_GetTick();
	Radio.SetTx(radioConfiguration.txTimeout);
	HAL_GPIO_TogglePin(SX_LED_TX_GPIO_Port, SX_LED_TX_Pin); // Turn TXLED On
	HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET); // Turn RX LED Off
}

/* ---------------- configuration channel (main.h LCMD_*) ----------------
 * After a beacon whose counter is a multiple of the listen period the tag listens LCMD_LISTEN_MS for
 * a command preamble (inverted IQ, so only commands are demodulated). A valid command goes to the
 * same handler as the extended UWB poll v2 and is answered right away with an ack (inverted IQ).
 * Anything unexpected ends the window and beacons go on: the beacon schedule is never held up by
 * more than one command and its ack. */
typedef enum { LS_BEACON, LS_LISTEN, LS_ACK } TagLoraState_t;
static TagLoraState_t lstate = LS_BEACON;
static bool beacon_sent = false;		// the last TX was a beacon: its listen slot may follow
static uint32_t listen_t0 = 0;
static uint32_t uwb_poll_tick = 0, ext_poll_tick = 0;
static uint8_t listen_override = 0;		// 0 = automatic
static uint32_t cmd_tick = 0;
static sLoRaAck lora_ack;

void Lora_NoteUwbPoll(bool extended) {
	uwb_poll_tick = HAL_GetTick();
	if (extended)
		ext_poll_tick = uwb_poll_tick;
}

void Lora_SetListenN(uint8_t n) {
	listen_override = n;
	cmd_tick = HAL_GetTick();
}

uint8_t Lora_ListenN(void) {
	uint32_t now = HAL_GetTick();
	if (listen_override && now - cmd_tick > 30000)
		listen_override = 0;
	if (now - uwb_poll_tick > 1000)
		return 1;					// UWB silent: LoRa is the only way in, listen after every beacon
	if (listen_override)
		return listen_override;
	if (dwMulti())
		return LCMD_LISTEN_GROUND;	// two stations: keep the channel as it was
	return (now - ext_poll_tick < 2000) ? LCMD_LISTEN_ACTIVE : LCMD_LISTEN_GROUND;
}

// Command mode: inverted IQ, CRC on, 11-byte payload; else the beacon settings. SX1262 datasheet
// 15.4: register 0x0736 bit 2 must be 0 with inverted IQ and 1 with normal IQ.
static void lora_cmd_mode(bool on) {
	PacketParams_t pp = radioConfiguration.packetParams;
	pp.Params.LoRa.InvertIQ = on ? LORA_IQ_INVERTED : LORA_IQ;
	pp.Params.LoRa.CrcMode = on ? LORA_CRC_ON : LORA_CRC_MODE;
	pp.Params.LoRa.PayloadLength = on ? sizeof(sLoRaCmd) : BUFFER_SIZE;
	Radio.SetPacketParams(&pp);
	uint8_t r = Radio.ReadReg(0x0736);
	Radio.WriteReg(0x0736, on ? (r & ~0x04) : (r | 0x04));
}

static void lora_listen_end(void) {
	Radio.SetStandby(STDBY_RC);
	Radio.ClearIrqStatus(0xFFFF);
	radioFlags.rxDone = radioFlags.rxTimeout = radioFlags.rxError = false;
	lora_cmd_mode(false);
	lstate = LS_BEACON;
}

static void lora_handle_command(void) {
	uint8_t buf[sizeof(sLoRaCmd)];
	uint8_t size = 0;
	if (Radio.GetPayload(buf, &size, sizeof(buf)) != 0 || size != sizeof(sLoRaCmd)) {
		lora_listen_end();
		return;
	}
	const sLoRaCmd *c = (const sLoRaCmd*)buf;
	if (c->magic != LCMD_MAGIC || c->checksum != checksum(buf, sizeof(sLoRaCmd)) || c->tag_id != SELF_ID) {
		lora_listen_end();
		return;
	}
	cmd_tick = HAL_GetTick();
	PWR_SetTimeOut(PWR_Duration_MyMessage);	// an anchor is talking to this tag: stay awake
	lora_ack.magic = LACK_MAGIC;
	lora_ack.tag_id = SELF_ID;
	lora_ack.seq = c->seq;
	lora_ack.item = c->item;
	lora_ack.value = dwLinkItem(c->src, c->seq, c->item, c->value, &lora_ack.flags);
	lora_ack.lora_dbm = lora_rxInfo.no_lora ? LINK_LORA_KEEP : Lora_LinkPower();
	lora_ack.listen_n = Lora_ListenN();
	lora_ack.checksum = checksum((uint8_t*)&lora_ack, sizeof(lora_ack));

	Radio.SetStandby(STDBY_RC);
	Radio.ClearIrqStatus(0xFFFF);
	radioFlags.rxDone = false;
	radioFlags.txDone = radioFlags.txTimeout = false;
	Radio.SetPayload((uint8_t*)&lora_ack, sizeof(lora_ack));	// still in command mode: inverted IQ
	ConfigureRadioTx(&Radio, &radioConfiguration);
	lora_tx_tick = HAL_GetTick();
	Radio.SetTx(radioConfiguration.txTimeout);
	lstate = LS_ACK;
}

// Sleep only from plain beacon mode: normal IQ, the setting the wake listener needs.
bool Lora_TagCanSleep(void) {
	return lstate == LS_BEACON;
}

void Lora_TagService(status_bits errors) {
	switch (lstate) {
	case LS_LISTEN:
		if (radioFlags.rxDone)
			lora_handle_command();
		else if (radioFlags.rxTimeout || radioFlags.rxError || HAL_GetTick() - listen_t0 > LCMD_EXCHANGE_MS)
			lora_listen_end();	// nothing for us (the timer stops on a preamble; the guard covers the rest)
		return;
	case LS_ACK:
		if (!Lora_TXidle())		// its TX deadline recovers a lost interrupt
			return;
		lora_cmd_mode(false);
		lstate = LS_BEACON;
		break;
	default:
		break;
	}
	if (!Lora_TXidle())
		return;
	if (beacon_sent) {
		beacon_sent = false;
		uint8_t n = Lora_ListenN();
		if (lora_msg_index % n == 0) {	// this beacon's slot: listen for a command
			lora_cmd_mode(true);
			Radio.SetStopRxTimerOnPreambleDetect(true);
			ConfigureRadioRx(&Radio, &radioConfiguration);
			Radio.ClearIrqStatus(0xFFFF);
			radioFlags.rxDone = radioFlags.rxTimeout = radioFlags.rxError = false;
			listen_t0 = HAL_GetTick();
			Radio.SetRx((uint32_t)LCMD_LISTEN_MS * 64);	// 15.625 us units
			lstate = LS_LISTEN;
			return;
		}
	}
	RunTXStateMachine(errors);
	beacon_sent = true;
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

/* A TX whose completion interrupt never comes would stop the beacon for good while the main
 * loop keeps the watchdog fed. The radio's own TX timeout is ~0.78 s (50000 x 15.625 us), so no
 * event for LORA_RECOVER_MS means it was lost: reset and reconfigure the radio, power included. */
#define LORA_RECOVER_MS 3000

int Lora_TXidle(void) {
	if (!radioFlags.txDone && !radioFlags.txTimeout && !lora_rxInfo.no_lora
			&& HAL_GetTick() - lora_tx_tick > LORA_RECOVER_MS) {
		Radio.Init();
		ConfigureGeneralRadio(&Radio, &radioConfiguration);	// txPower holds the power in use
		Radio.ClearIrqStatus(0xFFFF);
		lora_radio_dbm = radioConfiguration.txPower;
		Lora_Status();
		radioFlags.txTimeout = true;
	}
	return ((radioFlags.txDone || radioFlags.txTimeout) && !lora_rxInfo.no_lora);
}
