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
#include "stm32u5xx_it.h"
#include "rrc2040driver.h"

extern uint8_t BackOFF_CTRL;
extern uint16_t reset_counter;
extern uint8_t cable_id;
extern LoraRxInfo_t lora_rxInfo;
extern DwmInfo_t dwmInfo;
extern Fault_Counter fault_counter;
extern uint8_t HW_Version;
extern I2C_HandleTypeDef hi2c2;


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
	reply->op_bit_rply.errors.bits.UWB_Backoff = !!dwmInfo.backoff_timer;
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
	reply->wakeup_rply.errors.bits.UWB_Backoff = !!dwmInfo.backoff_timer;
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
		lora_rxInfo.lora_msg_index = 0;
		lora_rxInfo.new_data = LR_NO_VALID_DATA;
	}
	reply->op_twr_rply.data_status = dwmInfo.dw_ready | lora_rxInfo.new_data;// data_validity

			//reset UWB data valid flag
	dwmInfo.dw_ready = DW_NO_VALID_DATA;

	reply->op_twr_rply.uwb_distance = dwmInfo.distance;
	reply->op_twr_rply.lora_rssi = lora_rxInfo.rssi;
	reply->op_twr_rply.lora_snr = lora_rxInfo.snr;
	reply->op_twr_rply.counter = lora_rxInfo.lora_msg_index; //counter

			//reset Lora data every send
	lora_rxInfo.rssi = 0;
	lora_rxInfo.snr = 0;
	lora_rxInfo.lora_msg_index = 0;
	lora_rxInfo.new_data = LR_NO_VALID_DATA; // old data

	reply->op_twr_rply.tag_soc_battery = lora_rxInfo.tag_soc;
	reply->op_twr_rply.tag_errors = lora_rxInfo.tag_errors;
	reply->op_twr_rply.tag_errors.bits.no_LoRa = dwmInfo.tag_no_lora;




	reply->op_twr_rply.errors.all = 0;
	reply->op_twr_rply.errors.bits.no_LoRa = lora_rxInfo.no_lora;
	reply->op_twr_rply.errors.bits.no_UWB = dwmInfo.no_uwb;
	reply->op_twr_rply.errors.bits.UWB_Backoff = !!dwmInfo.backoff_timer;
	reply->op_twr_rply.errors.bits.wrong_UWB_ID = dwmInfo.wrong_ID;
	reply->op_twr_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
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
	reply->op_stt_rply.UWB_backoff = dwmInfo.backoff_timer;
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
	reply->op_stt_rply.battery = 0;
	if (dwmInfo.anchor_id == ANCHOR_WITH_BATTERY_CONNECTION){
		reply->op_stt_rply.battery = AbsoluteStateOfCharge(&hi2c2);
	}

	reply->op_stt_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);



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

	reply->op_sttd_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
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
