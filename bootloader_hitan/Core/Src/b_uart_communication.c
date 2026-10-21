/*
 * b_uart_communication.c
 *
 *  Created on: Nov 4, 2025
 *      Author: LironAvrmov
 */

#include "main.h"
#include "b_uart_communication.h"
#include "flash.h"
#include "usart.h"

extern uint8_t HW_Version;

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



	void UART_Reply_Status(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply)
	{
		reply->header.opcode = msg->header.opcode;
		reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_STATUS_REPLY);
		strncpy(reply->op_stt_rply.SW, VERSION, sizeof(reply->op_stt_rply.SW));
		reply->op_stt_rply.HW = INVALID;
		reply->op_stt_rply.self_id = INVALID;
		reply->op_stt_rply.is_tag = INVALID;
		//	UWB
		reply->op_stt_rply.UWB_tag_status = INVALID;
		reply->op_stt_rply.UWB_backoff = INVALID;
		reply->op_stt_rply.UWB_last_status_reg = INVALID;
		reply->op_stt_rply.UWB_RxCounter = INVALID;
		reply->op_stt_rply.UWB_TxCounter = INVALID;
		// LoRa
		reply->op_stt_rply.LoRa_last_SNR = INVALID;
		reply->op_stt_rply.LoRa_last_RSSI = INVALID;
		reply->op_stt_rply.LoRa_RxCounter = INVALID;
		reply->op_stt_rply.LoRa_Freq = INVALID;
		//	errors
		reply->op_stt_rply.errors.all = INVALID;
		reply->op_stt_rply.fault_counter._0_Nmi_counter = INVALID;
		reply->op_stt_rply.fault_counter._1_HardFault_counter = INVALID;
		reply->op_stt_rply.fault_counter._2_MemManage_counter = INVALID;
		reply->op_stt_rply.fault_counter._3_BusFault_counter = INVALID;
		reply->op_stt_rply.fault_counter._4_UsageFault_counter = INVALID;
		reply->op_stt_rply.fault_counter._5_Watchdog_counter = INVALID;

		reply->op_stt_rply.checksum = checksum((uint8_t*) reply, reply->header.length);

		UART_SendMessage(reply, reply->header.length);
	}


void UART_Reply_Start_Prog(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_BOOT_START_PROG_REPLY);
	reply->start_prog_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_Write_Buffer(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_BOOT_WRITE_BUFFER_REPLY);
	reply->start_wr_buff_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
}
void UART_Reply_Header_File(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = msg->header.opcode;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_HEADER_FILE_REPLY);
	reply->op_hdr_file_rply.validation = HAL_OK;
	reply->op_hdr_file_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
}

void UART_Reply_error(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
	reply->header.opcode = UART_OPCODE_ERROR;
	reply->header.length = sizeof(S_HEADER) + sizeof(S_UART_ERROR_REPLY);
	reply->op_error_rply.expected_opcode = msg->header.opcode;
	reply->op_error_rply.checksum = checksum((uint8_t*) reply, reply->header.length);
	UART_SendMessage(reply, reply->header.length);
}
