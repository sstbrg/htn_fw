/*! ----------------------------------------------------------------------------
 *  @file    main.c
 *  @brief   Single-sided two-way ranging (SS TWR) initiator example code
 *
 *           This is a simple code example which acts as the initiator in a SS TWR distance measurement exchange. This application sends a "poll"
 *           frame (recording the TX time-stamp of the poll), after which it waits for a "response" message from the "DS TWR responder" example
 *           code (companion to this application) to complete the exchange. The response message contains the remote responder's time-stamps of poll
 *           RX, and response TX. With this data and the local time-stamps, (of poll TX and response RX), this example application works out a value
 *           for the time-of-flight over-the-air and, thus, the estimated distance between the two devices, which it writes to the LCD.
 *
 * @attention
 *
 * Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author Decawave
 */
//#ifdef EX_06A_DEF
#ifndef DW_H
#define DW_H

#include <stdio.h>
#include <string.h>

#include "stdio.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "./decadriver/deca_device_api.h"
#include "./decadriver/deca_regs.h"
//#include "ucpd.h"
#include "./platform/deca_spi.h"
#include "./platform/port.h"


/*
 user Defines
 */

#define TAG_FLAG 0x41
#define ANCHOR_FLAG 0xE0

#pragma pack(push, 1)
typedef struct {
		uint8_t t_a_flag = ANCHOR_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = 0;
		uint16_t des = 0;
		uint8_t i9 = 0xE0;
		uint8_t i10 = 0;
		uint8_t i11 = 0;
} twr_anchor_msg_t;
#pragma pack (pop)

#pragma pack(push, 1)
typedef struct {
		uint8_t t_a_flag = TAG_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = 0;
		uint16_t des = 0;
		uint8_t tag_no_lora = 0xE1;
		uint32_t poll_rx_ts_id = 0;
		uint32_t resp_tx_ts_id = 0;
		uint8_t i18 = 0;
		uint8_t i19 = 0;
} twr_tag_msg_t;
#pragma pack (pop)

/* Example application name and version to display on LCD screen. */
#define APP_NAME "SS TWR INIT v1.4"
#define LONG_PREAM
#define NEW_NTM
#define INDEX
/* Inter-ranging delay period, in milliseconds. */
#define RNG_DELAY_MS 0

//Calculation define
#define PRF_16Mhz 115.72
#define PRF_64Mhz 121.74

///// Timers
#define THRESHOLD_MS 		(70)  // deRange rx timeout
#define TIME_TOP_RANGE_MS	200 // max time is 200ms, now set to 1000 because slot Hterm
#define MIN_BACKOFF_MS		10

extern uint32_t _1ms_tick;

// status defines
#define NO_UWB 1
#define OK_UWB 0

/* Default communication configuration. We use here EVK1000's mode 4. See NOTE 1 below. */
#ifndef LONG_PREAM
static dwt_config_t config = {
    2,                //Channel number.
    DWT_PRF_64M,      //Pulse repetition frequency.
	DWT_PLEN_256,     //Preamble length. Used in TX only.
    DWT_PAC8,         //Preamble acquisition chunk size. Used in RX only.
    9,                //TX preamble code. Used in TX only.
    9,                //RX preamble code. Used in RX only.
    0,                //0 to use standard SFD, 1 to use non-standard SFD.
    DWT_BR_6M8,       //Data rate.
    DWT_PHRMODE_STD,  //PHY header mode.
    (256 + 1 + 8 - 8)     //SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only.
};


#else

/* dw configuration base on User manual with Longer SFD timeout	*/
//static dwt_config_t config = {
//		2,                //Channel number.
//		DWT_PRF_64M,      //Pulse repetition frequency.
//		DWT_PLEN_1024,     //Preamble length. Used in TX only.
//		DWT_PAC32,         //Preamble acquisition chunk size. Used in RX only.
//		9,                //TX preamble code. Used in TX only.
//		9,                //RX preamble code. Used in RX only.
//		0,                //0 to use standard SFD, 1 to use non-standard SFD.
//		DWT_BR_850K,       //Data rate.
//		DWT_PHRMODE_STD,  //PHY header mode.
//		1100 //(1024 + 64 - 32) // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only.
//		};
static dwt_config_t config = {
		2,                //Channel number.
		DWT_PRF_64M,      //Pulse repetition frequency.
		DWT_PLEN_2048,     //Preamble length. Used in TX only.
		DWT_PAC64,         //Preamble acquisition chunk size. Used in RX only.
		9,                //TX preamble code. Used in TX only.
		9,                //RX preamble code. Used in RX only.
		1,                //0 to use standard SFD, 1 to use non-standard SFD.
		DWT_BR_110K,       //Data rate.
		DWT_PHRMODE_STD,  //PHY header mode.
		2049 //(2048+1+64-64) // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only.
		};
#endif
/*			dw configuration base on User manual
 * dwt_config_t config = { 2,                //Channel number.
		DWT_PRF_64M,      //Pulse repetition frequency.
		DWT_PLEN_1024,     //Preamble length. Used in TX only.
		DWT_PAC32,         //Preamble acquisition chunk size. Used in RX only.
		9,                //TX preamble code. Used in TX only.
		9,                //RX preamble code. Used in RX only.
		1,                //0 to use standard SFD, 1 to use non-standard SFD.
		DWT_BR_850K,       //Data rate.
		DWT_PHRMODE_STD,  //PHY header mode.
		(1024 + 1 + 16) // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only.
		};
 */


#define TX_ANT_DLY 16550
#define RX_ANT_DLY 16500


#define MASK_PC_ID 0xF0
/* Length of the common part of the message (up to and including the function code, see NOTE 3 below). */
#define ALL_MSG_COMMON_LEN 9
/* Indexes to access some of the fields in the frames defined above. */
#define T_A_FLAG 0
#define ALL_MSG_SN_ID 2
#define RESP_MSG_POLL_RX_TS_ID 10
#define RESP_MSG_RESP_TX_TS_ID 14
#define RESP_MSG_TS_LEN 4

//////////////////////////////////////////

	//////////////
	//	ANCHOR	//
	//////////////

//Anchor
#define ANC_PC_ID_SRC 5			//Id Source-1
#define ANC_ANCHOR_ID_SRC 6		//Id Source-2

#define ANC_TAG_DES_1 7			//Id Destination-1
#define ANC_TAG_DES_2 8			//Id Destination-2

//Tag-Imposter
#define ANC_TAG_PC_ID_SRC 5	//Id Source-1
#define ANC_TAG_ANCHOR_ID_SRC 6//Id Source-2

#define ANC_TAG_PC_ID_DEST 7//Id Destination-1
#define ANC_TAG_ANCHOR_ID_DEST 8//Id Destination-2

/* Buffer to store received response message.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
//#define ANC_RX_BUF_LEN 20

	//////////////
	//	  TAG 	//
	//////////////
#define TAG_LORA_STATUS 9

#define TAG_SRC_1 5 	//Id Source-1
#define TAG_SRC_2 6		//Id Source-2

#define TAG_PC_ID_DES 7			//Id Destination-1
#define TAG_ANCHOR_ID_DES 8	 	//Id Destination-2

/* Buffer to store received response message.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
//#define TAG_RX_BUF_LEN 12


//////////////////////////////////////////

#define MAX_POWER 0x1F1F1F1FUL
#define LEDS_ON	1
#define LEDS_OFF 0
///* Frame sequence number, incremented after each transmission. */


/* Hold copy of status register state here for reference so that it can be examined at a debug breakpoint. */
static uint32 status_reg = 0;

#define DW_NO_VALID_DATA 0xF0
#define VALID_DATA 0x00

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2  s and 1  s = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Delay between frames, in UWB microseconds. See NOTE 1 below. */
#define ANC_POLL_TX_TO_RESP_RX_DLY_UUS 0

#define TAG_POLL_RX_TO_RESP_TX_DLY_UUS 5000//2000//700/1700

/* Receive response timeout. See NOTE 5 below. */
#ifndef LONG_PREAM
	#define RESP_RX_TIMEOUT_UUS 3600   // preamble 256

#else
	#define TIMEOUT_PACS (2048/64*4)//(config->txPreambLength / config->rxPAC) * 2 = preamble timeot duration of 2 full preabmles
	#define RESP_RX_TIMEOUT_UUS 25000
//	#define RESP_RX_TIMEOUT_UUS 10000  //preamble 1024
//	#define RESP_RX_TIMEOUT_UUS 6000  //preamble 1024
#endif


/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

/* Hold copies of computed time of flight and distance here for reference so that it can be examined at a debug breakpoint. */

extern LoraRxInfo_t lora_rxInfo;
extern DwmInfo_t dwmInfo;

#define  LORA_TAG_STATUS_ID 9

/* Declaration of static functions. */


int	dwmInit( void );				// 	Init DW
void dwmStatus( void );				//	initialization status
void dwt_configenable_user_accumem_read(bool bit_set);
void dwmDiagStatus( void );			//	Diagnostic status
void dwReadExtended_regs( void );	// 	more register for debug: RF_INFO, RF_FQUAL, RF_TTCKI_ID, RF_TTCKO_ID, SYS_STATE_ID, ACC_MEM
int dwRange(void);					// 	calculating range from tag
int dwTagMode(void);
void dwScanMode(void);		 		// 	searching for DW messages on air to set BackOFF
void dwConfig(dwt_config_t *config);//	Register configuration
static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts);
static uint64 get_rx_timestamp_u64(void);
static void resp_msg_set_ts(uint8 *ts_field, const uint64 ts);

#ifdef __cplusplus
}
#endif

#endif

