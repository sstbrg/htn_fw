/*
 * uart_communication.c
 *
 *  Created on: Apr 21, 2025
 *      Author: LironAvrmov
 */

#include "main.h"
#include "uart_communication.h"
#include "usart.h"
#include <string.h>
#include <stddef.h>
#include "stm32u5xx_it.h"
#include "rrc2040driver.h"

extern uint8_t BackOFF_CTRL;
extern uint16_t reset_counter;
extern uint8_t cable_id;
extern LoraRxInfo_t lora_rxInfo;
extern DwmInfo_t dwmInfo;
extern Fault_Counter fault_counter;
extern uint8_t HW_Version;
extern uint32_t _1ms_tick;

// Backoff time left in ms, wrap-safe; 0 = not backing off. An expired (or implausible) deadline
// is cleared here, so a stale value can never look like a future one after the tick wraps.
uint32_t Backoff_Left(void) {
	uint32_t d = dwmInfo.backoff_timer;
	if (d == 0)
		return 0;
	int32_t left = (int32_t)(d - _1ms_tick);
	if (left <= 0 || left > BACKOFF_MAX_MS) {
		dwmInfo.backoff_timer = 0;
		return 0;
	}
	return (uint32_t)left;
}
extern I2C_HandleTypeDef hi2c2;

// Replies that grew in anc3.2.0 end at `legacy_len` (anc3.1.5's length) unless LINK_F_REPLY_EXT is on.
static void UART_SendSized(S_UART_MESSAGE *reply, uint8_t full_len, uint8_t legacy_len) {
	uint8_t len = (link_cfg.features & LINK_F_REPLY_EXT) ? full_len : legacy_len;
	reply->header.length = len;
	((uint8_t*)reply)[len - 1] = checksum((uint8_t*) reply, len);
	UART_SendMessage(reply, len);
}
#define LEGACY_LEN(type, first_new) (sizeof(S_HEADER) + offsetof(type, first_new) + 1)


uint8_t checksum(const uint8_t *uart_tx_buffer, const uint8_t length)
{
	uint8_t sum = 0;
	for (uint8_t inx = 0; inx < length - 1; inx++)
	{
		sum += uart_tx_buffer[inx];
	}
	return sum;
}

uint8_t Validate_UART(void *buffer, uint8_t received_len)
{

	uint8_t *buff = (uint8_t*) buffer;
	uint8_t reported_len = buff[LENGTH_INDEX];
	if (reported_len != received_len)
		return VALIDATE_ERROR;
	uint8_t calculatedChkSm = checksum(buffer, reported_len);
	uint8_t receivedChkSm = buff[received_len - 1];
	if (calculatedChkSm != receivedChkSm) // last byte
		return VALIDATE_ERROR;

	return VALIDATE_OK;

}

/*
 TODO: UART FUNCTION FOR EVERY OPCODE
 */

void UART_Reply_Burn_msg(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_USRT_START_PROGRAMMING_REPLY);
	reply->start_prog_rply.current_id = lora_rxInfo.self_id;
	reply->start_prog_rply.is_tag = IS_TAG;	//	ANCHOR SW
	reply->start_prog_rply.errors.all = 0;
	reply->start_prog_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Set_Self_ID(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply, uint8_t error)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_SET_ID_REPLY);
	reply->set_id_rply.self_id =  lora_rxInfo.self_id;
	reply->set_id_rply.is_tag = IS_TAG;	//	TAG SW
	reply->set_id_rply.errors.bits.flash_error = error;
	reply->set_id_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_BackOFF_CTRL(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_BACKOFF_CTRL_REPLY);

	reply->op_backoff_ctrl_rply.BackOFF_CTRL = BackOFF_CTRL;
	reply->op_backoff_ctrl_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}


void UART_Reply_Reset_Counter(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply, uint8_t error)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_READ_RESET_COUNTER_REPLY);

	reply->op_read_reset_counter_rply.reset_counter = reset_counter;
	reply->op_read_reset_counter_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_BIT_Control(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(UART_OPCODE_BIT_CONTROL);
	reply->op_bit_rply.self_id = lora_rxInfo.self_id;
	reply->op_bit_rply.tag_imposter = dwmInfo.tag_imposter;
	//	errors
	reply->op_bit_rply.errors.all = 0;
	reply->op_bit_rply.errors.bits.UWB_Backoff = (Backoff_Left() != 0);
	reply->op_bit_rply.errors.bits.no_LoRa = lora_rxInfo.no_lora;
	reply->op_bit_rply.errors.bits.no_UWB = dwmInfo.no_uwb;
	reply->op_bit_rply.errors.bits.no_IMU = bmi.no_IMU;
	reply->op_bit_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Wake_up(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_WAKEUP_REPLY);
	reply->wakeup_rply.anch_id = cable_id;
	reply->wakeup_rply.tag_id = dwmInfo.tag_dest;

	reply->wakeup_rply.lora_timeout = LORA_TIMEOUT_DURATION;
	reply->wakeup_rply.errors.all = 0;
	reply->wakeup_rply.errors.bits.no_LoRa = lora_rxInfo.no_lora;
	reply->wakeup_rply.errors.bits.no_UWB = dwmInfo.no_uwb;
	reply->wakeup_rply.errors.bits.UWB_Backoff = (Backoff_Left() != 0);
	reply->wakeup_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Search_Twr(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
{

	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_TWR_REPLY);

	reply->op_twr_rply.anchor_id = dwmInfo.anchor_id;
	reply->op_twr_rply.tag_id = dwmInfo.tag_dest;


	if (lora_rxInfo.tag_id_msg_valid != lora_rxInfo.tag_id){ // if the last lora rx valid is not equal to the current search(twr opcode) id.
		lora_rxInfo.rssi = 0;
		lora_rxInfo.snr = 0;
		lora_rxInfo.signal_rssi = 0;
		lora_rxInfo.lora_msg_index = 0;
		lora_rxInfo.new_data = LR_NO_VALID_DATA;
	}
	reply->op_twr_rply.data_status = dwmInfo.dw_ready | lora_rxInfo.new_data;// data_validity
	uint8_t uwb_valid = (dwmInfo.dw_ready == VALID_DATA);

			//reset UWB data valid flag
	dwmInfo.dw_ready = DW_NO_VALID_DATA;

	reply->op_twr_rply.uwb_distance = dwmInfo.distance;
	reply->op_twr_rply.lora_rssi = lora_rxInfo.rssi;
	reply->op_twr_rply.lora_snr = lora_rxInfo.snr;
	reply->op_twr_rply.counter = lora_rxInfo.lora_msg_index; //counter
	reply->op_twr_rply.lora_signal_rssi = lora_rxInfo.signal_rssi;
	if (uwb_valid) {	// this round's reported exchange
		reply->op_twr_rply.exchange_tick_ms = dwmInfo.exchange_tick_ms;
		reply->op_twr_rply.tag_lora_tx_dbm = dwmInfo.tag_lora_tx_dbm;
		reply->op_twr_rply.tag_uwb_tx_level = dwmInfo.tag_uwb_tx_level;
	} else {
		reply->op_twr_rply.exchange_tick_ms = 0;
		reply->op_twr_rply.tag_lora_tx_dbm = LINK_LORA_UNKNOWN;
		reply->op_twr_rply.tag_uwb_tx_level = LINK_UWB_UNKNOWN;
	}
	reply->op_twr_rply.k_used = link_st.k_used;
	reply->op_twr_rply.profile_used = link_st.ext ? link_st.echo_profile : 0xFF;

			//reset Lora data every send
	lora_rxInfo.rssi = 0;
	lora_rxInfo.snr = 0;
	lora_rxInfo.signal_rssi = 0;
	lora_rxInfo.lora_msg_index = 0;
	lora_rxInfo.new_data = LR_NO_VALID_DATA; // old data

	reply->op_twr_rply.tag_soc_battery = lora_rxInfo.tag_soc;
	reply->op_twr_rply.tag_errors = lora_rxInfo.tag_errors;
	reply->op_twr_rply.tag_errors.bits.no_LoRa = dwmInfo.tag_no_lora;




	reply->op_twr_rply.errors.all = 0;
	reply->op_twr_rply.errors.bits.no_LoRa = lora_rxInfo.no_lora;
	reply->op_twr_rply.errors.bits.no_UWB = dwmInfo.no_uwb;
	reply->op_twr_rply.errors.bits.UWB_Backoff = (Backoff_Left() != 0);
	reply->op_twr_rply.errors.bits.wrong_UWB_ID = dwmInfo.wrong_ID;
	UART_SendSized(reply, reply->header.length, LEGACY_LEN(S_UART_OP_TWR_REPLY, exchange_tick_ms));
}

void UART_Reply_Status(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_STATUS_REPLY);
	strncpy(reply->op_stt_rply.SW, VERSION, sizeof(reply->op_stt_rply.SW));
	reply->op_stt_rply.HW = HW_Version;
	reply->op_stt_rply.self_id = lora_rxInfo.self_id;
	reply->op_stt_rply.is_tag = IS_TAG;
	//	UWB
	reply->op_stt_rply.UWB_tag_status = dwmInfo.tag_no_lora;
	reply->op_stt_rply.UWB_backoff = Backoff_Left();	// ms left, 0 = not backing off
	reply->op_stt_rply.UWB_last_status_reg = dwmInfo.statusReg;

	// LoRa
	reply->op_stt_rply.LoRa_last_SNR = lora_rxInfo.snr;
	reply->op_stt_rply.LoRa_last_RSSI = lora_rxInfo.rssi;
	reply->op_stt_rply.LoRa_RxCounter = lora_rxInfo.RxCounter;
	reply->op_stt_rply.LoRa_Freq = lora_rxInfo.freq;
	//	errors
	reply->op_stt_rply.errors.all = 0;
	reply->op_stt_rply.errors.bits.no_LoRa = lora_rxInfo.no_lora;
	reply->op_stt_rply.errors.bits.no_UWB = dwmInfo.no_uwb;
	reply->op_stt_rply.errors.bits.no_IMU = bmi.no_IMU;
	reply->op_stt_rply.fault_counter = fault_counter;
	reply->op_stt_rply.link_features = link_cfg.features;
	reply->op_stt_rply.link_flags = (link_st.ext ? LINK_STATE_EXT : 0)
			| (link_cfg.lora_owner ? LINK_STATE_OWNER : 0)
			| (link_st.lora_phase != LINK_LP_IDLE ? LINK_STATE_PENDING : 0)
			| (link_st.reverting ? LINK_STATE_REVERT : 0)
			| (Link_Coexisting() ? LINK_STATE_COEXIST : 0)
			| (link_st.ext && link_st.echo_lde_nlos ? LINK_STATE_TAG_NLOS : 0)
			| (link_st.station_hold ? LINK_STATE_HOLD_MAX : 0)
			| (link_st.prof_phase != PROF_P_IDLE ? LINK_STATE_SWITCHING : 0);
	reply->op_stt_rply.profile_req = link_st.prof_target;
	reply->op_stt_rply.profile_echo = link_st.ext ? link_st.echo_profile : 0xFF;
	reply->op_stt_rply.profile_cur = link_st.prof_cur;
	reply->op_stt_rply.profile_reason = link_st.prof_reason;
	reply->op_stt_rply.req_lora_tx_dbm = (link_st.ext && link_cfg.lora_owner) ? link_st.lora_req : LINK_LORA_UNKNOWN;
	reply->op_stt_rply.req_uwb_tx_level = link_st.uwb_level;
	reply->op_stt_rply.tag_lora_tx_dbm = link_st.echo_lora_dbm;
	reply->op_stt_rply.tag_uwb_tx_level = link_st.echo_uwb_level;
	reply->op_stt_rply.lde_preset = link_st.lde_nlos;
	reply->op_stt_rply.k_used = link_st.k_used;
	{	// any anchor with a battery gauge on I2C2, not only cable id 3; 0 = no gauge, as before
		uint8_t soc = RRC2040_Soc(&hi2c2);
		reply->op_stt_rply.battery = (soc == RRC2040_NO_GAUGE) ? 0 : soc;
	}

	UART_SendSized(reply, reply->header.length, LEGACY_LEN(S_UART_STATUS_REPLY, link_features));



}

void UART_Reply_Status_UWB_Diag(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_STATUS_UWB_DIAG_REPLY);

	//	UWB
	memcpy(reply->op_sttd_rply.RX_FQUAL_buffer,dwmInfo.diagnostics.RX_FQUAL_buffer, sizeof(reply->op_sttd_rply.RX_FQUAL_buffer));

	reply->op_sttd_rply.stdNoise 		= dwmInfo.diagnostics.stdNoise;
	reply->op_sttd_rply.firstPath 		= dwmInfo.diagnostics.firstPath;
	reply->op_sttd_rply.cirPwr 			= dwmInfo.diagnostics.cirPwr;
	reply->op_sttd_rply.rxLvl 			= dwmInfo.diagnostics.rxLvl;
	reply->op_sttd_rply.rxPreamCount 	= dwmInfo.diagnostics.rxPreamCount;
	reply->op_sttd_rply.firstPathAmp1	= dwmInfo.diagnostics.firstPathAmp1;

	reply->op_sttd_rply.PHE   = dwmInfo.counter.PHE   ;
	reply->op_sttd_rply.RSL   = dwmInfo.counter.RSL   ;
	reply->op_sttd_rply.CRCG  = dwmInfo.counter.CRCG  ;
	reply->op_sttd_rply.CRCB  = dwmInfo.counter.CRCB  ;
	reply->op_sttd_rply.ARFE  = dwmInfo.counter.ARFE  ;
	reply->op_sttd_rply.OVER  = dwmInfo.counter.OVER  ;
	reply->op_sttd_rply.SFDTO = dwmInfo.counter.SFDTO ;
	reply->op_sttd_rply.PTO   = dwmInfo.counter.PTO   ;
	reply->op_sttd_rply.RTO   = dwmInfo.counter.RTO   ;
	reply->op_sttd_rply.TXF   = dwmInfo.counter.TXF   ;
	reply->op_sttd_rply.HPW   = dwmInfo.counter.HPW   ;
	reply->op_sttd_rply.TXW   = dwmInfo.counter.TXW   ;

	// latched with the diagnostics on the same exchange
	reply->op_sttd_rply.lde_ppindx       = dwmInfo.lde_ppindx;
	reply->op_sttd_rply.rxpacc_nosat     = dwmInfo.rxpacc_nosat;
	reply->op_sttd_rply.exchange_tick_ms = dwmInfo.exchange_tick_ms;

	UART_SendSized(reply, reply->header.length, LEGACY_LEN(S_UART_STATUS_UWB_DIAG_REPLY, lde_ppindx));
}

void UART_Reply_error(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = UART_OPCODE_ERROR;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_ERROR_REPLY);
	reply->op_error_rply.expected_opcode = msg->header.opcode;
	reply->op_error_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_UWB_Raw(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	static uint8_t temp_buf[sizeof(reply->op_uwb_raw_rply.buffer)+1];

	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_UWB_RAW_REPLY);

	reply->op_uwb_raw_rply.reg_id =	msg->op_uwb_raw_req.reg_id;
	reply->op_uwb_raw_rply.reg_offset =	msg->op_uwb_raw_req.reg_offset;

	if (reply->op_uwb_raw_req.reg_len > sizeof(reply->op_uwb_raw_rply.buffer)) // if asked reg_len greater that rply buffer size , return buffer size
		reply->op_uwb_raw_rply.reg_len = sizeof(reply->op_uwb_raw_rply.buffer);
	else
		reply->op_uwb_raw_rply.reg_len = msg->op_uwb_raw_req.reg_len;

	memset(reply->op_uwb_raw_rply.buffer, 0, sizeof(reply->op_uwb_raw_rply.buffer)); // reset buffer
	if(msg->op_uwb_raw_req.reg_id == ACC_MEM_ID){

		dwt_readaccdata(temp_buf,reply->op_uwb_raw_rply.reg_len+1 ,reply->op_uwb_raw_rply.reg_offset );
		memcpy(reply->op_uwb_raw_rply.buffer , temp_buf+1 , reply->op_uwb_raw_rply.reg_len);
 	}
	else{
		dwt_readfromdevice(	reply->op_uwb_raw_rply.reg_id,
						reply->op_uwb_raw_rply.reg_offset,
						reply->op_uwb_raw_rply.reg_len,
						reply->op_uwb_raw_rply.buffer);
	}

	reply->op_uwb_raw_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Write_UWB_Raw(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){

	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_WRITE_UWB_RAW_REPLY);
	reply->op_write_uwb_raw_rply.reg_id =	msg->op_write_uwb_raw_req.reg_id;
	reply->op_write_uwb_raw_rply.reg_offset =	msg->op_write_uwb_raw_req.reg_offset;
	reply->op_write_uwb_raw_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);

}

void UART_Reply_UPDATE_FW(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_UPDATE_FW_REPLY);
	reply->op_update_fw_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply,reply->header.length);
}
