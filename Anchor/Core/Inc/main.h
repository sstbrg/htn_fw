/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>

#include "BMI088.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "flash_handler.h"

//#include "uart_communication.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/*
 * 1.5.3	-	added Get Status command from pc: 0xFE on COM
 * C1.6.01	-	1.	changed spi2 clock config.
 * 				2.	added dwmInfo.uart_income == true to uart3 irq catch to stop dwScanMode()
 * C1.6.02 	-	added missing opcode parts after merge
 * C1.6.03	-	1.	changed self_id parsing. 16:2 - PC_ID, 1:0 - anchor_id.
 * 				2.	anchor id is read from gpio upon power up.
 * 				3.	changed pc_id value doesnt requires stm reset anymore.
 * C1.6.05	-	changed BackOff, TagImposter pc_id and anchor_id split and parsing to twr arrays
 * C1.6.06	-	1.changed anchor struct for twr messages
 * 				2.changed SELECTOR_STAT gpio direction to input.
 * C1.6.07	-	fixed DMA disappearing from ioc.
 * C1.6.08	-	fixed id parsing in lora
 * C1.6.09		1. added rtc
 * 				2. added iwdg for 2 sec
 * 				3. added more hardfault-s handling with nvic(reset)
 * 				4. fixe dma tx
 * 				5. set SELECTOR_STAT changed to input
 * 				6. fix hard-fult issue (UWB read overflow)
 * C1.6.10	-	1. dwScan (Backoff) change to msg struct instead of byte buff (Caused error little/big endian)
 *              2. some cleanup
 * C1.6.11  -   changed dw timeout to 6ms and added debug leds.
 * C1.6.12  -   1. Anchor BIT(Imposter mode) fixed lora freq and buffer size.
 * 				2. dw fixed CIR index
 * 				3. changed ranged frame_len comper size.
 * C1.6.13	-	fixed tag_imposter lora tx and rx sequence.
 * C1.6.14		added TC_PGDLY program to channel 2 value
 * C1.6.15		increased dw timeout
 * C2.0.0		New Era of hardware, REVC2 entered the chat
 * C2.0.1		configured shared ioc for Rev_C1 and Rev_C2
 * C2.0.2		1. 	fixed UART3 missing receive parts.
 * 				2.	moved to HSE clock
 * C2.0.03		added hw version to status
 * C2.0.04		fixed diagnostics data being erased
 * C2.0.05		added attention to no_lora status
 * C2.0.07		1. Clean code
 * 				2. added adc in ioc
 * C2.0.08		modifed dwRange polling approach
 * C2.0.09		returned lost wakeup seq
 * C2.0.10		added attention to no_dw at wakeup
 * C2.0.12		changed bw for tag ids
 * C2.0.13		1. fixed test of no_lora status
 *  			2. fixed missing tag_no_lora report to pc
 *  			3. changed lora id bw span between ids
 *  			4. added lora tx timeout
 * C2.0.14		added lora checksum to lora icd
 * C2.0.15		changed lora bit frequency source anchor id to self_if
 * C2.0.16		changed dw gain value.
 * C2.0.17		adjusted version to tag
 * C2.0.18		adjusted version to tag
 * C3.0.00		new world of updating over the wire
 * C3.0.05		bootloader application
 * C3.0.08		adjusted version to tag
 * C3.0.09		[added] rtc memory access like tag config and split flash to 2 banks.
 * C3.0.10		FIX id programming
 * C3.0.11		[changed] self id flash address
 * C3.0.13		adjusted version to bootloader
 * C3.0.14		adjusted version to bootloader
 * C3.0.15		[changed] Masserase function name
 * C3.0.16		[add] CRC & length validation
 * C3.0.17		adjusted version to bootloader
 * C3.0.18		[add] status command in bootloader
 * C3.0.19		[add] battery level to status message
 * C3.0.20		[fixed] header file address
 * C3.0.21		[ADD] CRC file handler to applications
 * C3.0.22		[FIX] calc CRC on all 254 pages
 * C3.0.23		[changed] Anchor Lora timeout duration
 * C3.0.24		[ADD] NVIC reset boot after 1 minute
 * C3.0.25		adjusted version to bootloader
 * C3.0.26		[changed] amount of pages to  erase, from 254 to 255(max 256)
 * C3.0.27		[added] UART_OPCODE_SEARCH_TWR can be requested as ONLY LORA.
 * C3.0.28		adjusted version to tag
 * C3.0.29		[fixed] missing crc in ioc
 * C3.0.30		adjusted version to tag
 * C3.0.31		[fixed] lora rx holding valid_Data of previous id request causing race condition.
 * C3.0.32		[added] computability with revD
 * C3.0.33		[added] disabled fine grain tx sequencing.
 * C3.0.34		merged Operational_diable_finegrain into Operational
 * C3.0.35		[added] dw internal power config according to hardware version(D = maxpower, C = 10db)
 * C3.0.36		[fixed] id changed twice crashing app
 * C3.0.40      [ADDED] anchor Battery Indicator by anchor id 3 f
 * C3.0.41		[ADDED] error bits to BIT Control(imposter) reply.
 * C3.0.42		[ADDED] (not used),when asking to enable diagnostics to enable accumem clockS.
 * T3.0.51		[changed] config to work with 110K, no backoff, main.c line 365
 * 				[changed] imposter tx delay to 5000.
 * 				[added] commented function to make sure the clocks are enabled as required
 * T3.0.53		[changed] uncommented function to make sure the clocks are enabled as required
 * T3.0.54		[added] NTM_NEW to testing. enabled gpiolcocks on demand. reset ALL_RX_GOOD registers each range.
 * C3.0.55		[Changed] PR1024 define name  to LONG_PREAM
 * C3.0.56		adjusted version to tag
 * C3.0.62		[fixed] ioc file generate, flash files and flash boot_address define. fixed MemoryManagmentTool setup
 * C3.0.63		adjusted version to tag.
 * C3.0.64		[fixed] upload bug where SRAM_BKUP_REGISTER would keep update flag value after reset
 * C3.0.65		[added] reset counter report
 * C3.0.66		[added] BackOFF(dwScanMode) ctrl bit. 1 = enable, 0  = disable. default enable
 * anc3.1.0	[added] FOTA over UWB (fota.c): FOTA flasher mode via UART opcodes 0x40-0x47,
 * 				FOTA target in dwScanMode. Image received into RAM, CRC checked, written from RAM.
 * anc3.1.1	[added] FOTA discovery, auto data-rate fallback, battery check
 * anc3.1.2	[changed] battery gauge read on every anchor; FOTA reports 255 only when no gauge answers
 * anc3.1.3	[added] FOTA LEDs: target blinks from the handshake, loader link LED while loading
 * anc3.1.4	[fixed] a tag stays awake while it counts down as FOTA target
 * anc3.1.5	[added] backoff arm-once, release on opcode 19=0, runtime window {top,min}; status backoff in ms left; per-anchor random seed; imposter id mask; SET_ID refreshes UWB ids; UART error callback re-arms RX; antenna delays from flash (opcode 20); LDE_CFG1 survives dwConfig; status-bit clearing and frame-length checks
 * anc3.2.0	[added] doc section 9 link adaptation, every feature off by default (opcode 22 SET_LINK_FEATURES);
 * 				with the mask clear, frames and replies are byte-identical to anc3.1.5.
 * 				9A LDE_PPINDX/RXPACC_NOSAT/tick latched with the diagnostics; 9B CIR window (opcode 21);
 * 				9C tick, tag power echo, SignalRssiPkt in SEARCH_TWR; 9D negotiated tag TX power over
 * 				extended UWB frames, used only after the tag answers a hello (any version mix keeps
 * 				ranging); 9F repeats, shortest range; 9G LOS/NLOS LDE preset; 9K frozen with two stations.
 * 				9E/9H/9J profiles not implemented.
 * 				[fixed] bug list 2026-10-22: TIM4 is a true 1 ms tick (was 2.05 ms; timings kept, opcode-19
 * 				bytes read as old ticks); Error_Handler resets; LoRa DIO1 below TIM4 and Lora_Status masks
 * 				only DIO1; lost LoRa TX/RX interrupt recovery; rxError consumed; no_uwb not latched, bounded
 * 				DW1000 re-init; wrap-safe backoff deadlines.
 * 				[fixed] imposter response timestamp uses the applied TX antenna delay (was the default);
 * 				[added] DW1000 crystal trim in flash (opcode 23); 9F/9G only above -90 dBm.
 * 				[added] 9G preset sent to the tag in the extended poll; extended frames also with 9G alone.
 * 				[added] configuration channel (opcode 24): LoRa commands (inverted IQ) into the tag's listen
 * 				slots, extended UWB poll v2 as backup; beacon parse checks GetPayload; bench UWB level pin.
 * anc3.2.1	issue #5: same-station legacy polls keep the tag's LoRa power/LDE; owner-only fast fallback; station UWB level and LDE preset (opcode 22 bytes 11-12)
 * anc3.3.0	#7 range profiles: opcode 25 SET_PROFILE, owner prepare/commit with the tag, rotation until proof, reverts to profile 0; beacon SF per profile
 * anc3.3.1	Radio watchdog: DW1000 registers checked against the profile in use once a second; mismatch -> full re-init on profile 0 (#7)
 * anc4.0.0	BMI088 IMU read out: opcode 26 READ_IMU returns one raw acc/gyr/temp sample; IMU INT pins PA1/PA2 are inputs (were outputs driven low against the BMI088 push-pull INT)
 */

#define VERSION "anc4.0.0"


#define IS_TAG false

#define LORA_RX_BUFFER_SIZE 6

//writing to rtc
#define SRAM_BKUP_REGISTER  0X40036400
#define SRAM_BKUP_REGISTER_CLEAR 0

//#define DEBUG_RESET_COUNTER_IN_FLASH True

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

typedef struct {
    bool no_lora;
    uint8_t raw_buffer[LORA_RX_BUFFER_SIZE];
    uint8_t lora_msg_index;
    uint8_t buffer_length;
    int8_t rssi;  // average rssi
    int8_t snr;
    int8_t signal_rssi;  // SignalRssiPkt: LoRa signal after despreading, without in-band noise
    uint16_t tag_id;
    uint16_t tag_id_msg_valid; // used to checkout if the twr new id request is valid with tag_id_msg_valid anchor is already holding.
    uint8_t tag_soc;
    status_bits tag_errors;
    uint8_t new_data;
    int8_t TxCounter;
    uint8_t TagStatusUwb;
    uint32_t RxCounter;
    uint32_t freq;
    uint8_t anchor_id;
    uint16_t mine_pc_id;
    uint16_t self_id;
} LoraRxInfo_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t msg_index;
    uint16_t tag_id;
    uint8_t soc;
    status_bits tag_errors;
	uint8_t checksum;
} sLoRaMsg;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint16_t anchor_id;
    uint16_t tag_scrambled_id;
    uint8_t spare;
	uint8_t checksum;
} sLoRaWakeupMsg;
#pragma pack(pop)

typedef struct {
    uint8_t anchor_id;
    dwt_rxdiag_t diagnostics;
    dwt_deviceentcnts_t counter;
    bool no_uwb;
    uint16_t tag_dest;
    double tof;
    uint32_t distance;
    uint8_t dw_ready;
    bool wrong_ID;
    uint8_t tag_no_lora;
    uint32_t statusReg;
    uint32_t auxData;
    uint32_t RxToCounter;
    bool detec_pc_id;
    uint16_t mine_pc_id;
    uint32_t backoff_timer;
    uint32_t TxCounter;
    uint32_t RxCounter;
    bool uart_income;
    bool tag_imposter;

    // Latched with the diagnostics on each valid range (dwRange), before RX re-arms
    uint16_t lde_ppindx;        // LDE_PPINDX (0x2E:1000), peak path index
    uint16_t rxpacc_nosat;      // RXPACC_NOSAT (0x27:2C)
    uint32_t exchange_tick_ms;  // HAL_GetTick() when the response frame was received
    int8_t   tag_lora_tx_dbm;   // tag's echo: LoRa power it is using, LINK_LORA_UNKNOWN if none
    uint8_t  tag_uwb_tx_level;  // tag's echo: UWB level of this response, LINK_UWB_UNKNOWN if none
    uint16_t cir_fp_index;      // FP_INDEX (Q10.6) the CIR window was taken around
    uint16_t cir_start;         // first accumulator sample of the window
    uint8_t  cir_n;             // samples in the window, 0 = nothing latched yet
    uint8_t  cir_buf[1 + 4 * 32];   // dummy octet + n x (i16 re, i16 im)
} DwmInfo_t;

#define CIR_WINDOW_N    32      // samples latched around the first path (opcode 21)
#define CIR_WINDOW_PRE  8       // samples before the first path

/* Link adaptation (doc section 9, opcode 22 SET_LINK_FEATURES). Every feature ships off; with the
 * mask clear the anchor sends and answers exactly what anc3.1.5 did. Rule zero: the link is never
 * lost - every adaptive step fails toward maximum power on the legacy frames. */
#define LINK_F_REPEATS      0x01    // 9F: K exchanges per SEARCH_TWR, shortest range reported
#define LINK_F_LDE_AUTO     0x02    // 9G: LOS/NLOS LDE preset from RX-FP
#define LINK_F_POWER        0x04    // 9D: negotiated tag TX power
#define LINK_F_PROFILE      0x08    // 9H: not implemented, profile stays 0
#define LINK_F_CHAN_ALT     0x10    // 9E: not implemented
#define LINK_F_CIR          0x20    // 9B: latch the CIR window (opcode 21)
#define LINK_F_REPLY_EXT    0x40    // 9A/9C: extra reply fields and the STATUS link_state block

#define LINK_LORA_MAX_DBM   22
#define LINK_LORA_MIN_DBM   (-9)    // SX1262 high-power PA floor
#define LINK_LORA_STEP_DB   2
#define LINK_LORA_UNKNOWN   0x7F    // replies: unknown / not requested
#define LINK_LORA_MIN_SNR   (-7)    // below this SNR LoRa only steps up: SF12 decodes to -20 dB, so this keeps
                                    // the 10 dB margin after one more 2 dB step (SNR reads only 3-4 dB near compression)
#define LINK_UWB_LEVELS     8       // level 0 = maximum, each level 3 dB lower
#define LINK_UWB_UNKNOWN    0xFF
#define LINK_UWB_SENS_DBM   (-106)  // DW1000 110 kbps sensitivity: never step below it + 10 dB margin
#define LINK_UWB_LOST_MS    1000    // no UWB response this long: everything back to maximum
#define LINK_LORA_LOST_MS   1500    // no LoRa packet this long (packets ~0.55 s apart): LoRa maximum
#define LINK_EXT_HOLD_MS    5000    // after falling back to legacy frames, no hello for this long
#define LINK_TRIES          5       // polls a request or a confirm is repeated before giving up
#define LINK_RXFP_SOFT_DB   6.0f    // RX-FP soft threshold (APS006): above it, likely NLOS
#define LINK_LDE_HOLD       10      // exchanges beyond the threshold before the LDE preset flips
#define LINK_LDE_NLOS       0x67    // NTM 7 / PMULT 3 (APS006 Pt 2)
#define LINK_K_MAX          5
#define LINK_STATION_HOLD_MS 2000   // station UWB level held at maximum after two misses
#define LINK_MISS_OWNER      2      // unanswered extended polls before legacy frames: LoRa owner
#define LINK_MISS_OTHER      10     //   other anchors (a miss costs their exchange, not the station's link)
#define LINK_STRONG_DBM     (-90.0f) // 9F/9G act only above this rx level: near sensitivity (km range) RX-FP is
                                    // high from noise, the NLOS threshold and "keep the shortest" would pick noise

typedef struct {            // opcode 22 payload, in order
    uint8_t features;
    uint8_t k_max;          // 9F: 0 = anchor decides, 1-5 = fixed
    int8_t  lora_target_dbm;
    uint8_t lora_band_db;
    int8_t  uwb_target_dbm;
    uint8_t uwb_band_db;
    uint8_t profile_max;    // 9H, stored only
    uint8_t profile_override;
    uint8_t step_interval_ds;   // minimum time between steps away from robustness, 0.1 s
    uint8_t lora_owner;     // 1 = this anchor runs the LoRa loop (one per station)
    uint8_t uwb_level_pin;  // bench: fixed UWB level for the tag's responses (needs bit2), 0xFF = automatic
    // Station values (issue #5): the host decides for the whole station and sends the same value
    // to all its anchors; 0xFF = this anchor decides alone. K for the station is byte 1 (k_max).
    uint8_t uwb_level_station;  // UWB level 0..7 (needs bit2); two misses -> maximum for LINK_STATION_HOLD_MS
    uint8_t lde_station;        // 0 = LOS, 1 = NLOS preset, for anchor and tag (needs bit1)
} LinkCfg_t;

typedef struct {
    // extended frames
    uint8_t  ext;           // tag answered the hello: extended polls in use
    uint8_t  tag_ver;       // the tag's link version from its hello answer (1 = tag3.2.0)
    uint8_t  ext_miss;      // consecutive unanswered extended polls
    uint8_t  ext_hold;      // fell back from extended frames: no hello for LINK_EXT_HOLD_MS
    uint32_t ext_hold_tick;
    uint16_t ext_tag;       // tag the state belongs to
    uint8_t  reverting;     // feature switched off: asking for maximum until the tag shows it
    uint8_t  revert_tries;
    // UWB loop: one level per exchange, this anchor only
    uint8_t  uwb_level;     // level the last poll asked for
    uint8_t  uwb_prev;      // level before the last step
    uint8_t  uwb_good;      // good responses since the last step
    uint8_t  uwb_miss;      // consecutive unanswered polls
    uint8_t  uwb_mismatch;
    uint8_t  uwb_floor;     // quietest level the tag can do (fewer levels on boards with a lower maximum)
    uint32_t uwb_step_tick;
    // LoRa loop, owner only: request / ack / confirm / proof (9J)
    uint8_t  seq;
    uint8_t  lora_phase;    // LINK_LP_*
    int8_t   lora_cur;      // power the tag reports in use
    int8_t   lora_req;      // power requested under seq
    uint8_t  lora_tries;
    uint8_t  lora_good;     // LoRa packets since the last completed step
    uint32_t lora_step_tick;
    // last seen from the tag
    uint8_t  ack_seq;
    int8_t   echo_lora_dbm;
    uint8_t  echo_uwb_level;
    uint8_t  echo_profile;
    uint8_t  echo_pending;
    uint8_t  echo_lde_nlos; // the tag runs the NLOS LDE preset
    uint32_t last_uwb_tick;
    uint32_t last_lora_tick;
    // 9F / 9G
    uint32_t coexist_tick;  // last time the tag reported two stations or this anchor's backoff was armed (9K)
    // range profiles (#7)
    uint8_t  prof_cur;      // profile in force with this tag
    uint8_t  prof_target;   // the host's last request
    uint8_t  prof_prev;     // rotation partner (the profile before the switch)
    uint8_t  prof_phase;    // PROF_P_*
    uint8_t  prof_reason;   // last revert / refusal (PROF_R_*)
    uint8_t  prof_seq;      // the host's sequence number
    uint8_t  prof_rot;      // rotation: this poll on prof_target (1) or prof_prev (0)
    uint8_t  prof_rx;       // configuration the DW1000 runs now
    uint32_t prof_t0;       // phase start / deadline base
    uint8_t  station_hold;  // station UWB level dropped to maximum after misses
    uint32_t station_hold_tick;
    uint8_t  lde_nlos;      // NLOS preset active
    uint8_t  lde_count;
    float    last_rxfp_db;
    float    last_rx_dbm;
    uint8_t  k_used;
} LinkState_t;

#define LINK_LP_IDLE    0
#define LINK_LP_REQ     1   // quieter request sent, waiting for the ack
#define LINK_LP_CONF    2   // confirm sent, waiting for the tag to show the new power
#define LINK_LP_UP      3   // louder request sent (applied at once), waiting for the echo

/* One configuration item for one tag at a time (opcode 24 TAG_CONFIG): LoRa first, the extended
 * UWB poll v2 as backup once LoRa has not answered for LITEM_UWB_AFTER_MS. */
#define LITEM_IDLE          0
#define LITEM_PENDING       1
#define LITEM_DONE          2
#define LITEM_FAILED        3
#define LITEM_UWB_AFTER_MS  3000
#define LITEM_GIVE_UP_MS    60000
#define LITEM_VIA_LORA      1
#define LITEM_VIA_UWB       2
typedef struct {
    uint8_t  state;         // LITEM_IDLE / PENDING / DONE / FAILED
    uint16_t tag;
    uint8_t  item;
    uint16_t value;
    uint8_t  seq;
    uint16_t readback;
    uint8_t  flags;         // LACK_F_* from the answer
    uint8_t  via;           // LITEM_VIA_* that answered
    uint8_t  lora_tries;
    uint8_t  tag_listen_n;  // the tag's listen period from its last ack (0 = unknown)
    uint32_t listen_tick;
    uint32_t start_tick;
} LinkItem_t;
extern LinkItem_t link_item;
bool Link_ItemQueue(uint16_t tag, uint8_t item, uint16_t value);
bool Link_ItemLoraDue(uint8_t beacon_index);	// send the command after this beacon?

#define PROF_P_IDLE     0
#define PROF_P_PREP     1   // owner: the tag is told to get ready
#define PROF_P_COMMIT   2   // owner: the tag is told to switch
#define PROF_P_ARMED    3   // other anchors: target known, polls stay on prof_cur until they miss
#define PROF_P_ROTATE   4   // polls alternate prof_target / prof_prev until a reply decides
#define PROF_ROTATE_MS  1000    // rotation without any reply: profile 0
#define PROF_ARMED_MS   30000   // armed without the tag switching: give up
bool Link_ProfileRequest(uint8_t seq, uint8_t profile);
void Link_ProfileGround(uint8_t reason);
uint8_t Link_ProfileSF(void);	// LoRa beacon SF for the profile in force
void Lora_SetBeaconSF(uint8_t sf);	// Lora.cpp: SF the beacons arrive at

extern LinkCfg_t link_cfg;
extern LinkState_t link_st;
uint32_t Backoff_Left(void);	// ms left, wrap-safe; clears an expired deadline
#define BACKOFF_MAX_MS		2000	// a deadline further out than this is not a real backoff
void Link_Init(void);
bool Link_Coexisting(void);
void Link_SetConfig(const uint8_t *cfg, uint8_t len);
void Link_OnLoraPacket(int8_t signal_rssi_dbm, int8_t snr_db);

typedef struct {
    uint8_t rf_info[RX_FINFO_LEN];
    uint8_t rf_fqual[RX_FQUAL_LEN];
    uint8_t rf_ttcki_id[RX_TTCKI_LEN];
    uint8_t rf_ttcko_id[RX_TTCKO_LEN];
} DwmExtra_data;

extern uint32_t _1ms_tick;
enum tag_wakeup {
    /*0*/ WAKEUP_START,
    /*1*/ WAKEUP_WAIT_FOR_TX_IDLE
};
typedef struct {
    GPIO_TypeDef *GPIO_Port;
    uint16_t GPIO_Pin;
} VER_PIN;

#define PIN(LED) Leds_GPIO[LED].GPIO_Pin
#define READ_HW_PIN(VER_PIN, BIT) (!HAL_GPIO_ReadPin(VER_PIN.GPIO_Port, VER_PIN.GPIO_Pin) << BIT)
enum {
    VER_R0,
    VER_R1,
    VER_R2,
};

void main_init();
void write_clear_bkup_sram(void);
void write_bkup_sram(void);
void SetAnchor_cable_id();
void SetAnchor_self_id();
/* Configuration channel (doc 9J/9L): requests reach the tag over LoRa (primary) and over the
 * extended UWB poll (backup), through one handler on the tag. LoRa commands and acks are sent with
 * inverted IQ: a normal-IQ receiver (every anchor listening for beacons, and all older firmware)
 * never demodulates them, so they cannot be mistaken for a beacon. The tag listens for a short
 * window after a beacon whose counter is a multiple of its listen period (every beacon while UWB
 * is silent, every 4th while extended UWB polls arrive, every 20th otherwise); anchors send only
 * into windows that fall on a multiple of 4, which every period above lands on. Items change
 * only what cannot cut the link; a lost command delays a change, it never changes how the tag
 * communicates. */
#define LCMD_MAGIC          0xC3
#define LACK_MAGIC          0xA5
#define LCMD_SLOT_MOD       4       // anchors send after beacons with msg_index % 4 == 0
#define LCMD_LISTEN_GROUND  20      // tag listen period at rest (multiple of LCMD_SLOT_MOD)
#define LCMD_LISTEN_ACTIVE  4       // while extended UWB polls arrive
#define LCMD_LISTEN_MS      150     // RX window until a preamble shows (timer stops on preamble)
#define LCMD_EXCHANGE_MS    1500    // guard on a whole command or ack reception

#define LITEM_NOP           0       // ping: ack carries the state
                                    // 1: reserved (the LDE preset follows 9G in every extended poll)
#define LITEM_LISTEN_N      2       // tag listen period 1..255 (back to 20 after 30 s without commands)
#define LITEM_ANT_TX        3       // TX antenna delay, flash; 0xFFFF = default
#define LITEM_ANT_RX        4       // RX antenna delay, flash; 0xFFFF = default
#define LITEM_XTAL          5       // DW1000 crystal trim 0..31, flash; 0xFF = clear
#define LITEM_PROFILE_PREP  6       // #7: profile the tag should get ready for (nothing switches)
#define LITEM_PROFILE_COMMIT 7      // #7: switch to it after this answer has gone out (0 = always allowed)
#define LITEM_HW            8       // #17: read-only, read-back = the tag's HW_Version (0 = Rev C, 2 = Rev D); older tags reject it

/* Range profiles (#7). UWB channel 2, PRF 64, code 9 never change; LoRa commands, acks and wake-ups
 * stay SF12 at maximum power - only the tag's beacon SF follows the profile. Profile 0 is the ground
 * state: power-up, reset, wake-up, two stations, FOTA, and every unilateral revert land there. */
#define LINK_PROFILES       3
#define PROF_R_NONE         0       // revert / refusal reasons, reported to the host
#define PROF_R_HOST         1       //   the host asked
#define PROF_R_SILENCE      2       //   no frame on the new profile
#define PROF_R_MISSES       3       //   missed polls
#define PROF_R_MULTI        4       //   two stations share the tag
#define PROF_R_NO_PROOF     5       //   the switch did not complete
#define PROF_R_WATCHDOG     6       //   radio configuration did not match the profile
#define PROF_R_FOTA         7
#define PROF_R_UNSUPPORTED  8       //   tag too old, feature bit off, or no extended frames
#define LITEM_NONE          0xFF    // extended poll v2: no item

#define LACK_F_REJECTED     0x01    // value out of range: nothing changed
#define LACK_F_LDE_NLOS     0x02
#define LACK_F_MULTI        0x04    // two stations: adaptation items held at the ground state
#define LACK_F_PENDING      0x08    // a quieter LoRa power request waits for its confirm
#define LACK_F_APPLYING     0x10    // UWB path: taken, applied after this response; the next answer has the read-back

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic;         // LCMD_MAGIC
    uint16_t tag_id;
    uint16_t src;           // sending anchor's id
    uint8_t  seq;           // item sequence number (a repeat is answered, not re-applied)
    uint8_t  item;
    uint16_t value;
    uint8_t  spare;
    uint8_t  checksum;      // sum of the bytes before it
} sLoRaCmd;                 // 11 bytes: the same 13 LoRa symbols as a 6-byte beacon at SF12

typedef struct {
    uint8_t  magic;         // LACK_MAGIC
    uint16_t tag_id;
    uint8_t  seq;           // the command answered
    uint8_t  item;
    uint16_t value;         // read-back after applying
    int8_t   lora_dbm;      // LoRa power in use
    uint8_t  flags;         // LACK_F_*
    uint8_t  listen_n;      // listen period in force
    uint8_t  checksum;
} sLoRaAck;                 // 11 bytes
#pragma pack(pop)
void Link_ItemFillCmd(sLoRaCmd *c);
void Link_ItemOnLoraAck(const sLoRaAck *a);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VER1_R1_Pin GPIO_PIN_2
#define VER1_R1_GPIO_Port GPIOE
#define VER1_R2_Pin GPIO_PIN_3
#define VER1_R2_GPIO_Port GPIOE
#define SX_DIO1_Pin GPIO_PIN_4
#define SX_DIO1_GPIO_Port GPIOE
#define SX_DIO1_EXTI_IRQn EXTI4_IRQn
#define LED_R_Pin GPIO_PIN_5
#define LED_R_GPIO_Port GPIOE
#define LED_G_Pin GPIO_PIN_6
#define LED_G_GPIO_Port GPIOE
#define SX_BUSY_Pin GPIO_PIN_13
#define SX_BUSY_GPIO_Port GPIOC
#define LED_B_Pin GPIO_PIN_8
#define LED_B_GPIO_Port GPIOF
#define FTDI_UART6_NPWREN_Pin GPIO_PIN_1
#define FTDI_UART6_NPWREN_GPIO_Port GPIOC
#define FTDI_UART6_RX_Pin GPIO_PIN_2
#define FTDI_UART6_RX_GPIO_Port GPIOC
#define FTDI_UART6_TX_Pin GPIO_PIN_3
#define FTDI_UART6_TX_GPIO_Port GPIOC
#define IMU_INT_ACC_Pin GPIO_PIN_1
#define IMU_INT_ACC_GPIO_Port GPIOA
#define IMU_INT_GYR_Pin GPIO_PIN_2
#define IMU_INT_GYR_GPIO_Port GPIOA
#define IMU_SPI1_CS_Pin GPIO_PIN_4
#define IMU_SPI1_CS_GPIO_Port GPIOA
#define IMU_SPI1_SCK_Pin GPIO_PIN_5
#define IMU_SPI1_SCK_GPIO_Port GPIOA
#define IMU_SPI1_MISO_Pin GPIO_PIN_6
#define IMU_SPI1_MISO_GPIO_Port GPIOA
#define IMU_SPI1_MOSI_Pin GPIO_PIN_7
#define IMU_SPI1_MOSI_GPIO_Port GPIOA
#define IMU_NCS_GYR_Pin GPIO_PIN_0
#define IMU_NCS_GYR_GPIO_Port GPIOB
#define SELECTOR_STAT_Pin GPIO_PIN_10
#define SELECTOR_STAT_GPIO_Port GPIOB
#define RRC_I2C2_SCL_ID1_Pin GPIO_PIN_13
#define RRC_I2C2_SCL_ID1_GPIO_Port GPIOB
#define RRC_I2C2_SDA_ID0_Pin GPIO_PIN_14
#define RRC_I2C2_SDA_ID0_GPIO_Port GPIOB
#define FTDI_UART3_TX_Pin GPIO_PIN_8
#define FTDI_UART3_TX_GPIO_Port GPIOD
#define FTDI_UART3_RX_Pin GPIO_PIN_9
#define FTDI_UART3_RX_GPIO_Port GPIOD
#define FTDI_UART3_NPWREN_Pin GPIO_PIN_10
#define FTDI_UART3_NPWREN_GPIO_Port GPIOD
#define VER2_R0_Pin GPIO_PIN_6
#define VER2_R0_GPIO_Port GPIOC
#define VER2_R1_Pin GPIO_PIN_7
#define VER2_R1_GPIO_Port GPIOC
#define VER2_R2_Pin GPIO_PIN_8
#define VER2_R2_GPIO_Port GPIOC
#define SX_LED_RX_Pin GPIO_PIN_9
#define SX_LED_RX_GPIO_Port GPIOC
#define SX_LED_TX_Pin GPIO_PIN_8
#define SX_LED_TX_GPIO_Port GPIOA
#define SX_OPT_Pin GPIO_PIN_9
#define SX_OPT_GPIO_Port GPIOA
#define SX_Mode_SX126x_Pin GPIO_PIN_10
#define SX_Mode_SX126x_GPIO_Port GPIOA
#define SX_Mode_FRx_Pin GPIO_PIN_11
#define SX_Mode_FRx_GPIO_Port GPIOA
#define SX_RESET_Pin GPIO_PIN_12
#define SX_RESET_GPIO_Port GPIOA
#define DW_PA_BYPASS_Pin GPIO_PIN_15
#define DW_PA_BYPASS_GPIO_Port GPIOA
#define DW_SPI3_SCK_Pin GPIO_PIN_10
#define DW_SPI3_SCK_GPIO_Port GPIOC
#define DW_SPI3_MISO_Pin GPIO_PIN_11
#define DW_SPI3_MISO_GPIO_Port GPIOC
#define DW_SPI3_MOSI_Pin GPIO_PIN_12
#define DW_SPI3_MOSI_GPIO_Port GPIOC
#define DW_RESET_Pin GPIO_PIN_0
#define DW_RESET_GPIO_Port GPIOD
#define SX_SPI2_SCK_Pin GPIO_PIN_1
#define SX_SPI2_SCK_GPIO_Port GPIOD
#define DW_CS_Pin GPIO_PIN_2
#define DW_CS_GPIO_Port GPIOD
#define SX_SPI2_MISO_Pin GPIO_PIN_3
#define SX_SPI2_MISO_GPIO_Port GPIOD
#define SX_SPI2_MOSI_Pin GPIO_PIN_4
#define SX_SPI2_MOSI_GPIO_Port GPIOD
#define DW_WAKEUP_Pin GPIO_PIN_10
#define DW_WAKEUP_GPIO_Port GPIOG
#define SX_ANT_SW_Pin GPIO_PIN_6
#define SX_ANT_SW_GPIO_Port GPIOB
#define SX_SPI2_CS_Pin GPIO_PIN_7
#define SX_SPI2_CS_GPIO_Port GPIOB
#define DW_IRQ_Pin GPIO_PIN_8
#define DW_IRQ_GPIO_Port GPIOB
#define MCU_STD_N_BYPS_Pin GPIO_PIN_9
#define MCU_STD_N_BYPS_GPIO_Port GPIOB
#define VER1_R0_Pin GPIO_PIN_1
#define VER1_R0_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */
void print(const char *s);
void printP(const char *s, const char *p);
void read_hw_version(void);

typedef struct{
	uint32_t crc;
	uint32_t len;
}HEADER_FILE;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
