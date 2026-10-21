#include "dw1000.h"
#include "../Lora/Inc/Lora.hpp"
#include "main.h"
#include "flash_handler.h"
#include "Power.h"
#include "fota.h"

#define print(...) ;

static uint64 poll_rx_ts;
static uint64 resp_tx_ts;
static uint32 status_reg = 0;

extern uint32_t _1ms_tick; //timer TIM4 1ms
extern uint8_t HW_Version;
extern LoraRxInfo_t lora_rxInfo;
DwmInfo_t dwmInfo = { .no_uwb = false, .anchor_dest = 0, .dw_ready = false, .RxToCounter = 0, .uart_income = false};

static uint64 get_rx_timestamp_u64(void);
static void resp_msg_set_ts(uint8 *ts_field, const uint64 ts);


//static uint8 tx_resp_msg[] = {TAG_FLAG, 0x88, 0, 0xCA, 0xDE, (uint8_t)(SELF_ID&0xFF),(uint8_t)(SELF_ID>>8), 'P', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
twr_tag_msg_t tx_resp_msg;

uint8_t dummy_buff[1024] = {0};

#ifndef LONG_PREAM

dwt_config_t config = {
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
//dwt_config_t config = { 2,                //Channel number.
//		DWT_PRF_64M,      //Pulse repetition frequency.
//		DWT_PLEN_1024,     //Preamble length. Used in TX only.
//		DWT_PAC32,         //Preamble acquisition chunk size. Used in RX only.
//		9,                //TX preamble code. Used in TX only.
//		9,                //RX preamble code. Used in RX only.
//		0,                //0 to use standard SFD, 1 to use non-standard SFD.
//		DWT_BR_850K,       //Data rate.
//		DWT_PHRMODE_STD,  //PHY header mode.
//		1100 //(1024 + 1 + 16) // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only.
//		};

//REV D configuration
dwt_config_t config = {
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
/*
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
void dwmStatus() {
	uint32 ID = dwt_readdevid();
	// Read and validate device ID, return -1 if not recognised
	if (DWT_DEVICE_ID != ID) // MP IC ONLY (i.e. DW1000) FOR THIS CODE
	{
		dwmInfo.no_uwb = true;
	}
}

int dwmInit() {
	/* Reset and initialize DW1000.
	 * For initialization, DW1000 clocks must be temporarily set to crystal speed. After initialization SPI rate can be increased for optimum
	 * performance. */
	print("tag number %u\n\r", TAG_ID);
	reset_DW1000(); /* Target specific drive of RSTn line into DW1000 low for a period. */
	port_set_dw1000_slowrate();
	if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR) {
		print("Tag  DW1000 Init Failed\r\n");
		dwmInfo.no_uwb = true;
		return FALSE;
	}
	dwmInfo.no_uwb = false;
	print("Tag  DW1000 Init DW1000 Succeed\r\n");
	return TRUE;

}

void LPL_Setup(uint32_t long_sleep_s, uint8_t snooze_units, uint16_t pdt_symbols)
{
	port_set_dw1000_slowrate();

	uint16_t lpcal = dwt_calibratesleepcnt();
	(void)lpcal;
	uint16_t sleep16 = (uint16_t)((long_sleep_s*19200000u)>>12);
	dwt_configuresleepcnt(sleep16);
	dwt_setsnoozetime(snooze_units);
	dwt_setpreambledetecttimeout(pdt_symbols);
	dwt_configuresleep(	DWT_PRESRV_SLEEP | DWT_CONFIG | DWT_RX_EN,
						DWT_WAKE_SLPCNT | DWT_SLP_EN);
}

void LPL_EN(uint8_t en)
{
	dwt_setlowpowerlistening(en);
	if (en)
	{
		dwt_entersleep();
	}
}

void dwConfig(dwt_config_t *config) {
	dwt_txconfig_t tx_config;

	tx_config.PGdly = 0xC2; //Channel 2 TC_PGDELAY
	if (HW_Version == 2)
	{
		tx_config.power =  MAX_POWER;
	}
	else{
		tx_config.power = 0x40404040; //10db
	}
	dwt_setlnapamode(DWT_PA_ENABLE | DWT_LNA_ENABLE ); //While using PA LNA Enable

	dwt_enablegpioclocks(); //REQUIRED =: Sets PMSC_CTRL0 GPCE|GPRN so EXTRXE (GPIO6) actualy drives the LNA

	port_set_dw1000_fastrate();

	/* Configure DW1000. See NOTE 5 below. */
	dwt_configure(config);

	/* Apply default antenna delay value. See NOTE 2 below. */
	dwt_setrxantennadelay(RX_ANT_DLY);
	dwt_settxantennadelay(TX_ANT_DLY);


	dwt_setsmarttxpower(DISABLE);

		dwt_configuretxrf(&tx_config);//max power refer user manual pg.106
	//printf("0x1E = %x\n\r",dwt_read32bitreg(TX_POWER_ID));
	dwt_setleds(LEDS_ON);

	#ifdef NEW_NTM
		//set lower SNR for NLOS operation
		dwt_write8bitoffsetreg(LDE_IF_ID, LDE_CFG1_OFFSET, 0x6D); //refer user manual pg.175
		dwt_write16bitoffsetreg(LDE_IF_ID, LDE_CFG2_OFFSET, 0x0003); //refer user manual pg.177*/
		//printf("0x2E = %x\n\r",dwt_read32bitreg(LDE_IF_ID));
	#endif

	//	dwt_setpreambledetecttimeout(TIMEOUT_PACS);
	dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);


	// Disable fine grain TX sequencing
	dwt_setfinegraintxseq(0);

	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS); // to clear flag
}

#ifdef TIMING_TESTS_USING_LED_GPIO
	#define DBG_TOGG				HAL_GPIO_TogglePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin);
	#define DBG_ON					HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_SET);
	#define DBG_OFF					HAL_GPIO_WritePin(SX_LED_RX_GPIO_Port, SX_LED_RX_Pin, GPIO_PIN_RESET);
#else
	#define DBG_TOGG
	#define DBG_ON
	#define DBG_OFF
#endif // TIMING_TESTS_USING_LED_GPIO

int dwRange(void) {
	/* Activate reception immediately. */

	tx_resp_msg.tag_no_lora = lora_rxInfo.no_lora;
	uint8_t RetVal = false;
	dwt_rxenable(DWT_START_RX_IMMEDIATE);

	/* Poll for reception of a frame or error/timeout. See NOTE 6 below. */
	uint32_t SavedTimer = HAL_GetTick();

//	while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {
//		if  ((HAL_GetTick() - SavedTimer) > THRESHOLD_MS){
//			dwmInfo.RxToCounter++;
//			break;
//		}
//		if(dwmInfo.uart_income == true){
//			dwt_forcetrxoff();
//			dwmInfo.statusReg = SYS_STATUS_RXFCE; // stop begin rx if pc adress you
//			break;
//		}
//	};

	while (1){
		status_reg = dwt_read32bitreg(SYS_STATUS_ID);

		if (status_reg & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)) {
			break;
		}

		if  ((HAL_GetTick() - SavedTimer) > THRESHOLD_MS){
			dwmInfo.RxToCounter++;
			dwt_forcetrxoff();
			dwmInfo.statusReg = SYS_STATUS_RXFCE; // stop begin rx if pc adress you
			break;
		}
		if(dwmInfo.uart_income == true){
			dwt_forcetrxoff();
			dwmInfo.statusReg = SYS_STATUS_RXFCE; // stop begin rx if pc adress you
			break;
		}
	};
	dwmInfo.statusReg = status_reg;

	if (status_reg & SYS_STATUS_RXFCG) {

		uint32 frame_len;
		twr_anchor_msg_t rx_buffer;

		DBG_ON
		/* Clear good RX frame event in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		/* A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
		uint8_t first = 0;
		dwt_readrxdata(&first, 1, 0);
		if (first == FOTA_FLAG) {
			fota_uwb_rx(frame_len);
			return 0;
		}
		if (frame_len == sizeof(rx_buffer)) {
			dwt_readrxdata((uint8_t *)&rx_buffer, frame_len, 0);
		}
		else {
			dummy_buff[0]++;
			dwt_readrxdata(&dummy_buff[1], frame_len,0);
			return 0;
		}
		/* Check that the frame is a poll sent by "SS TWR initiator" example.
		 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
		if (rx_buffer.des == SELF_ID) {
			tx_resp_msg.des = rx_buffer.src;
			lora_rxInfo.wakeup_detected = true; //enable Lora while recieve uwb
			uint32 resp_tx_time;
			int ret;
			RetVal = true;
			/* Retrieve poll reception timestamp. */
			poll_rx_ts = get_rx_timestamp_u64();

			/* Compute final message transmission time. See NOTE 7 below. */
			resp_tx_time = (poll_rx_ts
					+ (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
			dwt_setdelayedtrxtime(resp_tx_time);

			/* Response TX timestamp is the transmission time we programmed plus the antenna delay. */
			resp_tx_ts = (((uint64) (resp_tx_time & 0xFFFFFFFEUL)) << 8)
					+ TX_ANT_DLY;

			tx_resp_msg.msd_index = rx_buffer.msd_index;//return the same message index

			/* Write all timestamps in the final message. See NOTE 8 below. */
			resp_msg_set_ts((uint8_t*)&tx_resp_msg.poll_rx_ts_id, poll_rx_ts);
			resp_msg_set_ts((uint8_t*)&tx_resp_msg.resp_tx_ts_id, resp_tx_ts);

			/* Write and send the response message. See NOTE 9 below. */
			//tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
			dwt_writetxdata(sizeof(tx_resp_msg), (uint8_t*)&tx_resp_msg, 0); /* Zero offset in TX buffer. */
			dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1); /* Zero offset in TX buffer, ranging. */

			dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, dwt_read8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET) | 0x20); // force Tx clock on first (PMSC_CTRL0 bits 5,4 = 1,0)
			ret = dwt_starttx(DWT_START_TX_DELAYED);
			/* If dwt_starttx() returns an error, abandon this ranging exchange and proceed to the next one. See NOTE 10 below. */
			if (ret == DWT_SUCCESS) {
				/* Poll DW1000 until TX frame sent event set. See NOTE 6 below. */
				while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS)) {
				};
				/* Clear TXFRS event. */
				dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

				/* Increment frame sequence number after transmission of the poll message (modulo 256). */
				PWR_SetTimeOut(PWR_Duration_MyMessage);
			}
		}
		DBG_OFF
	} else {
		/* Clear RX error events in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID,
				SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR| SYS_STATUS_ALL_RX_GOOD);

		/* Reset RX to properly reinitialise LDE operation. */
		dwt_rxreset();
	}

	return (RetVal);
}

void dw_PreSleep(void)
{
	LPL_Setup(2,2,512);
	// Clear all receive status bits
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD);
	// set IRQ enable
	dwt_setinterrupt(DWT_INT_RFCG, true);

	//	Start low power listening
	LPL_EN(true);
}

void dw_PostSleep(void)
{
	//	Stop low power listening
	LPL_EN(false);

	dwt_setpreambledetecttimeout(TIMEOUT_PACS);

}
/*! ------------------------------------------------------------------------------------------------------------------
 * @fn get_rx_timestamp_u64()
 *
 * @brief Get the RX time-stamp in a 64-bit variable.
 *        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
 *
 * @param  none
 *
 * @return  64-bit value of the read time-stamp.
 */
static uint64 get_rx_timestamp_u64(void) {
	uint8 ts_tab[5];
	uint64 ts = 0;
	int i;
	dwt_readrxtimestamp(ts_tab);
	for (i = 4; i >= 0; i--) {
		ts <<= 8;
		ts |= ts_tab[i];
	}
	return ts;
}

/*
 uint64 get_system_timestamp(void)
 {
 uint8 ts_tab[5];
 uint64 ts = 0;
 int i;
 dwt_readsystime(ts_tab);
 for (i = 4; i >= 0; i--)
 {
 ts <<= 8;
 ts |= ts_tab[i];
 }
 return ts;
 }
 */

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn final_msg_set_ts()
 *
 * @brief Fill a given timestamp field in the response message with the given value. In the timestamp fields of the
 *        response message, the least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to fill
 *         ts  timestamp value
 *
 * @return none
 */
static void resp_msg_set_ts(uint8 *ts_field, const uint64 ts) {
	int i;
	for (i = 0; i < RESP_MSG_TS_LEN; i++) {
		ts_field[i] = (ts >> (i * 8)) & 0xFF;
	}
}
//#endif
/***************************************************
 * NOTES:
 *
 * 1. The single-sided two-way ranging scheme implemented here has to be considered carefully as the accuracy of the distance measured is highly
 *    sensitive to the clock offset error between the devices and the length of the response delay between frames. To achieve the best possible
 *    accuracy, this response delay must be kept as low as possible. In order to do so, 6.8 Mbps data rate is used in this example and the response
 *    delay between frames is defined as low as possible. The user is referred to User Manual for more details about the single-sided two-way ranging
 *    process.
 * 2. The sum of the values is the TX to RX antenna delay, experimentally determined by a calibration process. Here we use a hard coded typical value
 *    but, in a real application, each device should have its own antenna delay properly calibrated to get the best possible precision when performing
 *    range measurements.
 * 3. The frames used here are Decawave specific ranging frames, complying with the IEEE 802.15.4 standard data frame encoding. The frames are the
 *    following:
 *     - a poll message sent by the initiator to trigger the ranging exchange.
 *     - a response message sent by the responder to complete the exchange and provide all information needed by the initiator to compute the
 *       time-of-flight (distance) estimate.
 *    The first 10 bytes of those frame are common and are composed of the following fields:
 *     - byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
 *     - byte 2: sequence number, incremented for each new frame.
 *     - byte 3/4: PAN ID (0xDECA).
 *     - byte 5/6: destination address, see NOTE 4 below.
 *     - byte 7/8: source address, see NOTE 4 below.
 *     - byte 9: function code (specific values to indicate which message it is in the ranging process).
 *    The remaining bytes are specific to each message as follows:
 *    Poll message:
 *     - no more data
 *    Response message:
 *     - byte 10 -> 13: poll message reception timestamp.
 *     - byte 14 -> 17: response message transmission timestamp.
 *    All messages end with a 2-byte checksum automatically set by DW1000.
 * 4. Source and destination addresses are hard coded constants in this example to keep it simple but for a real product every device should have a
 *    unique ID. Here, 16-bit addressing is used to keep the messages as short as possible but, in an actual application, this should be done only
 *    after an exchange of specific messages used to define those short addresses for each device participating to the ranging exchange.
 * 5. In a real application, for optimum performance within regulatory limits, it may be necessary to set TX pulse bandwidth and TX power, (using
 *    the dwt_configuretxrf API call) to per device calibrated values saved in the target system or the DW1000 OTP memory.
 * 6. We use polled mode of operation here to keep the example as simple as possible but all status events can be used to generate interrupts. Please
 *    refer to DW1000 User Manual for more details on "interrupts". It is also to be noted that STATUS register is 5 bytes long but, as the event we
 *    use are all in the first bytes of the register, we can use the simple dwt_read32bitreg() API call to access it instead of reading the whole 5
 *    bytes.
 * 7. As we want to send final TX timestamp in the final message, we have to compute it in advance instead of relying on the reading of DW1000
 *    register. Timestamps and delayed transmission time are both expressed in device time units so we just have to add the desired response delay to
 *    response RX timestamp to get final transmission time. The delayed transmission time resolution is 512 device time units which means that the
 *    lower 9 bits of the obtained value must be zeroed. This also allows to encode the 40-bit value in a 32-bit words by shifting the all-zero lower
 *    8 bits.
 * 8. In this operation, the high order byte of each 40-bit timestamps is discarded. This is acceptable as those time-stamps are not separated by
 *    more than 2**32 device time units (which is around 67 ms) which means that the calculation of the round-trip delays (needed in the
 *    time-of-flight computation) can be handled by a 32-bit subtraction.
 * 9. dwt_writetxdata() takes the full size of the message as a parameter but only copies (size - 2) bytes as the check-sum at the end of the frame is
 *    automatically appended by the DW1000. This means that our variable could be two bytes shorter without losing any data (but the sizeof would not
 *    work anymore then as we would still have to indicate the full length of the frame to dwt_writetxdata()).
 * 10. When running this example on the EVB1000 platform with the POLL_RX_TO_RESP_TX_DLY response delay provided, the dwt_starttx() is always
 *     successful. However, in cases where the delay is too short (or something else interrupts the code flow), then the dwt_starttx() might be issued
 *     too late for the configured start time. The code below provides an example of how to handle this condition: In this case it abandons the
 *     ranging exchange and simply goes back to awaiting another poll message. If this error handling code was not here, a late dwt_starttx() would
 *     result in the code flow getting stuck waiting subsequent RX event that will will never come. The companion "initiator" example (ex_06a) should
 *     timeout from awaiting the "response" and proceed to send another poll in due course to initiate another ranging exchange.
 * 11. The user is referred to DecaRanging ARM application (distributed with EVK1000 product) for additional practical example of usage, and to the
 *     DW1000 API Guide for more details on the DW1000 driver functions.
 **************************************************/
