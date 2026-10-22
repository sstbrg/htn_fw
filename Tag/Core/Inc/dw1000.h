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

/* UWB frames (doc section 9, rule zero). The legacy frames are what every firmware speaks: a tag
 * drops any poll that is not a legacy or extended poll by length, an older tag any poll that is not
 * exactly 12 bytes, and an older anchor any response over 20 bytes. So extended frames are used
 * only after both ends have shown they are new:
 *   1. a new anchor with link adaptation on sends legacy polls with i9 = LINK_HELLO (older anchors
 *      send 0xE0; a legacy poll always means "tag at maximum");
 *   2. a new tag answers a hello with a legacy response whose byte 9 has LINK_CAPABLE set;
 *   3. only then does the anchor send extended polls, and the tag answers an extended poll (and only
 *      that) with an extended response. Two unanswered extended polls, or a legacy answer, put the
 *      anchor back on legacy frames for LINK_EXT_HOLD_MS. */
#define LINK_HELLO          0x4C    // legacy poll byte 9: "I can do extended frames"
#define LINK_EXT_MARK       0x4D    // extended poll byte 9
#define LINK_CAPABLE        0x80    // legacy response byte 9 (bit0 stays the no-LoRa flag)
#define LINK_CAPABLE_MULTI  0x40    //   with LINK_CAPABLE: polls from two stations in the last 2 s (9K)
#define LINK_RESP_NO_LORA   0x01    // extended response byte 9 flags
#define LINK_RESP_PENDING   0x02    //   a quieter LoRa request waits for its confirm
#define LINK_RESP_MULTI     0x04    //   polls from two stations in the last 2 s: no adaptation (9K)
#define LINK_RESP_LDE_NLOS  0x08    //   the tag's LDE runs the NLOS preset (9G)
#define LINK_CTRL_LDE_NLOS  0x10    // extended poll ctrl: tag LDE to the NLOS preset; clear = LOS (9G)
#define LINK_LDE_NLOS_CFG1  0x67    // NTM 7 / PMULT 3 (APS006 Pt 2); LOS is 0x6D
#define LINK_MULTI_MS       2000    // a second station counts this long after its last poll
#define LINK_LORA_KEEP      0x7F    // extended poll: no LoRa request; response: tag has no LoRa

#pragma pack(push, 1)
typedef struct {
		uint8_t t_a_flag = ANCHOR_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = 0;
		uint16_t des = 0;
		uint8_t i9 = 0xE0;	// 0xE0, LINK_HELLO or LINK_EXT_MARK
		uint8_t i10 = 0;
		uint8_t i11 = 0;
} twr_anchor_msg_t;

typedef struct {			// extended poll: legacy bytes 0-9, then the request (9J handshake)
		uint8_t t_a_flag = ANCHOR_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = 0;
		uint16_t des = 0;
		uint8_t i9 = LINK_EXT_MARK;
		uint8_t seq = 0;			// LoRa request sequence number (repeated = keep-alive)
		int8_t req_lora_dbm = LINK_LORA_KEEP;	// requested LoRa power, LINK_LORA_KEEP = no request
		uint8_t req_uwb_level = 0;	// UWB level for the response to this poll (0 = maximum)
		uint8_t confirm_seq = 0;	// confirms the acknowledged request, 0 = none
		uint8_t ctrl = 0;			// bits 3..0 profile (9H, always 0), LINK_CTRL_LDE_NLOS (9G)
		uint8_t crc0 = 0;
		uint8_t crc1 = 0;
} twr_anchor_ext_msg_t;

typedef struct {
		uint8_t t_a_flag = TAG_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = SELF_ID;
		uint16_t des = 0;
		uint8_t tag_no_lora = 0xE1;	// legacy: bit0 no LoRa (+ LINK_CAPABLE); extended: LINK_RESP_* flags
		uint32_t poll_rx_ts_id = 0;
		uint32_t resp_tx_ts_id = 0;
		uint8_t i18 = 0;
		uint8_t i19 = 0;
} twr_tag_msg_t;

typedef struct {			// extended response: legacy bytes 0-17, then the tag's state
		uint8_t t_a_flag = TAG_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = SELF_ID;
		uint16_t des = 0;
		uint8_t tag_no_lora = 0;	// LINK_RESP_* flags
		uint32_t poll_rx_ts_id = 0;
		uint32_t resp_tx_ts_id = 0;
		uint8_t ack_seq = 0;		// request the tag will apply on confirm (or applied, when louder)
		int8_t lora_dbm = 0;		// LoRa power in use, LINK_LORA_KEEP = no LoRa
		uint8_t uwb_level = 0;		// UWB level of this response
		uint8_t profile_used = 0;	// 9H, always 0 for now
		uint8_t crc0 = 0;
		uint8_t crc1 = 0;
} twr_tag_ext_msg_t;
#pragma pack (pop)

/* Extended frames v2 (tag3.2.1 and later; the hello answer's bits 5..1 carry the version): v1 plus
 * one configuration item (LITEM_*, main.h), the backup path for what normally goes over LoRa. */
#define LINK_VERSION        3       // 2: extended frames v2 (tag3.2.1); 3: range profiles (#7)
#define LINK_VER_OF(b)      (((b) >> 1) & 0x1F)

#pragma pack(push, 1)
typedef struct {
		uint8_t t_a_flag = ANCHOR_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = 0;
		uint16_t des = 0;
		uint8_t i9 = LINK_EXT_MARK;
		uint8_t seq = 0;
		int8_t req_lora_dbm = LINK_LORA_KEEP;
		uint8_t req_uwb_level = 0;
		uint8_t confirm_seq = 0;
		uint8_t ctrl = 0;
		uint8_t item_seq = 0;		// configuration item (backup path)
		uint8_t item = 0xFF;		// LITEM_*, 0xFF = none
		uint16_t item_value = 0;
		uint8_t crc0 = 0;
		uint8_t crc1 = 0;
} twr_anchor_ext2_msg_t;

typedef struct {
		uint8_t t_a_flag = TAG_FLAG;
		uint8_t i1 = 0x88;
		uint8_t msd_index = 0;
		uint8_t i3 = 0xCA;
		uint8_t i4 = 0xDE;
		uint16_t src = SELF_ID;
		uint16_t des = 0;
		uint8_t tag_no_lora = 0;
		uint32_t poll_rx_ts_id = 0;
		uint32_t resp_tx_ts_id = 0;
		uint8_t ack_seq = 0;
		int8_t lora_dbm = 0;
		uint8_t uwb_level = 0;
		uint8_t profile_used = 0;
		uint8_t item_seq = 0;		// item answered (0xFF item = none)
		uint8_t item = 0xFF;
		uint16_t item_value = 0;	// read-back
		uint8_t item_flags = 0;		// LACK_F_*
		uint8_t crc0 = 0;
		uint8_t crc1 = 0;
} twr_tag_ext2_msg_t;
#pragma pack (pop)

// The legacy sizes are what older firmware checks: they must never change.
static_assert(sizeof(twr_anchor_msg_t) == 12, "legacy poll is 12 bytes");
static_assert(sizeof(twr_tag_msg_t) == 20, "legacy response is 20 bytes");
static_assert(sizeof(twr_anchor_ext_msg_t) == 17 && sizeof(twr_tag_ext_msg_t) == 24, "extended frames");
static_assert(sizeof(twr_anchor_ext2_msg_t) == 21 && sizeof(twr_tag_ext2_msg_t) == 29, "extended frames v2");

#define LINK_LORA_MAX_DBM   22
#define LINK_LORA_MIN_DBM   (-9)    // SX1262 high-power PA floor
#define LINK_UWB_LEVELS     8       // level 0 = maximum, each level 3 dB lower
#define LINK_REVERT_MS      500     // no owner poll this long after a quieter step: previous power
#define LINK_SILENCE_MS     1000    // no owner poll this long: maximum


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
void dwApplyAntennaDelays(void);	//	per-device antenna delays from flash, or the defaults
void dwApplyXtalTrim(void);			//	per-device DW1000 crystal trim from flash, or the OTP/default
int dwRange(void);
void dwmStatus( void );
void dwRecover( void );
bool dwMulti( void );
void dwProfileGround( void );		//	range profile 0 now (FOTA, resets)
uint8_t dwProfileCur( void );				//	polls from two stations in the last LINK_MULTI_MS (9K)
uint16_t dwLinkItem(uint16_t src, uint8_t seq, uint8_t item, uint16_t value, uint8_t *flags);				//	main loop: bounded re-initialisation while no_uwb

void dw_PreSleep(void);
void dw_PostSleep(void);

#ifdef __cplusplus
}
#endif


#endif
