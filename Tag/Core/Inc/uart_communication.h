/*
 * uart_communication.h
 *
 *  Created on: Apr 21, 2025
 *      Author: LironAvrmov
 */

#ifndef INC_UART_COMMUNICATION_H_
#define INC_UART_COMMUNICATION_H_
#include "./decadriver/deca_regs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RX_UART_LEN 100
#define LEN_MSG_INX 1
#define CHECKSUM_MSG_INX(opcode) MESSAGE_LENGTH(opcode) -1

#define OPCODE_INDX 	0

#define VALIDATE_OK	1
#define VALIDATE_ERROR 0

#define RX_EMPTY 0

#define UART_RX_TIMEOUT 1  //(msec)

#define TAG_ID_LEN 2
#define TAG_ID_IDX 2

#define  MAX_BYTES 255




#pragma pack (1)

typedef struct
{
	uint8_t opcode;
	uint8_t length;
} S_HEADER;



/**********************************
 *
 * 	 BURN - Start Programming = Erase all
 *
 **********************************/

typedef struct
{
	uint8_t checksum;
} S_USRT_START_PROGRAMMING_REQUEST;

typedef struct
{
	uint16_t current_id;
	uint8_t is_tag;
	status_bits errors;
	uint8_t checksum;
} S_USRT_START_PROGRAMMING_REPLY;


/***************
 *
 * 	 ID
 *
 ***************/

typedef struct
{
	uint16_t new_id;
	uint8_t checksum;

} S_UART_SET_ID_REQUST;

typedef struct
{
	uint16_t self_id;
	uint8_t is_tag;
	status_bits errors;
	uint8_t checksum;

} S_UART_SET_ID_REPLY;
/***************
 *
 * 	 TWR
 *
 ***************/
typedef struct
{
	uint16_t tag_id;
	uint8_t checksum;

} S_UART_OP_TWR_REQUEST;


typedef struct
{
	uint16_t anchor_id;
	uint16_t tag_id;
	uint32_t uwb_distance;
	int8_t lora_rssi;
	int8_t lora_snr;
	uint8_t lora_counter;
	uint8_t tag_battery;
	status_bits tagVerrors;
	status_bits errors;
	uint8_t checksum;
} S_UART_OP_TWR_REPLY;

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
	uint8_t 		battery;
	uint32_t		LoRa_RxCounter;
	uint32_t		LoRa_Freq;

	//	UWB Status
//	dwt_rxdiag_t 	UWB_disagnostics;
	uint8_t		 	UWB_tag_status;
	uint32_t		UWB_backoff;
	uint32_t		UWB_last_status_reg;
	uint32_t		UWB_TxCounter;
	uint32_t		UWB_RxCounter;
	uint8_t  		is_tag;	//	0 = anchor
	uint32_t		reserved[6];
	uint8_t checksum;
} S_UART_STATUS_REPLY;


/***************
 *
 * 	 Wake up without key
 *
 ***************/
typedef struct{
	uint16_t tag_id;
	uint8_t  Wakeup_HW;
	uint8_t checksum;
}S_UART_WAKEUP_REQUEST;

typedef struct{
	uint16_t anchor_id;
	uint16_t tag_id;
	uint16_t lora_timeout;
	status_bits errors;
	uint8_t checksum;
}S_UART_WAKEUP_REPLY;

/***************
 *
 * 	 RAW UWB Register
 *
 ***************/
#define MAX_RAW_BUFF 200
typedef struct
{
	uint8_t reg_id;
	uint16_t reg_offset;
	uint8_t reg_len;
	uint8_t checksum;
} S_UART_OP_UWB_RAW_REQUEST;

typedef struct
{
	uint8_t reg_id;
	uint16_t reg_offset;
	uint8_t reg_len;
	uint8_t  buffer[MAX_RAW_BUFF];
	uint8_t checksum;
} S_UART_OP_UWB_RAW_REPLY;

typedef struct{
	uint8_t checksum;
}S_UART_OP_UPDATE_FW_REQUEST;

typedef struct{
	uint8_t checksum;
}S_UART_OP_UPDATE_FW_REPLY;


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
		S_USRT_START_PROGRAMMING_REQUEST start_prog_req;
		S_USRT_START_PROGRAMMING_REPLY start_prog_rply;

		S_UART_SET_ID_REQUST set_id_req;
		S_UART_SET_ID_REPLY set_id_rply;

		S_UART_OP_TWR_REQUEST op_twr_req;
		S_UART_OP_TWR_REPLY op_twr_rply;

		S_UART_WAKEUP_REQUEST wakeup_req;
		S_UART_WAKEUP_REPLY wakeup_rply;

		S_UART_STATUS_REQUEST op_stt_req;
		S_UART_STATUS_REPLY op_stt_rply;

		S_UART_OP_UWB_RAW_REQUEST op_uwb_raw_req;
		S_UART_OP_UWB_RAW_REPLY op_uwb_raw_rply;

		S_UART_OP_UPDATE_FW_REQUEST  op_update_fw_req;
		S_UART_OP_UPDATE_FW_REPLY  op_update_fw_rply;

		S_UART_ERROR_REPLY op_error_rply;

		uint8_t buff[MAX_BYTES];
	};
} S_UART_MESSAGE;


typedef enum
{
	/* 1*/UART_OPCODE_BURN = 1,
	/* 2*/UART_OPCODE_SET_ID, 			//
	/* 3*/RESERVED_3,		//	Tag Only
	/* 4*/RESERVED_4,	//	Anchor Only
	/* 5*/UART_OPCODE_SEARCH_TWR,
	/* 6*/UART_OPCODE_WAKEUP_MODE, 			//	???
	/* 7*/RESERVED_7,
	/* 8*/UART_OPCODE_BIT_CONTROL,
	/* 9*/UART_OPCODE_LOOPBACK,				//	For Test
	/* 10*/UART_OPCODE_RESET,
	/* 11*/UART_OPCODE_STATUS,				//	Send status to PC
	/*12*/UART_OPCODE_READ_UWB_RAW,
	/*13*/RESERVED_13,
	/*14*/RESERVED_14,
	/*15*/UART_OPCODE_UPDATE_FW,


	/*EE*/UART_OPCODE_ERROR = 0xEE,

} GET_UART_OPCODE;


typedef enum
{
	OPCODE_INDEX, LENGTH_INDEX,
} MESSAGE_STRUCTURE;

#pragma pack ()

uint8_t Validate_UART(void *buffer, uint8_t expected_len);
uint8_t checksum(const uint8_t *uart_tx_buffer, const uint8_t length);

void UART_Reply_Burn_msg	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Set_Self_ID	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply, uint8_t error);
void UART_Reply_Operational	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Search_Twr	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Wake_up		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Status		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_error		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_UWB_Raw		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_UPDATE_FW(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);


#ifdef __cplusplus
}
#endif

#endif /* INC_UART_COMMUNICATION_H_ */
