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
		uint16_t src = 0;
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
		uint16_t src = 0;
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
		uint16_t src = 0;
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
// _1ms_tick is a true 1 ms since anc3.2.0 (it was 2.05 ms): the values keep the old real timings.
#define THRESHOLD_MS 		(144)  // dwRange rx timeout (70 old ticks)
#define TIME_TOP_RANGE_MS	410	// backoff = random() % top + min: 21..430 ms, as 200 / 10 old ticks
#define MIN_BACKOFF_MS		21
#define OLD_TICKS_TO_MS(t)	((uint16_t)(((uint32_t)(t) * 2051u + 500u) / 1000u))	// opcode 19 window units

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


extern uint16_t backoff_top_ms;		// backoff window: random() % top + min (runtime, opcode 19)
extern uint16_t backoff_min_ms;
extern uint8_t lde_cfg1;			// LDE_CFG1 (NTM/PMULT), re-applied by dwConfig()

int	dwmInit( void );				// 	Init DW
void dwSetFrameIds(void);			//	frame ids from dwmInfo.mine_pc_id/anchor_id (boot and SET_ID)
void dwApplyAntennaDelays(void);	//	per-device antenna delays from flash, or the defaults
void dwApplyXtalTrim(void);			//	per-device DW1000 crystal trim from flash, or the OTP/default
void dwmStatus( void );				//	initialization status
void dwRecover( void );				//	main loop: bounded re-initialisation while no_uwb
void dwt_configenable_user_accumem_read(bool bit_set);
void dwmDiagStatus( void );			//	Diagnostic status
void dwReadExtended_regs( void );	// 	more register for debug: RF_INFO, RF_FQUAL, RF_TTCKI_ID, RF_TTCKO_ID, SYS_STATE_ID, ACC_MEM
int dwRange(void);					// 	calculating range from tag
void dwLatchExchange(void);			//	PPINDX, RXPACC_NOSAT and CIR window of the frame just received
int dwRangeRound(uint8_t k_req);		//	SEARCH_TWR: 1..K exchanges (9F), shortest valid range kept
uint8_t dwLdeCfg1(void);			//	LDE_CFG1 in force: lde_cfg1 or the 9G NLOS preset
bool Link_WantHello(void);
bool Link_Coexisting(void);
void Link_IdleTick(void);
void dwProfileGround(void);			//	range profile 0 now (FOTA)
void dwRadioWatch(void);			//	main loop: DW1000 registers must match the profile in use
void dwNoteConfig(void);			//	take channel / PAC tuning as written (after raw writes)
void Link_BeforePoll(void);
void Link_FillExtPoll(twr_anchor_ext_msg_t *p);
bool Link_ItemUwbDue(void);					// put the pending item in the next extended poll (v2)
void Link_ItemFillPoll(twr_anchor_ext2_msg_t *p);
void Link_ItemOnUwb(const twr_tag_ext2_msg_t *r);
void Link_OnResponse(const twr_tag_ext_msg_t *r, bool ext_poll, bool ext_resp, bool hello);
void Link_OnExchange(uint8_t result, bool first, bool ext_poll, bool ext_resp);
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

