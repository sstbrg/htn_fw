#include "dw1000.h"
#include "main.h"
#include "uart_communication.h"

extern uint8_t HW_Version;

/* Frame sequence number, incremented after each transmission. */
static uint8 frame_seq_nb = 0;
static uint64 poll_rx_ts;
static uint64 resp_tx_ts;

/* Frames used in the ranging process. See NOTE 3 below. */
twr_anchor_msg_t tx_poll_msg;
twr_tag_msg_t rx_resp_msg;
twr_tag_msg_t tx_resp_msg;
uint8_t dummy_buff[1024] = {0};


//accessing dw1000 quality buffers data
dwt_rxdiag_t diagnostics;


int dwmInit() {
	/* Reset and initialise DW1000.
	 * For initialisation, DW1000 clocks must be temporarily set to crystal speed. After initialisation SPI rate can be increased for optimum
	 * performance. */
	reset_DW1000(); /* Target specific drive of RSTn line into DW1000 low for a period. */
	port_set_dw1000_slowrate();
	if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR) {
		dwmInfo.no_uwb = true;
		return 0;
	}

	dwmInfo.no_uwb = false;
		//Anchor
	tx_poll_msg.src = (dwmInfo.mine_pc_id << 2) | dwmInfo.anchor_id;

		//Tag
	rx_resp_msg.des = (dwmInfo.mine_pc_id << 2) | dwmInfo.anchor_id;
	tx_resp_msg.src = (dwmInfo.mine_pc_id << 2) | dwmInfo.anchor_id;

	return 1;
}


void dwmStatus() {
	uint32 ID = dwt_readdevid();
	// Read and validate device ID, return -1 if not recognised
	if (DWT_DEVICE_ID != ID) // MP IC ONLY (i.e. DW1000) FOR THIS CODE
			{
		dwmInfo.no_uwb = true;
	}
	else{
		dwmInfo.no_uwb = false;//
	}
}

void dwt_configenable_user_accumem_read(bool bit_set){
	uint32 reg = 0;
	if(bit_set == true){
		reg = dwt_read32bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET);
		reg |= (PMSC_CTRL0_FACE|PMSC_CTRL0_AMCE);
		dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, reg);
	}else if (bit_set == false){
		reg = dwt_read32bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET);
		reg &= ~(PMSC_CTRL0_FACE|PMSC_CTRL0_AMCE);
		dwt_write8bitoffsetreg(PMSC_ID, PMSC_CTRL0_OFFSET, reg);
	}
}



void dwmDiagStatus(){
		//Reading buffers
//	dwt_readdiagnostics(&dwmInfo.diagnostics); // 02.09.2025 now appears in dwRange.
	dwt_readeventcounters(&dwmInfo.counter);

		//Making calculations
	//page 46 in user manual, RX Level
	dwmInfo.diagnostics.rxLvl = (10 * log10((dwmInfo.diagnostics.cirPwr * pow(2,17))/pow(dwmInfo.diagnostics.rxPreamCount,2)) - PRF_64Mhz) * 100; // multiplied by 100 to have fraction
}

void dwConfig(dwt_config_t *config) {
	dwt_txconfig_t tx_config;

	tx_config.PGdly = 0xC2; //Channel 2 TC_PGDELA
	if (HW_Version == 2)
	{
		tx_config.power =  MAX_POWER;
	}
	else{
		tx_config.power = 0x40404040; //10db
	}
//	tx_config.power = 0x0; //15db
//  tx_config.power = 0x67676767; //11db
//	tx_config.power = 0x40404040; //10db
//	tx_config.power = 0x60606060;//7.5db
//	tx_config.power = 0x80808080;//5db
//	tx_config.power = 0Xa0a0A0A0;//2.5DB
//	tx_config.power =  0xC0C0C0C0; //0db
//	tx_config.power =  0xE0E0E0E0; //turned off - 0db

	dwt_setlnapamode(DWT_PA_ENABLE | DWT_LNA_ENABLE ); //While using PA LNA Enable
	dwt_enablegpioclocks(); //REQUIRED =: Sets PMSC_CTRL0 GPCE|GPRN so EXTRXE (GPIO6) actualy drives the LNA

	port_set_dw1000_fastrate();

	/* Configure DW1000. See NOTE 6 below. */
	dwt_configure(config);

	/* Apply default antenna delay value. See NOTE 2 below. */
	dwt_setrxantennadelay(RX_ANT_DLY);
	dwt_settxantennadelay(TX_ANT_DLY);

	dwt_setsmarttxpower(DISABLE);

	dwt_configuretxrf(&tx_config);//max power refer user manual pg.106

	dwt_setleds(LEDS_ON);

#ifdef NEW_NTM
			//set lower SNR for NLOS operation
			dwt_write8bitoffsetreg(LDE_IF_ID, LDE_CFG1_OFFSET, 0x6D); //refer user manual pg.175
			dwt_write16bitoffsetreg(LDE_IF_ID, LDE_CFG2_OFFSET, 0x0003); //refer user manual pg.177*/
			//printf("0x2E = %x\n\r",dwt_read32bitreg(LDE_IF_ID));
		#endif
	/* Set expected response's delay and timeout. See NOTE 1 and 5 below.
	 * As this example only handles one incoming frame with always the same delay and timeout, those values can be set here once for all. */
	dwt_setrxaftertxdelay(ANC_POLL_TX_TO_RESP_RX_DLY_UUS);
	dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

	// Disable fine grain TX sequencing
	dwt_setfinegraintxseq(0);

	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
}

void dwScanMode(void) {
	dwt_rxenable(DWT_START_RX_IMMEDIATE);
	status_reg = dwt_read32bitreg(SYS_STATUS_ID);
	uint32_t SavedTimer = _1ms_tick;

	dwmInfo.distance = 0;

	while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID))
			& (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_DF_CLK))) {

		if(dwmInfo.uart_income == true){
			dwt_forcetrxoff();
			break;
		}
		if ((_1ms_tick - SavedTimer) > THRESHOLD_MS) {
			dwmInfo.RxToCounter++;
			break;
		}

	}

	dwmInfo.statusReg = status_reg;

	if ((status_reg & SYS_STATUS_RXFCG) && !(status_reg & SYS_STATUS_ALL_DF_CLK)) {

		uint32 frame_len;
		twr_tag_msg_t rx_buffer;

		/* Clear good RX frame event in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		/* A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
		if (frame_len <= sizeof(rx_buffer)) {
			dwt_readrxdata((uint8_t*)&rx_buffer, frame_len, 0);
		}
		else {
			dummy_buff[0]++;
			dwt_readrxdata(&dummy_buff[1], frame_len,0);
			return;
		}

		/* Check that the frame is a poll sent by "SS TWR initiator" example.
		 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
		if(rx_buffer.t_a_flag == ANCHOR_FLAG){ // if detected Anchor message check if its your own
			if ((rx_buffer.src >> 2) == (tx_poll_msg.src>>2)){ // compare pc_ids
				dwmInfo.detec_pc_id = true;
			}
			else{
				dwmInfo.detec_pc_id = false;
			}
		}else if (rx_buffer.t_a_flag == TAG_FLAG){
			if ((rx_buffer.des >> 2) == (tx_poll_msg.src>>2)){ // compare pc_ids
				dwmInfo.detec_pc_id = true;
			}
			else{
				dwmInfo.detec_pc_id = false;
			}
		}

		if (dwmInfo.detec_pc_id == true){
			dwmInfo.backoff_timer = 0;
		}else{
			dwmInfo.backoff_timer = _1ms_tick + random() % TIME_TOP_RANGE_MS + MIN_BACKOFF_MS; //TODO: [10ms - 200ms]
		}

	} else {
		/* Clear RX error events in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID,
				SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);

		/* Reset RX to properly reinitialise LDE operation. */
		dwt_rxreset();
	}

	if(status_reg == 0x800002){
		printf("break1");
	}

}
/*************************
 *
 * 		RANGE
 *
 */
char errors_cnt[32] = {0};
dwt_deviceentcnts_t counters;

int dwRange(void) {
	dwmInfo.dw_ready = DW_NO_VALID_DATA;
	dwmInfo.distance = 0;

	tx_poll_msg.des = dwmInfo.tag_dest;

	rx_resp_msg.src = dwmInfo.tag_dest;

	/* Write frame data to DW1000 and prepare transmission. See NOTE 7 below. */
	tx_poll_msg.msd_index = frame_seq_nb;
	// write frame index to rx to compare the correct data response received
	rx_resp_msg.msd_index = frame_seq_nb;

	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
	dwt_writetxdata(sizeof(tx_poll_msg), (uint8_t*)&tx_poll_msg, 0); /* Zero offset in TX buffer. */
	dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1); /* Zero offset in TX buffer, ranging. */

	/* Start transmission, indicating that a response is expected so that reception is enabled automatically after the frame is sent and the delay
	 * set by dwt_setrxaftertxdelay() has elapsed. */
	dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

	dwmInfo.TxCounter++;
	/* We assume that the transmission is achieved correctly, poll for reception of a frame or error/timeout. See NOTE 8 below. */
	uint32_t SavedTimer = _1ms_tick;

	status_reg = dwt_read32bitreg(SYS_STATUS_ID); // workaround to fix weird behavior.

//	while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID))
//			& (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_DF_CLK))) {
//		 if ((_1ms_tick - SavedTimer) > THRESHOLD_MS) {
//			 break;
//		 }
//
//	};
	while (1){
		status_reg = dwt_read32bitreg(SYS_STATUS_ID);

		if (status_reg & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)) {
			break;
		}

		if  ((_1ms_tick - SavedTimer) > THRESHOLD_MS){
			dwmInfo.RxToCounter++;
			dwt_forcetrxoff();
			dwmInfo.statusReg = SYS_STATUS_RXFCE; // stop begin rx if pc adress you
			break;
		}
	};

	dwmInfo.statusReg = status_reg;

	if ((status_reg & SYS_STATUS_RXFCG) && !(status_reg & SYS_STATUS_ALL_DF_CLK)) {
		uint32 frame_len;
		twr_tag_msg_t rx_buffer;
		/* Clear good RX frame event in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		/* A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;
		if (frame_len == sizeof(rx_buffer)) {
			dwt_readrxdata((uint8_t *)&rx_buffer, frame_len, 0);
		}
		else {
			dummy_buff[0]++;
			dwt_readrxdata(&dummy_buff[1], frame_len,0);
			return 0;
		}

		/* Check that the frame is the expected response from the companion "SS TWR responder" example.
		 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
		if (memcmp((uint8_t*)&rx_buffer, (uint8_t*)&rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
			dwmInfo.RxCounter++;
			uint32 poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
			int32 rtd_init, rtd_resp;
			float clockOffsetRatio;
			dwmInfo.tag_no_lora = rx_buffer.tag_no_lora;
			/* Retrieve poll transmission and response reception timestamps. See NOTE 9 below. */
			poll_tx_ts = dwt_readtxtimestamplo32();
			resp_rx_ts = dwt_readrxtimestamplo32();

			/* Read carrier integrator value and calculate clock offset ratio. See NOTE 11 below. */
//			clockOffsetRatio = dwt_readcarrierintegrator()
//					* (FREQ_OFFSET_MULTIPLIER * HERTZ_TO_PPM_MULTIPLIER_CHAN_2
//							/ 1.0e6);
			double freqOffsetMultiplier = (config.dataRate == DWT_BR_110K) ? FREQ_OFFSET_MULTIPLIER_110KB : FREQ_OFFSET_MULTIPLIER;
			clockOffsetRatio = dwt_readcarrierintegrator()
								* (freqOffsetMultiplier * HERTZ_TO_PPM_MULTIPLIER_CHAN_2 / 1.0e6);

			/* Get timestamps embedded in response message. */
			resp_msg_get_ts((uint8_t*)&rx_buffer.poll_rx_ts_id, &poll_rx_ts);
			resp_msg_get_ts((uint8_t*)&rx_buffer.resp_tx_ts_id, &resp_tx_ts);

			/* Compute time of flight and distance, using clock offset ratio to correct for differing local and remote clock rates */
			rtd_init = resp_rx_ts - poll_tx_ts; //anchor
			rtd_resp = (resp_tx_ts - poll_rx_ts)* (1 - clockOffsetRatio); //Tag


			dwmInfo.tof = ((rtd_init - rtd_resp) / 2.0) * DWT_TIME_UNITS;
			if (dwmInfo.tof < 0){
				dwmInfo.dw_ready = DW_NO_VALID_DATA;
			}else{
				dwmInfo.distance = dwmInfo.tof * SPEED_OF_LIGHT*100;
				dwmInfo.dw_ready = VALID_DATA;

				dwt_readdiagnostics(&dwmInfo.diagnostics); // reading diagnostics for bit usage at need

				dwmInfo.wrong_ID = false;
			}
		}else{
			dwmInfo.wrong_ID = true;
		}
	} else {
		/* Clear RX error/timeout events in the DW1000 status register. */
		status_reg &= SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR;
		for (int i = 0 ; i < 32; i++)
			errors_cnt[i] += !!(status_reg & (1<<i));


		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_GOOD);

		/* Reset RX to properly reinitialize LDE operation. */
		dwt_rxreset();
	}


	/* Execute a delay between ranging exchanges. */
	/* Increment frame sequence number after transmission of the poll message (modulo 256). */
	frame_seq_nb++;

	return 0;
}


/*! -----------------------------------------------------------------------------------------------------------------
 *  Tag mode
 */

 int dwTagMode(void) {
	/* Activate reception immediately. */
	tx_resp_msg.tag_no_lora = lora_rxInfo.no_lora;
	uint8_t RetVal = false;

	dwt_rxenable(DWT_START_RX_IMMEDIATE);

	/* Poll for reception of a frame or error/timeout. See NOTE 6 below. */
	uint32_t SavedTimer = _1ms_tick;

	while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {
		if  ((_1ms_tick - SavedTimer) > THRESHOLD_MS){
			dwmInfo.RxToCounter++;
			break;
		}
	};
	dwmInfo.statusReg = status_reg;

	if (status_reg & SYS_STATUS_RXFCG) {

		uint32 frame_len;
		twr_anchor_msg_t rx_buffer;

		/* Clear good RX frame event in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		/* A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
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
		if (rx_buffer.des == tx_resp_msg.src) {
			tx_resp_msg.des = rx_buffer.src;
			uint32 resp_tx_time;
			int ret;
			RetVal = true;
			/* Retrieve poll reception timestamp. */
			poll_rx_ts = get_rx_timestamp_u64();

			/* Compute final message transmission time. See NOTE 7 below. */
			resp_tx_time = (poll_rx_ts
					+ (TAG_POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
			dwt_setdelayedtrxtime(resp_tx_time);

			/* Response TX timestamp is the transmission time we programmed plus the antenna delay. */
			resp_tx_ts = (((uint64) (resp_tx_time & 0xFFFFFFFEUL)) << 8)
					+ TX_ANT_DLY;

           	tx_resp_msg.msd_index = rx_buffer.msd_index;//return the same message index

           	/* Write all timestamps in the final message. See NOTE 8 below. */
			resp_msg_set_ts((uint8_t*)&tx_resp_msg.poll_rx_ts_id, poll_rx_ts);
			resp_msg_set_ts((uint8_t*)&tx_resp_msg.resp_tx_ts_id, resp_tx_ts);

			/* Write and send the response message. See NOTE 9 below. */
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
			}
		}
	} else {
		/* Clear RX error events in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID,
				SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_GOOD);

		/* Reset RX to properly reinitialise LDE operation. */
		dwt_rxreset();
	}

	return (RetVal);
}



/*! ------------------------------------------------------------------------------------------------------------------
 * @fn resp_msg_get_ts()
 *
 * @brief Read a given timestamp value from the response message. In the timestamp fields of the response message, the
 *        least significant byte is at the lower address.
 *
 * @param  ts_field  pointer on the first byte of the timestamp field to get
 *         ts  timestamp value
 *
 * @return none
 */
static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts) {
	int i;
	*ts = 0;
	for (i = 0; i < RESP_MSG_TS_LEN; i++) {
		*ts += ts_field[i] << (i * 8);
	}
}
//#endif

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
	uint64 ts = 0;
	///-------------------- // simpler
	uint8 ts_tab[8] = {0};
	dwt_readrxtimestamp(ts_tab);
	ts = *(uint64*)ts_tab;

	//------------ origin

//	uint8 ts_tab[5];
//	int i;
//	dwt_readrxtimestamp(ts_tab);
//	for (i = 4; i >= 0; i--) {
//		ts <<= 8;
//		ts |= ts_tab[i];
//	}
	return ts;

}

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

/***************************************************
 * NOTES:
 *
 * 1. The single-sided two-way ranging scheme implemented here has to be considered carefully as the accuracy of the distance measured is highly
 *    sensitive to the clock offset error between the devices and the length of the response delay between frames. To achieve the best possible
 *    accuracy, this response delay must be kept as low as possible. In order to do so, 6.8 Mbps data rate is used in this example and the response
 *    delay between frames is defined as low as possible. The user is referred to User Manual for more details about the single-sided two-way ranging
 *    process.  NB:SEE ALSO NOTE 11.
 * 2. The sum of the values is the TX to RX antenna delay, this should be experimentally determined by a calibration process. Here we use a hard coded
 *    value (expected to be a little low so a positive error will be seen on the resultant distance estimate. For a real production application, each
 *    device should have its own antenna delay properly calibrated to get good precision when performing range measurements.
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
 * 5. This timeout is for complete reception of a frame, i.e. timeout duration must take into account the length of the expected frame. Here the value
 *    is arbitrary but chosen large enough to make sure that there is enough time to receive the complete response frame sent by the responder at the
 *    6.8M data rate used (around 200  s).
 * 6. In a real application, for optimum performance within regulatory limits, it may be necessary to set TX pulse bandwidth and TX power, (using
 *    the dwt_configuretxrf API call) to per device calibrated values saved in the target system or the DW1000 OTP memory.
 * 7. dwt_writetxdata() takes the full size of the message as a parameter but only copies (size - 2) bytes as the check-sum at the end of the frame is
 *    automatically appended by the DW1000. This means that our variable could be two bytes shorter without losing any data (but the sizeof would not
 *    work anymore then as we would still have to indicate the full length of the frame to dwt_writetxdata()).
 * 8. We use polled mode of operation here to keep the example as simple as possible but all status events can be used to generate interrupts. Please
 *    refer to DW1000 User Manual for more details on "interrupts". It is also to be noted that STATUS register is 5 bytes long but, as the event we
 *    use are all in the first bytes of the register, we can use the simple dwt_read32bitreg() API call to access it instead of reading the whole 5
 *    bytes.
 * 9. The high order byte of each 40-bit time-stamps is discarded here. This is acceptable as, on each device, those time-stamps are not separated by
 *    more than 2**32 device time units (which is around 67 ms) which means that the calculation of the round-trip delays can be handled by a 32-bit
 *    subtraction.
 * 10. The user is referred to DecaRanging ARM application (distributed with EVK1000 product) for additional practical example of usage, and to the
 *     DW1000 API Guide for more details on the DW1000 driver functions.
 * 11. The use of the carrier integrator value to correct the TOF calculation, was added Feb 2017 for v1.3 of this example.  This significantly
 *     improves the result of the SS-TWR where the remote responder unit's clock is a number of PPM offset from the local inmitiator unit's clock.
 *     As stated in NOTE 2 a fixed offset in range will be seen unless the antenna delsy is calibratred and set correctly.
 *
 **************************************************/
