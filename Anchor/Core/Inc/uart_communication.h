/*
 * uart_communication.h
 *
 *  Created on: Apr 21, 2025
 *      Author: LironAvrmov
 */

#ifndef INC_UART_COMMUNICATION_H_
#define INC_UART_COMMUNICATION_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "stm32u5xx_it.h"


#define RX_UART_LEN 100
#define LEN_MSG_INX 1
#define CHECKSUM_MSG_INX(opcode) MESSAGE_LENGTH(opcode) -1

#define OPCODE_INDX 	0

#define VALIDATE_OK	1
#define VALIDATE_ERROR 0

#define RX_EMPTY 0

#define UART_RX_TIMEOUT 1  //(msec)

#define TAG_ID_LEN 2
#define TAG_ID_ID 2

#define  MAX_BYTES 255

#define DW_NO_VALID_DATA 0xF0
#define LR_NO_VALID_DATA 0x0F
#define VALID_DATA 0x00
#define LORA_TIMEOUT_DURATION 600

#define LINK_STATE_EXT		0x01	// extended UWB frames in use with this tag
#define LINK_STATE_OWNER	0x02	// this anchor runs the LoRa loop
#define LINK_STATE_PENDING	0x04	// a LoRa step is in its handshake
#define LINK_STATE_REVERT	0x08	// switched off: asking for maximum
#define LINK_STATE_COEXIST	0x10	// two stations or backoff armed: adaptation frozen (9K)
#define LINK_STATE_TAG_NLOS	0x20	// the tag reports its NLOS LDE preset (9G)

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
	uint8_t data_status; // data_validity
	uint32_t uwb_distance;
	int8_t lora_rssi;
	int8_t lora_snr;
	uint8_t counter;
	uint8_t tag_soc_battery;
	status_bits tag_errors;
	status_bits errors;
	// LINK_F_REPLY_EXT only (doc 9C/9F/9H); without it the reply ends here, as anc3.1.5
	uint32_t exchange_tick_ms;	// HAL_GetTick() at the reported response frame, 0 = no exchange this round
	int8_t tag_lora_tx_dbm;		// from the tag's response, LINK_LORA_UNKNOWN = unknown
	uint8_t tag_uwb_tx_level;	// from the tag's response, LINK_UWB_UNKNOWN = unknown
	int8_t lora_signal_rssi;	// SignalRssiPkt (dBm), same packet as lora_rssi
	uint8_t k_used;				// exchanges run this round (9F)
	uint8_t profile_used;		// 9H: 0, or 0xFF when the tag does not report it
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
	uint8_t		 	UWB_tag_status;
	uint32_t		UWB_backoff;
	uint32_t		UWB_last_status_reg;
	uint32_t		UWB_TxCounter;
	uint32_t		UWB_RxCounter;
	uint8_t  		is_tag;	//	0 = anchor
	Fault_Counter 	fault_counter;

	//	link_state (doc 9I), LINK_F_REPLY_EXT only; without it the reply ends here, as anc3.1.5
	uint8_t			link_features;		//	active feature mask (opcode 22)
	uint8_t			link_flags;			//	LINK_STATE_*
	uint8_t			profile_req;		//	9H: always 0
	uint8_t			profile_echo;		//	0xFF = not reported
	int8_t			req_lora_tx_dbm;	//	requested LoRa power, LINK_LORA_UNKNOWN = none
	uint8_t			req_uwb_tx_level;	//	UWB level of the last poll
	int8_t			tag_lora_tx_dbm;	//	last echo from the tag, LINK_LORA_UNKNOWN = unknown
	uint8_t			tag_uwb_tx_level;	//	LINK_UWB_UNKNOWN = unknown
	uint8_t			lde_preset;			//	0 = LOS (lde_cfg1), 1 = NLOS preset (9G)
	uint8_t			k_used;				//	last round's exchanges (9F)

	uint8_t 		checksum;
} S_UART_STATUS_REPLY;

typedef struct
{
	uint8_t UWB_enable_EVC; // EVC : Events Counter
	uint8_t checksum;
} S_UART_STATUS_UWB_DIAG_REQUEST;

typedef struct
{
		//	UWB Status
//	dwt_rxdiag_t UWB_diagnostics;
	uint8 	RX_FQUAL_buffer [8]; 	//all bytes of register
	uint16	stdNoise ;          // Standard deviation of noise
	uint16  firstPath ;         // First path index (10.6 bits fixed point integer)
	uint16	cirPwr;				// Channel Impulse Response Power.
	int16	rxLvl;				// Rx Level , somekind of Rssi
	uint16  rxPreamCount ;      // Count of preamble symbols accumulated
	uint16  firstPathAmp1 ;

// dwt_deviceentcnts_t counter;
	 uint16 PHE ;                    //number of received header errors
	 uint16 RSL ;                    //number of received frame sync loss events
	 uint16 CRCG ;                   //number of good CRC received frames
	 uint16 CRCB ;                   //number of bad CRC (CRC error) received frames
	 uint16 ARFE ;                   //number of address filter errors
	 uint16 OVER ;                   //number of receiver overflows (used in double buffer mode)
	 uint16 SFDTO ;                  //SFD timeouts
	 uint16 PTO ;                    //Preamble timeouts
	 uint16 RTO ;                    //RX frame wait timeouts
	 uint16 TXF ;                    //number of transmitted frames
	 uint16 HPW ;                    //half period warn
	 uint16 TXW ;                    //power up warn

	// LINK_F_REPLY_EXT only: latched with the diagnostics on the same valid exchange
	uint16_t lde_ppindx;			// LDE_PPINDX, peak path index
	uint16_t rxpacc_nosat;			// RXPACC_NOSAT
	uint32_t exchange_tick_ms;		// HAL_GetTick() at the response frame

	uint8_t checksum;
} S_UART_STATUS_UWB_DIAG_REPLY;


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
	uint16_t anch_id;
	uint16_t tag_id;
	uint16_t lora_timeout;
	status_bits errors;
	uint8_t checksum;
}S_UART_WAKEUP_REPLY;

/***************
 *
 * 	 error
 *
 ***************/
typedef struct{
	uint8_t expected_opcode;
	uint8_t checksum;
}S_UART_ERROR_REPLY;

/***************
 *
 * 	 BIT CONTROL
 *
 ***************/
typedef struct{
	uint8_t	tag_imposter;
	uint8_t checksum;
}S_UART_OP_BIT_CONTROL_REQUEST;


typedef struct{
	uint16_t self_id;
	uint8_t	tag_imposter;
	status_bits errors;
	uint8_t checksum;
}S_UART_OP_BIT_CONTROL_REPLY;

typedef struct{
	uint8_t checksum;
}S_UART_OP_UPDATE_FW_REQUEST;

typedef struct{
	uint8_t checksum;
}S_UART_OP_UPDATE_FW_REPLY;

/***************
 *
 * Reset Control
 *
 ***************/
typedef struct{
	uint8_t checksum;

}S_UART_OP_READ_RESET_COUNTER_REQUEST;

typedef struct{
	uint8_t checksum;

}S_UART_OP_RESET_RESET_COUNTER_REQUEST;


typedef struct{
	uint16_t reset_counter;
	uint8_t checksum;

}S_UART_OP_READ_RESET_COUNTER_REPLY;

/***************
 *
 * BackOFF Control
 *
 ***************/
typedef struct{
	uint8_t BackOFF_CTRL;
	uint8_t checksum;

}S_UART_OP_BACKOFF_CTRL_REQ;

typedef struct{
	uint8_t BackOFF_CTRL;
	uint8_t checksum;

}S_UART_OP_BACKOFF_CTRL_REPLY;


/***************
 *
 * 	 Read RAW UWB Register
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

/***************
 *
 * 	 Write RAW UWB Register
 *
 ***************/
#define SECTOR_SIZE 1
typedef struct
{
	uint8_t reg_id;
	uint8_t reg_offset;
	uint8_t buffer[SECTOR_SIZE]; // max write 1 memory sector of 32bits.
	uint8_t checksum;
} S_UART_OP_WRITE_UWB_RAW_REQUEST;

typedef struct
{
	uint8_t reg_id;
	uint8_t reg_offset;
	uint8_t checksum;
} S_UART_OP_WRITE_UWB_RAW_REPLY;


/*************************************/
/*************************************/
/*************************************/

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

		S_UART_OP_BIT_CONTROL_REQUEST op_bit_req;
		S_UART_OP_BIT_CONTROL_REPLY op_bit_rply;

		S_UART_STATUS_REQUEST op_stt_req;
		S_UART_STATUS_REPLY op_stt_rply;

		S_UART_STATUS_UWB_DIAG_REQUEST op_sttd_req;
		S_UART_STATUS_UWB_DIAG_REPLY op_sttd_rply;

		S_UART_OP_UWB_RAW_REQUEST op_uwb_raw_req;
		S_UART_OP_UWB_RAW_REPLY op_uwb_raw_rply;

		S_UART_OP_WRITE_UWB_RAW_REQUEST op_write_uwb_raw_req;
		S_UART_OP_WRITE_UWB_RAW_REPLY op_write_uwb_raw_rply;

		S_UART_OP_UPDATE_FW_REQUEST  op_update_fw_req;
		S_UART_OP_UPDATE_FW_REPLY  op_update_fw_rply;

		S_UART_OP_READ_RESET_COUNTER_REQUEST  op_read_reset_counter_req;
		S_UART_OP_RESET_RESET_COUNTER_REQUEST  op_reset_reset_counter_req;
		S_UART_OP_READ_RESET_COUNTER_REPLY  op_read_reset_counter_rply;

		S_UART_OP_BACKOFF_CTRL_REQ op_backoff_ctrl_req;
		S_UART_OP_BACKOFF_CTRL_REPLY op_backoff_ctrl_rply;

		S_UART_ERROR_REPLY op_error_rply;

		uint8_t buff[MAX_BYTES];
	};
} S_UART_MESSAGE;


typedef enum
{
	/* 1*/UART_OPCODE_BURN = 1,
	/* 2*/UART_OPCODE_SET_ID, 			//
	/* 3*/RESERVE_3,		//	Tag Only
	/* 4*/RESERVE_4,	//	Anchor Only
	/* 5*/UART_OPCODE_SEARCH_TWR,
	/* 6*/UART_OPCODE_WAKEUP_MODE, 			//	???
	/* 7*/RESERVE_7,
	/* 8*/UART_OPCODE_BIT_CONTROL,
	/* 9*/UART_OPCODE_LOOPBACK,				//	For Test
	/*10*/UART_OPCODE_RESET,
	/*11*/UART_OPCODE_STATUS,				//	Send status to PC
	/*12*/UART_OPCODE_READ_UWB_RAW,
	/*13*/UART_OPCODE_DIAGNOSTIC_STATUS, //status with more UWB diagnostic parameters
	/*14*/UART_OPCODE_WRITE_UWB_RAW,
	/*15*/UART_OPCODE_UPDATE_FW,
	/*16*/UART_OPCODE_RESET_RESET_COUNTER,
	/*17*/UART_OPCODE_READ_RESET_COUNTER,
	/*19*/UART_OPCODE_BACKOFF_CTRL = 19,	// control of dwScanMode. enable or disable; optional {top_ms, min_ms}
	/*20*/UART_OPCODE_ANT_DELAY = 20,		// per-device antenna delays {tx, rx} in flash; empty = read
	/*21*/UART_OPCODE_READ_CIR_WINDOW = 21,	// CIR window latched with the last valid range
	/*24*/UART_OPCODE_TAG_CONFIG = 24,		// configuration item to the tag over LoRa, UWB as backup; empty = status
	/*23*/UART_OPCODE_XTAL_TRIM = 23,		// DW1000 crystal trim {trim 0..31, 0xFF = clear} in flash; empty = read
	/*22*/UART_OPCODE_SET_LINK_FEATURES = 22,	// LinkCfg_t (doc 9I), a shorter payload sets the leading fields; empty = read

	/*EE*/UART_OPCODE_ERROR = 0xEE,

} GET_UART_OPCODE;


typedef enum
{
	OPCODE_INDEX, LENGTH_INDEX,
} MESSAGE_STRUCTURE;

uint8_t Validate_UART(void *buffer, uint8_t expected_len);
uint8_t checksum(const uint8_t *uart_tx_buffer, const uint8_t length);

void UART_Reply_Burn_msg	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Set_Self_ID	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply, uint8_t error);
void UART_Reply_Operational	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Search_Twr	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Wake_up		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_BIT_Control	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Status		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Status_UWB_Diag	(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_error		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_UWB_Raw		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_Write_UWB_Raw(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);
void UART_Reply_UPDATE_FW(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);

void UART_Reply_Reset_Counter		(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply, uint8_t error);
void UART_Reply_BackOFF_CTRL(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply);

bool UART_MessageArrived(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UART_COMMUNICATION_H_ */
