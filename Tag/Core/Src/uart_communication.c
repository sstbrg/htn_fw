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
#include "rrc2040driver.h"

extern LoraRxInfo_t lora_rxInfo;
extern DwmInfo_t dwmInfo;
extern BMI088 bmi;
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

uint8_t Validate_UART(void *buffer, uint8_t expected_len)
{
	uint8_t *buff = (uint8_t*) buffer;
	uint8_t len = buff[LENGTH_INDEX];
	if (len != expected_len)
		return VALIDATE_ERROR;
	uint8_t calculatedChkSm = checksum(buffer, len);
	uint8_t receivedChkSm = buff[expected_len - 1];
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
	reply->start_prog_rply.current_id = SELF_ID;
	reply->start_prog_rply.is_tag = IS_TAG;	//	TAG SW
	reply->start_prog_rply.errors.all = 0;
	reply->start_prog_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Set_Self_ID(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply, uint8_t error)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_SET_ID_REPLY);
	reply->set_id_rply.self_id = SELF_ID;
	reply->set_id_rply.is_tag = IS_TAG;	//	TAG SW
	reply->set_id_rply.errors.bits.flash_error = error;
	reply->set_id_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Status(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
{
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_STATUS_REPLY);
	strncpy(reply->op_stt_rply.SW, VERSION, sizeof(reply->op_stt_rply.SW));
	reply->op_stt_rply.HW = HW_Version;
	reply->op_stt_rply.self_id = SELF_ID;
	reply->op_stt_rply.is_tag = 1;

	//	UWB
//	reply->op_stt_rply.UWB_disagnostics = dwmInfo.disagnostics;
	reply->op_stt_rply.UWB_tag_status = dwmInfo.tag_no_lora;
	reply->op_stt_rply.UWB_backoff = dwmInfo.backoff_timer;
	reply->op_stt_rply.UWB_last_status_reg = dwmInfo.statusReg;
	// LoRa
	reply->op_stt_rply.LoRa_last_SNR = lora_rxInfo.snr;
	reply->op_stt_rply.LoRa_last_RSSI = lora_rxInfo.rssi;
	reply->op_stt_rply.LoRa_RxCounter = lora_rxInfo.rx_counter;
	reply->op_stt_rply.LoRa_Freq = lora_rxInfo.freq;
	//	errors
	reply->op_stt_rply.errors.all = 0;
	reply->op_stt_rply.errors.bits.no_LoRa = lora_rxInfo.no_lora;
	reply->op_stt_rply.errors.bits.no_UWB = dwmInfo.no_uwb;
	reply->op_stt_rply.errors.bits.no_IMU = bmi.no_IMU;
	reply->op_stt_rply.battery = AbsoluteStateOfCharge(&hi2c2);

	reply->op_stt_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

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

	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_UWB_RAW_REPLY);

	reply->op_uwb_raw_rply.reg_id =	msg->op_uwb_raw_req.reg_id;
	reply->op_uwb_raw_rply.reg_offset =	msg->op_uwb_raw_req.reg_offset;
	reply->op_uwb_raw_rply.reg_len =	msg->op_uwb_raw_req.reg_len;
	if (reply->op_uwb_raw_rply.reg_len > sizeof(reply->op_uwb_raw_rply.buffer))
		reply->op_uwb_raw_rply.reg_len = sizeof(reply->op_uwb_raw_rply.buffer);


	memset(reply->op_uwb_raw_rply.buffer, 0, sizeof(reply->op_uwb_raw_rply.buffer));
	if(msg->op_uwb_raw_req.reg_id == ACC_MEM_ID){
		dwt_readaccdata(reply->op_uwb_raw_rply.buffer,reply->op_uwb_raw_rply.reg_len ,reply->op_uwb_raw_rply.reg_offset );
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

void UART_Reply_UPDATE_FW(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_OP_UPDATE_FW_REPLY);
	reply->op_update_fw_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply,reply->header.length);
}



