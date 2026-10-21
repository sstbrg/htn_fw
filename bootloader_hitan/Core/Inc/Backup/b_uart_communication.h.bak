/*
 * b_uart_communication.h
 *
 *  Created on: Nov 4, 2025
 *      Author: LironAvrmov
 */


#ifndef INC_B_UART_COMMUNICATION_H_
#define INC_B_UART_COMMUNICATION_H_



#define RX_UART_LEN 100
#define LEN_MSG_INX 1
#define CHECKSUM_MSG_INX(opcode) MESSAGE_LENGTH(opcode) -1

#define OPCODE_INDX 	0

#define VALIDATE_OK	1
#define VALIDATE_ERROR 0

#define RX_EMPTY 0

#define UART_RX_TIMEOUT 1  //(msec)

#define  MAX_BYTES 512
#define INVALID 0xFF

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

typedef struct{
	uint32_t _0_Nmi_counter;
	uint32_t _1_HardFault_counter;
	uint32_t _2_MemManage_counter;
	uint32_t _3_BusFault_counter;
	uint32_t _4_UsageFault_counter;
	uint32_t _5_Watchdog_counter;
}Fault_Counter;


#pragma pack (push,1)

typedef struct
{
	uint8_t opcode;
	uint8_t length;
} S_HEADER;

/**********
 *
 * start programming
 *
 *********/
typedef struct {
	uint8_t checksum;
}S_UART_BOOT_START_PROG_REQUEST;

typedef struct {
	uint8_t checksum;
}S_UART_BOOT_START_PROG_REPLY;


/**********
 *
 *   write buffer
 *
 *********/
typedef struct {
	uint8_t buffer[FLASH_TOTAL_BUFFER_SIZE];
}S_UART_BOOT_WRITE_BUFFER_REQUEST ;

typedef struct {
	uint8_t checksum;
}S_UART_BOOT_WRITE_BUFFER_REPLY;

/**********
 *
 * header file
 *
 *********/
typedef struct{
	uint32_t crc_file;
	uint32_t file_len;
	uint8_t checksum;
}S_UART_HEADER_FILE_REQUEST;

typedef struct{
	uint8_t validation;; // '0' - valid
	uint8_t checksum;
}S_UART_HEADER_FILE_REPLY;

/***************
 *
 * 	 Status
 *
 ***************/
typedef struct
{
	uint8_t checksum;
} S_UART_STATUS_REQUEST;


typedef struct
{
	char SW[9];	//	null terminated string (last char == 0)
	uint8_t HW;	//	from HW resistors
	uint16_t self_id;

	//	LoRa status
	int8_t			LoRa_last_RSSI;
	int8_t			LoRa_last_SNR;
	status_bits 	errors;
	uint8_t 		spare;
	uint32_t		LoRa_RxCounter;
	uint32_t		LoRa_Freq;

	//	UWB Status
	uint8_t		 	UWB_tag_status;
	uint32_t		UWB_backoff;
	uint32_t		UWB_last_status_reg;
	uint32_t		UWB_TxCounter;
	uint32_t		UWB_RxCounter;
	uint8_t  		is_tag;	//	0 = anchor
	Fault_Counter 	fault_counter;

	uint8_t 		checksum;
} S_UART_STATUS_REPLY;

/***************
 *
 * 	 error
 *
 ***************/
typedef struct{
	uint8_t expected_opcode;
	uint8_t checksum;
}S_UART_ERROR_REPLY;


typedef struct
{
	S_HEADER header;
	union
	{
		S_UART_BOOT_START_PROG_REQUEST start_prog_req;
		S_UART_BOOT_START_PROG_REPLY start_prog_rply;

		S_UART_BOOT_WRITE_BUFFER_REQUEST start_wr_buff_req;
		S_UART_BOOT_WRITE_BUFFER_REPLY start_wr_buff_rply;

		S_UART_HEADER_FILE_REQUEST op_hdr_file_req;
		S_UART_HEADER_FILE_REPLY op_hdr_file_rply;

		S_UART_STATUS_REQUEST op_stt_req;
		S_UART_STATUS_REPLY op_stt_rply;

		S_UART_ERROR_REPLY op_error_rply;

		uint8_t buff[MAX_BYTES];
	};
} S_UART_MESSAGE;


typedef enum
{
	 /*10*/UART_OPCODE_RESET = 10,
	 /*11*/UART_OPCODE_STATUS = 11,
	 /*16*/UART_OPCODE_START_PROG = 16,
	 /*17*/UART_OPCODE_WRITE_BUFFER,
	 /*18*/UART_OPCODE_HEADER_FILE,

	/*EE*/UART_OPCODE_ERROR = 0xEE,

} GET_UART_OPCODE;


typedef enum
{
	OPCODE_INDEX, LENGTH_INDEX,
} MESSAGE_STRUCTURE;



#pragma pack(pop)

uint8_t Validate_UART(void *buffer, uint8_t expected_len);
uint8_t checksum(const uint8_t *uart_tx_buffer, const uint8_t length);

void UART_Reply_Status(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_SendMessage(void *buff, uint32_t buff_len);
void UART_Reply_Start_Prog(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Write_Buffer(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Header_File(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_error(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);


#endif /* INC_B_UART_COMMUNICATION_H_ */
