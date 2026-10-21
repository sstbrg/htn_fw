/*-----------------------------------------------------------------------
 *  @file    main.c
 *  @brief   Single-sided two-way ranging (SS TWR) responder example code
 *
 *           This is a simple code example which acts as the responder in a SS TWR distance measurement exchange. This application waits for a "poll"
 *           message (recording the RX time-stamp of the poll) expected from the "SS TWR initiator" example code (companion to this application), and
 *           then sends a "response" message to complete the exchange. The response message contains all the time-stamps recorded by this application,
 *           including the calculated/predicted TX time-stamp for the response message itself. The companion "SS TWR initiator" example application
 *           works out the time-of-flight over-the-air and, thus, the estimated distance between the two devices.
 *
 * @attention
 *
 * Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author Decawave
 */
#ifndef DW
#define DW

#include <stdint.h>
#include <inttypes.h> // Include this header for PRIu64 macro
#include "stdio.h"
#include "string.h"
#include "./decadriver/deca_device_api.h"
#include "./decadriver/deca_regs.h"
#include "../Lora/Inc/Lora.hpp"
#include "./platform/deca_spi.h"
#include "./platform/port.h"
#include "flash_handler.h"
/* Example application name and version to display on LCD screen. */
#define APP_NAME "SS TWR RESP v1.2"
#define LONG_PREAM
#define INDEX
#define PATCH_ANT
#define NEW_NTM

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct {
		uint8_t t_a_flag = TAG_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = SELF_ID;
		uint16_t des = 0;
		uint8_t tag_no_lora = 0xE1;
		uint32_t poll_rx_ts_id = 0;
		uint32_t resp_tx_ts_id = 0;
		uint8_t i18 = 0;
		uint8_t i19 = 0;
} twr_tag_msg_t;
#pragma pack (pop)


/* Default communication configuration. We use here EVK1000's mode 4. See NOTE 1 below. */



/* Default antenna delay values for 64 MHz PRF. See NOTE 2 below. */
#ifndef LONG_PREAM
	#define TIMEOUT_PACS 256/8*2//(config->txPreambLength / config->rxPAC) * 2 = preamble timeot duration of 2 full preabmles
#else

//	#define TIMEOUT_PACS 1024/32*2//(config->txPreambLength / config->rxPAC) * 2 = preamble timeot duration of 2 full preabmles
#define TIMEOUT_PACS (2048/64*4)//(config->txPreambLength / config->rxPAC) * 2 = preamble timeot duration of 2 full preabmle
#define RESP_RX_TIMEOUT_UUS 25000 //preamble 2048
#endif


//#define RX_TIMEOUT_UUS 10000//3600//3500
#define THRESHOLD_MS (70)  // deRange rx timeout

#ifdef PATCH_ANT
	#define TX_ANT_DLY  16500	// patch antenna- 16489
	#define RX_ANT_DLY  16500 	// patch antenna- 16489
#else
	#define TX_ANT_DLY  16436 //16436 	// small antenna- 16436
	#define RX_ANT_DLY  16436 //16436	// small antenna- 16436
#endif


#define RX_PC_IDX 5
#define RX_ANCHOR_IDX 6
#define TX_PC_IDX 7
#define TX_ANCHOR_IDX 8

#define TAG_ID_IDX7 7
#define TAG_ID_IDX8 8

#define MAX_POWER 0x1F1F1F1FUL //30.5 dB
//#define MAX_POWER 0x0; //15db
//#define MAX_POWER 0x67676767; //11db
//#define MAX_POWER 0x40404040; //10db
//#define MAX_POWER 0x60606060;//7.5db
//#define MAX_POWER 0x80808080;//5db
//#define MAX_POWER 0Xa0a0A0A0;//2.5DB
//#define MAX_POWER 0xC0C0C0C0; //0db
//#define MAX_POWER 0xE0E0E0E0; //turned off - 0db

#define LEDS_ON	1
#define LEDS_OFF	0
/* Frames used in the ranging process. See NOTE 3 below. */

/* Length of the common part of the message (up to and including the function code, see NOTE 3 below). */
//#define ALL_MSG_COMMON_LEN 9
/* Index to access some of the fields in the frames involved in the process. */
#define LORA_STATUS 9
#define T_A_FLAG 0
#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4
/* Frame sequence number, incremented after each transmission. */
//static uint8 frame_seq_nb = 0;

/* Buffer to store received messages.
 * Its size is adjusted to longest frame that this example code is supposed to handle. */
//#define RX_BUF_LEN 12

/* Hold copy of status register state here for reference so that it can be examined at a debug breakpoint. */


/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
 * 1 uus = 512 / 499.2 us and 1  us = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Delay between frames, in UWB microseconds. See NOTE 1 below. */
#define POLL_RX_TO_RESP_TX_DLY_UUS 5000//2000//700//1700//700

/* Timestamps of frames transmission/reception.
 * As they are 40-bit wide, we need to define a 64-bit int type to handle them. */
typedef unsigned long long uint64;

/* Declaration of static functions. */

extern dwt_config_t config;

uint64 get_system_timestamp(void);
int	dwmInit();
void dwConfig(dwt_config_t *config);
int dwRange(void);
void dwmStatus( void );

void dw_PreSleep(void);
void dw_PostSleep(void);

#ifdef __cplusplus
}
#endif


#endif
