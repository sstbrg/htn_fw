#include "dw1000.h"
#include "main.h"
#include "uart_communication.h"
#include "fota.h"
#include "flash_handler.h"

extern uint8_t HW_Version;

/* Frame sequence number, incremented after each transmission. */
static uint8 frame_seq_nb = 0;
static uint64 poll_rx_ts;
static uint64 resp_tx_ts;

/* Frames used in the ranging process. See NOTE 3 below. */
twr_anchor_msg_t tx_poll_msg;
twr_anchor_ext_msg_t tx_poll_ext;
twr_anchor_ext2_msg_t tx_poll_ext2;
twr_tag_msg_t rx_resp_msg;
twr_tag_msg_t tx_resp_msg;
uint8_t dummy_buff[1024] = {0};

uint16_t backoff_top_ms = TIME_TOP_RANGE_MS;
uint16_t backoff_min_ms = MIN_BACKOFF_MS;
uint8_t lde_cfg1 = 0x6D;	// LOS default; live-tunable via opcode 14 (LDE_IF_ID:LDE_CFG1_OFFSET)


//accessing dw1000 quality buffers data
dwt_rxdiag_t diagnostics;


static bool dw_init_ok = false;	// dwt_initialise() succeeded and dwConfig() ran since
static uint32_t dw_retry_tick = 0;

int dwmInit() {
	/* Reset and initialise DW1000.
	 * For initialisation, DW1000 clocks must be temporarily set to crystal speed. After initialisation SPI rate can be increased for optimum
	 * performance. */
	reset_DW1000(); /* Target specific drive of RSTn line into DW1000 low for a period. */
	port_set_dw1000_slowrate();
	dw_init_ok = false;
	if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR) {
		dwmInfo.no_uwb = true;
		return 0;
	}

	dwmInfo.no_uwb = false;
	dw_init_ok = true;
	dwApplyXtalTrim();
	dwSetFrameIds();

	return 1;
}

void dwSetFrameIds(void) {
	uint16_t id = (dwmInfo.mine_pc_id << 2) | dwmInfo.anchor_id;
	tx_poll_msg.src = id;			// anchor poll
	rx_resp_msg.des = id;			// expected tag response
	// imposter response: bit 15 is the LoRa-only flag on the UART side and never part of an
	// over-the-air id, so answer the id a poll can actually carry
	tx_resp_msg.src = id & 0x7FFF;
}

// Crystal trim: self-id flash page slot 3, 0..31; 0xFFFF = keep what dwt_initialise() loaded
// (the chip's OTP trim, or mid-range 0x10 when the OTP has none).
void dwApplyXtalTrim(void) {
	uint16_t t = ((const uint16_t*)FLASH_SELF_ID_ADDRESS)[3];
	if (t <= FS_XTALT_MASK)
		dwt_setxtaltrim((uint8_t)t);
}

// TX antenna delay in force. A delayed response computes its own TX timestamp (programmed time +
// this), so it must be the applied value: with the compile-time default a calibrated TX delay had
// no effect on the range.
static uint16_t tx_ant_dly = TX_ANT_DLY;

// Antenna delays: self-id flash page slots 1 (TX) and 2 (RX); 0 or 0xFFFF = not calibrated
void dwApplyAntennaDelays(void) {
	const uint16_t *page = (const uint16_t*)FLASH_SELF_ID_ADDRESS;
	uint16_t tx = page[1], rx = page[2];
	tx_ant_dly = (tx == 0 || tx == 0xFFFF) ? TX_ANT_DLY : tx;
	dwt_settxantennadelay(tx_ant_dly);
	dwt_setrxantennadelay((rx == 0 || rx == 0xFFFF) ? RX_ANT_DLY : rx);
}


void dwmStatus() {
	uint32 ID = dwt_readdevid();
	// Read and validate device ID, return -1 if not recognised
	if (DWT_DEVICE_ID != ID) // MP IC ONLY (i.e. DW1000) FOR THIS CODE
			{
		dwmInfo.no_uwb = true;
	}
	else{
		dwmInfo.no_uwb = !dw_init_ok;	// a valid id on a chip that never initialised is not usable
	}
}

// Main loop: with no_uwb set, check again every 5 s - a valid id on an initialised chip clears it,
// anything else gets a full re-initialisation. Without this one bad read disabled UWB until reset.
void dwRecover(void) {
	if (!dwmInfo.no_uwb || HAL_GetTick() - dw_retry_tick < 5000)
		return;
	dw_retry_tick = HAL_GetTick();
	if (dw_init_ok && dwt_readdevid() == DWT_DEVICE_ID) {
		dwmInfo.no_uwb = false;
		return;
	}
	if (dwmInit())
		dwConfig(&config);
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

	/* Antenna delays: per-device values from flash, else the defaults. See NOTE 2 below. */
	dwApplyAntennaDelays();

	dwt_setsmarttxpower(DISABLE);

	dwt_configuretxrf(&tx_config);//max power refer user manual pg.106

	dwt_setleds(LEDS_ON);

#ifdef NEW_NTM
			//set lower SNR for NLOS operation
			dwt_write8bitoffsetreg(LDE_IF_ID, LDE_CFG1_OFFSET, dwLdeCfg1()); //refer user manual pg.175; live-tuned value or the 9G NLOS preset
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
	Link_IdleTick();
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
		twr_tag_ext2_msg_t rx_buffer;	// any UWB frame up to an extended v2 response

		/* Clear good RX frame event in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		/* A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
		uint8_t first = 0;
		dwt_readrxdata(&first, 1, 0);
		if (first == FOTA_FLAG) {
			fota_uwb_rx(frame_len);
			return;
		}
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

		// Arm once per busy period: a running backoff is left alone by both own and foreign frames,
		// so it always expires and the next SEARCH_TWR goes out (bounded wait). An expired
		// deadline counts as idle and can re-arm on the next foreign frame.
		bool running = Backoff_Left() != 0;
		if (!dwmInfo.detec_pc_id && !running){
			uint32_t deadline = _1ms_tick + random() % backoff_top_ms + backoff_min_ms;
			dwmInfo.backoff_timer = deadline ? deadline : 1;	// 0 means "not backing off"
			link_st.coexist_tick = HAL_GetTick();	// 9K: another station is on the air
		}

	} else {
		/* Clear RX error events, and any latched good-RX bits, in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID,
				SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_GOOD);

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

// dwExchange() results
#define EX_NONE		0	// no frame from the tag
#define EX_FRAME	1	// the tag answered, but no usable range
#define EX_VALID	2	// valid range

static uint8_t dwExchange(bool first);

int dwRange(void) {
	dwExchange(true);
	return 0;
}

/* One SS-TWR exchange. Legacy poll unless the tag has shown it understands the extended one
 * (see LINK_HELLO in dw1000.h); the link logic sees every outcome. */
static uint8_t dwExchange(bool first) {
	uint8_t result = EX_NONE;
	dwmInfo.dw_ready = DW_NO_VALID_DATA;
	dwmInfo.distance = 0;

	Link_BeforePoll();
	bool ext_poll = link_st.ext;
	bool hello = false;

	tx_poll_msg.des = dwmInfo.tag_dest;

	rx_resp_msg.src = dwmInfo.tag_dest;

	/* Write frame data to DW1000 and prepare transmission. See NOTE 7 below. */
	tx_poll_msg.msd_index = frame_seq_nb;
	// write frame index to rx to compare the correct data response received
	rx_resp_msg.msd_index = frame_seq_nb;

	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
	if (ext_poll) {
		memcpy((uint8_t*)&tx_poll_ext, (const uint8_t*)&tx_poll_msg, ALL_MSG_COMMON_LEN);	// flag, ids and sequence number
		tx_poll_ext.i9 = LINK_EXT_MARK;
		Link_FillExtPoll(&tx_poll_ext);
		if (Link_ItemUwbDue()) {	// v2: the same, plus the configuration item (backup path)
			memcpy((uint8_t*)&tx_poll_ext2, (const uint8_t*)&tx_poll_ext, sizeof(twr_anchor_ext_msg_t) - 2);
			Link_ItemFillPoll(&tx_poll_ext2);
			dwt_writetxdata(sizeof(tx_poll_ext2), (uint8_t*)&tx_poll_ext2, 0);
			dwt_writetxfctrl(sizeof(tx_poll_ext2), 0, 1);
		} else {
			dwt_writetxdata(sizeof(tx_poll_ext), (uint8_t*)&tx_poll_ext, 0);
			dwt_writetxfctrl(sizeof(tx_poll_ext), 0, 1);
		}
	} else {
		hello = Link_WantHello();
		tx_poll_msg.i9 = hello ? LINK_HELLO : ANCHOR_FLAG;	// 0xE0 is what anc3.1.5 sends
		dwt_writetxdata(sizeof(tx_poll_msg), (uint8_t*)&tx_poll_msg, 0); /* Zero offset in TX buffer. */
		dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1); /* Zero offset in TX buffer, ranging. */
	}

	/* Start transmission, indicating that a response is expected so that reception is enabled automatically after the frame is sent and the delay
	 * set by dwt_setrxaftertxdelay() has elapsed. */
	dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

	dwmInfo.TxCounter++;
	/* We assume that the transmission is achieved correctly, poll for reception of a frame or error/timeout. See NOTE 8 below. */
	uint32_t SavedTimer = _1ms_tick;

	status_reg = dwt_read32bitreg(SYS_STATUS_ID); // workaround to fix weird behavior.

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
		twr_tag_ext2_msg_t rx_buffer;	// legacy, extended or extended v2 response; the legacy part is common
		/* Clear good RX frame event in the DW1000 status register. */
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

		/* A frame has been received, read it into the local buffer. */
		frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFLEN_MASK;
		if (frame_len <= sizeof(rx_buffer)) {
			dwt_readrxdata((uint8_t *)&rx_buffer, frame_len, 0);
		}
		else {
			dummy_buff[0]++;
			dwt_readrxdata(&dummy_buff[1], frame_len,0);
			Link_OnExchange(EX_NONE, first, ext_poll, false);
			return EX_NONE;		// no sequence step, as anc3.1.5
		}

		/* Check that the frame is the expected response from the companion "SS TWR responder" example.
		 * As the sequence number field of the frame is not relevant, it is cleared to simplify the validation of the frame. */
		if (frame_len >= sizeof(twr_tag_msg_t)
				&& memcmp((uint8_t*)&rx_buffer, (uint8_t*)&rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
			dwmInfo.RxCounter++;
			result = EX_FRAME;
			uint32 poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
			int32 rtd_init, rtd_resp;
			float clockOffsetRatio;
			bool ext_resp = ext_poll && (frame_len == sizeof(twr_tag_ext_msg_t) || frame_len == sizeof(twr_tag_ext2_msg_t));
			Link_OnResponse((const twr_tag_ext_msg_t*)&rx_buffer, ext_poll, ext_resp, hello);
			if (ext_resp && frame_len == sizeof(twr_tag_ext2_msg_t))
				Link_ItemOnUwb(&rx_buffer);
			/* Retrieve poll transmission and response reception timestamps. See NOTE 9 below. */
			poll_tx_ts = dwt_readtxtimestamplo32();
			resp_rx_ts = dwt_readrxtimestamplo32();
			uint32_t rx_tick = HAL_GetTick();

			/* Read carrier integrator value and calculate clock offset ratio. See NOTE 11 below. */
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
				result = EX_VALID;

				dwt_readdiagnostics(&dwmInfo.diagnostics); // reading diagnostics for bit usage at need
				dwmInfo.exchange_tick_ms = rx_tick;
				dwLatchExchange();

				dwmInfo.wrong_ID = false;
			}
			Link_OnExchange(result, first, ext_poll, ext_resp);
		}else{
			dwmInfo.wrong_ID = true;
			Link_OnExchange(EX_NONE, first, ext_poll, false);
		}
	} else {
		/* Clear RX error/timeout events in the DW1000 status register. */
		status_reg &= SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR;
		for (int i = 0 ; i < 32; i++)
			errors_cnt[i] += !!(status_reg & (1<<i));


		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_GOOD);

		/* Reset RX to properly reinitialize LDE operation. */
		dwt_rxreset();
		Link_OnExchange(EX_NONE, first, ext_poll, false);
	}

	/* Increment frame sequence number after transmission of the poll message (modulo 256). */
	frame_seq_nb++;

	return result;
}

/* What the host is told about one exchange; 9F keeps the shortest of a round's exchanges. */
typedef struct {
	uint32_t distance;
	double tof;
	dwt_rxdiag_t diagnostics;
	uint16_t lde_ppindx, rxpacc_nosat;
	uint32_t exchange_tick_ms;
	int8_t tag_lora_tx_dbm;
	uint8_t tag_uwb_tx_level, tag_no_lora;
	uint16_t cir_fp_index, cir_start;
	uint8_t cir_n;
	uint8_t cir_buf[sizeof(dwmInfo.cir_buf)];
} DwExchange_t;

static DwExchange_t best_ex;

static void ex_save(DwExchange_t *e) {
	e->distance = dwmInfo.distance;
	e->tof = dwmInfo.tof;
	e->diagnostics = dwmInfo.diagnostics;
	e->lde_ppindx = dwmInfo.lde_ppindx;
	e->rxpacc_nosat = dwmInfo.rxpacc_nosat;
	e->exchange_tick_ms = dwmInfo.exchange_tick_ms;
	e->tag_lora_tx_dbm = dwmInfo.tag_lora_tx_dbm;
	e->tag_uwb_tx_level = dwmInfo.tag_uwb_tx_level;
	e->tag_no_lora = dwmInfo.tag_no_lora;
	e->cir_fp_index = dwmInfo.cir_fp_index;
	e->cir_start = dwmInfo.cir_start;
	e->cir_n = dwmInfo.cir_n;
	memcpy(e->cir_buf, dwmInfo.cir_buf, sizeof(e->cir_buf));
}

static void ex_restore(const DwExchange_t *e) {
	dwmInfo.distance = e->distance;
	dwmInfo.tof = e->tof;
	dwmInfo.diagnostics = e->diagnostics;
	dwmInfo.lde_ppindx = e->lde_ppindx;
	dwmInfo.rxpacc_nosat = e->rxpacc_nosat;
	dwmInfo.exchange_tick_ms = e->exchange_tick_ms;
	dwmInfo.tag_lora_tx_dbm = e->tag_lora_tx_dbm;
	dwmInfo.tag_uwb_tx_level = e->tag_uwb_tx_level;
	dwmInfo.tag_no_lora = e->tag_no_lora;
	dwmInfo.cir_fp_index = e->cir_fp_index;
	dwmInfo.cir_start = e->cir_start;
	dwmInfo.cir_n = e->cir_n;
	memcpy(dwmInfo.cir_buf, e->cir_buf, sizeof(e->cir_buf));
}

/* SEARCH_TWR round (9F). NLOS error is one-sided (a reflection only reads long), so with repeats
 * on the anchor runs K exchanges back-to-back and reports the shortest valid one with its own
 * diagnostics. K: k_req from the request, else the configured k_max, else 3 while RX-FP sits above
 * the soft threshold (or the NLOS LDE preset is on) and 1 otherwise. A miss ends the round early. */
int dwRangeRound(uint8_t k_req) {
	uint8_t k = 1;
	if (link_cfg.features & LINK_F_REPEATS) {
		if (k_req)
			k = k_req;
		else if (link_cfg.k_max)
			k = link_cfg.k_max;
		else if (link_st.last_rx_dbm > LINK_STRONG_DBM)	// weak (far): the shortest of K would be noise
			k = (link_st.lde_nlos || link_st.last_rxfp_db > LINK_RXFP_SOFT_DB) ? 3 : 1;
		else
			k = 1;
		if (k > LINK_K_MAX)
			k = LINK_K_MAX;
		if (Link_Coexisting())
			k = 1;		// 9K: repeats touch the air; the section 8 timings assume one exchange
	}

	bool got = false;
	uint8_t used = 0;
	while (used < k) {
		uint8_t r = dwExchange(used == 0);
		used++;
		if (r != EX_VALID)
			break;
		if (!got || dwmInfo.distance < best_ex.distance) {
			if (k > 1)
				ex_save(&best_ex);
			got = true;
		}
	}
	if (got) {
		if (k > 1)
			ex_restore(&best_ex);
		dwmInfo.dw_ready = VALID_DATA;
	}
	link_st.k_used = used;
	return 0;
}

#define RXPACC_NOSAT_OFFSET 0x2C	// DRX_CONF sub-register, 2 bytes

// Latch what the host needs to attribute to this exchange while the DW1000 still holds the
// received frame: RX has not re-armed, so LDE and the accumulator belong to this response.
void dwLatchExchange(void) {
	dwmInfo.lde_ppindx = dwt_read16bitoffsetreg(LDE_IF_ID, LDE_PPINDX_OFFSET);
	dwmInfo.rxpacc_nosat = dwt_read16bitoffsetreg(DRX_CONF_ID, RXPACC_NOSAT_OFFSET);

	if (!(link_cfg.features & LINK_F_CIR)) {	// 9B off: no accumulator read, nothing latched
		dwmInfo.cir_n = 0;
		return;
	}
	// 32 complex samples from 8 before the detected first path (FP_INDEX is Q10.6)
	uint16_t fp = dwmInfo.diagnostics.firstPath;
	int start = (fp >> 6) - CIR_WINDOW_PRE;
	if (start < 0)
		start = 0;
	if (start > (ACC_MEM_LEN / 4) - CIR_WINDOW_N)
		start = (ACC_MEM_LEN / 4) - CIR_WINDOW_N;
	dwt_readaccdata(dwmInfo.cir_buf, 1 + 4 * CIR_WINDOW_N, start * 4);	// clocks forced on inside
	dwmInfo.cir_fp_index = fp;
	dwmInfo.cir_start = start;
	dwmInfo.cir_n = CIR_WINDOW_N;
}

/*************************
 *
 * 		LINK ADAPTATION (doc section 9, rule zero)
 *
 * The link is never lost. Every adaptive change fails toward maximum power on the legacy frames:
 *  - extended frames only after the tag answered a hello; 2 unanswered extended polls, a legacy
 *    answer, 1 s without a response or another tag put the anchor back on legacy polls, which the
 *    tag always answers at maximum;
 *  - UWB (9D): the tag sends each response at the level that poll asked for, so there is no shared
 *    state to get out of step. Louder at once; quieter one 3 dB level at a time, after 2 good
 *    responses and step_interval, never below 10 dB over sensitivity. One miss: back to the
 *    previous level; two: maximum;
 *  - LoRa (9D/9J, the owner anchor only): the tag's LoRa power is shared state, so a quieter step
 *    is request (seq) -> ack -> confirm -> proof, each repeated up to LINK_TRIES polls; louder
 *    takes effect at once. No packet for LINK_LORA_LOST_MS, a bad SNR or a failed confirm asks for
 *    more power. The tag drops back by itself when the owner goes quiet (LINK_REVERT_MS / LINK_SILENCE_MS);
 *  - switching a feature off asks for maximum until the tag shows it, then returns to legacy polls.
 */
LinkCfg_t link_cfg = { .features = 0, .k_max = 0, .lora_target_dbm = -70, .lora_band_db = 6,
		.uwb_target_dbm = -80, .uwb_band_db = 5, .profile_max = 0, .profile_override = 0xFF,
		.step_interval_ds = 20, .lora_owner = 0, .uwb_level_pin = 0xFF,
		.uwb_level_station = 0xFF, .lde_station = 0xFF };
LinkState_t link_st;

static uint32_t link_step_ms(void) {
	return (uint32_t)link_cfg.step_interval_ds * 100;
}

static void link_next_seq(void) {
	if (++link_st.seq == 0)
		link_st.seq = 1;	// 0 means "no confirm"
}

// Everything at maximum on legacy frames, as anc3.1.5.
static void link_reset(void) {
	link_st.ext = 0;
	link_st.tag_ver = 0;
	link_st.ext_miss = 0;
	link_st.reverting = 0;
	link_st.uwb_level = link_st.uwb_prev = 0;
	link_st.uwb_good = link_st.uwb_miss = link_st.uwb_mismatch = 0;
	link_st.uwb_floor = LINK_UWB_LEVELS - 1;
	link_st.station_hold = 0;
	link_st.prof_cur = link_st.prof_target = 0;	// the radio follows before the next poll
	link_st.prof_phase = PROF_P_IDLE;
	link_st.lora_phase = LINK_LP_IDLE;
	link_st.lora_cur = link_st.lora_req = LINK_LORA_MAX_DBM;
	link_st.lora_tries = link_st.lora_good = 0;
	link_st.echo_lora_dbm = LINK_LORA_UNKNOWN;
	link_st.echo_uwb_level = LINK_UWB_UNKNOWN;
	link_st.echo_profile = 0xFF;
	link_st.echo_pending = 0;
	link_st.echo_lde_nlos = 0;
}

// Give up on extended frames for a while.
static void link_fallback(void) {
	link_reset();
	link_st.ext_hold = 1;
	link_st.ext_hold_tick = HAL_GetTick();
}

static void link_lora_up(int8_t dbm) {
	if (dbm > LINK_LORA_MAX_DBM)
		dbm = LINK_LORA_MAX_DBM;
	link_next_seq();
	link_st.lora_req = dbm;
	link_st.lora_phase = LINK_LP_UP;
	link_st.lora_tries = 0;
	link_st.lora_good = 0;
}

static bool link_lora_active(void) {
	return (link_cfg.features & LINK_F_POWER) && link_cfg.lora_owner && link_st.ext && !link_st.reverting;
}

/* 9K: with two stations on one tag (the tag says so) or this anchor's backoff armed, nothing
 * adapts: legacy polls (the 12-byte frame the section 8 backoff and duty cycle were tuned on, tag at
 * maximum), K = 1, no requests. Adaptation resumes LINK_MULTI_MS after the last sign of a second station. */
static bool link_backoff_armed(void) {
	return Backoff_Left() != 0;
}

bool Link_Coexisting(void) {
	if (link_backoff_armed())
		link_st.coexist_tick = HAL_GetTick();
	return HAL_GetTick() - link_st.coexist_tick < LINK_MULTI_MS;
}

void Link_Init(void) {
	link_reset();
	link_st.ext_hold = 0;
	link_st.ext_tag = 0xFFFF;
	link_st.seq = (uint8_t)(random() | 1);	// not the seq a tag may still hold from before a reset
	link_st.lde_nlos = 0;
	link_st.lde_count = 0;
	link_st.last_rxfp_db = 0;
	link_st.last_rx_dbm = -200.0f;	// unknown: no repeats until a strong exchange
	link_st.k_used = 1;
	link_st.coexist_tick = HAL_GetTick() - LINK_MULTI_MS;
	link_st.prof_rx = 0;	// dwConfig() at boot runs profile 0
	link_st.prof_seq = 0;
	link_st.prof_reason = PROF_R_NONE;
}

// Extended frames carry the tag's TX power (9D) and LDE preset (9G).
static bool link_wants_ext(void) {
	return (link_cfg.features & (LINK_F_POWER | LINK_F_LDE_AUTO)) != 0;
}

bool Link_WantHello(void) {
	if (link_st.ext_hold && HAL_GetTick() - link_st.ext_hold_tick >= LINK_EXT_HOLD_MS)
		link_st.ext_hold = 0;
	return link_wants_ext() && !link_st.ext_hold;
}

static void link_before_poll(void);
static void link_profile_tick(void);
static void link_profile_after(bool answered);

void Link_BeforePoll(void) {
	link_before_poll();
	link_profile_tick();	// the configuration this poll goes out on
}

static void link_before_poll(void) {
	uint32_t now = HAL_GetTick();
	if (dwmInfo.tag_dest != link_st.ext_tag) {	// state belongs to one tag
		link_reset();
		link_st.ext_hold = 0;
		link_st.ext_tag = dwmInfo.tag_dest;
	}
	if (!link_st.ext)
		return;
	if (Link_Coexisting() && !link_st.reverting) {	// 9K: back to the ground state, legacy polls
		link_reset();
		return;
	}
	if (now - link_st.last_uwb_tick > LINK_UWB_LOST_MS) {	// quiet (host paused, tag away): ground state,
		link_reset();										// hello again at once
		return;
	}
	if (link_lora_active() && now - link_st.last_lora_tick > LINK_LORA_LOST_MS
			&& link_st.lora_cur < LINK_LORA_MAX_DBM && link_st.lora_phase != LINK_LP_UP)
		link_lora_up(LINK_LORA_MAX_DBM);	// packets stopped: find them at full power
}

void Link_FillExtPoll(twr_anchor_ext_msg_t *p) {
	// 9G for the tag too: the channel is reciprocal, so the preset this anchor chose from the
	// response also suits the tag's reception of the poll
	p->ctrl = ((link_cfg.features & LINK_F_LDE_AUTO) && link_st.lde_nlos && !link_st.reverting) ? LINK_CTRL_LDE_NLOS : 0;
	p->confirm_seq = 0;
	if (link_st.reverting) {
		if (++link_st.revert_tries > LINK_TRIES) {	// legacy polls put a new tag at maximum anyway
			link_reset();
			p->req_uwb_level = 0;
		}
		p->seq = link_st.seq;
		p->req_lora_dbm = LINK_LORA_MAX_DBM;
		p->req_uwb_level = 0;
		return;
	}
	if ((link_cfg.features & LINK_F_POWER) && link_cfg.uwb_level_pin < LINK_UWB_LEVELS) {
		link_st.uwb_level = link_cfg.uwb_level_pin;	// bench pin; the loop below does not move it
	} else if ((link_cfg.features & LINK_F_POWER) && link_cfg.uwb_level_station < LINK_UWB_LEVELS) {
		// the station's level, from the host; after misses, maximum for a while (unilateral)
		if (link_st.station_hold && HAL_GetTick() - link_st.station_hold_tick >= LINK_STATION_HOLD_MS)
			link_st.station_hold = 0;
		uint8_t lvl = link_st.station_hold ? 0 : link_cfg.uwb_level_station;
		link_st.uwb_level = (lvl > link_st.uwb_floor) ? link_st.uwb_floor : lvl;
	}
	p->req_uwb_level = link_st.uwb_level;

	if (!link_lora_active()) {
		p->seq = link_st.seq;
		p->req_lora_dbm = LINK_LORA_KEEP;
		return;
	}
	switch (link_st.lora_phase) {
	case LINK_LP_REQ:
		if (++link_st.lora_tries > LINK_TRIES) {	// never acknowledged: the tag did not switch
			link_st.lora_phase = LINK_LP_IDLE;
			link_st.lora_req = link_st.lora_cur;
		}
		break;
	case LINK_LP_CONF:
		if (++link_st.lora_tries > LINK_TRIES) {	// no proof: back to the last confirmed power
			link_lora_up(link_st.lora_cur);
		}
		break;
	case LINK_LP_UP:
		if (++link_st.lora_tries > LINK_TRIES) {	// keep asking, from a fresh seq
			link_lora_up(link_st.lora_req);
		}
		break;
	default:
		link_st.lora_req = link_st.lora_cur;	// keep-alive: same seq, current power
		break;
	}
	p->seq = link_st.seq;
	p->req_lora_dbm = link_st.lora_req;
	if (link_st.lora_phase == LINK_LP_CONF)
		p->confirm_seq = link_st.seq;
}

// A frame from the tag being ranged (legacy or extended); valid range or not.
void Link_OnResponse(const twr_tag_ext_msg_t *r, bool ext_poll, bool ext_resp, bool hello) {
	uint32_t now = HAL_GetTick();
	if (!ext_resp) {	// legacy response: no-LoRa flag only, power not reported
		dwmInfo.tag_no_lora = r->tag_no_lora & 1;
		dwmInfo.tag_lora_tx_dbm = LINK_LORA_UNKNOWN;
		dwmInfo.tag_uwb_tx_level = LINK_UWB_UNKNOWN;
		if (ext_poll) {				// an older tag, or an anchor in imposter mode
			link_fallback();
		} else if (hello && (r->tag_no_lora & LINK_CAPABLE)) {	// a new tag
			if (r->tag_no_lora & LINK_CAPABLE_MULTI)
				link_st.coexist_tick = now;
			if (Link_Coexisting())
				return;		// 9K: stay on legacy polls while two stations share the tag
			link_reset();	// extended frames from now on
			link_st.tag_ver = LINK_VER_OF(r->tag_no_lora) ? LINK_VER_OF(r->tag_no_lora) : 1;
			link_st.ext = 1;
			link_st.last_uwb_tick = link_st.last_lora_tick = now;
			link_st.uwb_step_tick = link_st.lora_step_tick = now;
			link_next_seq();
		}
		return;
	}

	bool no_lora = r->tag_no_lora & LINK_RESP_NO_LORA;
	dwmInfo.tag_no_lora = no_lora;
	dwmInfo.tag_lora_tx_dbm = no_lora ? LINK_LORA_UNKNOWN : r->lora_dbm;
	dwmInfo.tag_uwb_tx_level = r->uwb_level;
	link_st.ack_seq = r->ack_seq;
	link_st.echo_lora_dbm = dwmInfo.tag_lora_tx_dbm;
	link_st.echo_uwb_level = r->uwb_level;
	link_st.echo_profile = r->profile_used;
	link_st.echo_pending = (r->tag_no_lora & LINK_RESP_PENDING) != 0;
	link_st.echo_lde_nlos = (r->tag_no_lora & LINK_RESP_LDE_NLOS) != 0;
	link_st.last_uwb_tick = now;
	link_st.ext_miss = 0;
	if (r->tag_no_lora & LINK_RESP_MULTI) {	// 9K: a second station appeared; the tag is at maximum already
		link_st.coexist_tick = now;
		if (!link_st.reverting) {
			link_reset();
			return;
		}
	}

	// UWB: the tag answers at the level this poll asked for. Louder than asked is its floor (boards
	// with a lower maximum have fewer levels); quieter than asked is not obeying.
	if (r->uwb_level < link_st.uwb_level) {
		link_st.uwb_floor = r->uwb_level;
		link_st.uwb_level = r->uwb_level;
		if (link_st.uwb_prev > link_st.uwb_level)
			link_st.uwb_prev = link_st.uwb_level;
		link_st.uwb_mismatch = 0;
	} else if (r->uwb_level > link_st.uwb_level) {
		if (++link_st.uwb_mismatch >= 2) {
			link_st.uwb_level = link_st.uwb_prev = 0;
			link_st.uwb_mismatch = 0;
		}
	} else {
		link_st.uwb_mismatch = 0;
	}

	if (link_st.reverting) {
		if ((no_lora || r->lora_dbm == LINK_LORA_MAX_DBM) && r->uwb_level == 0 && !link_st.echo_lde_nlos)
			link_reset();	// the tag is back on its defaults: legacy polls
		return;
	}
	if (!link_lora_active() || no_lora)
		return;

	int8_t tag = r->lora_dbm;
	switch (link_st.lora_phase) {
	case LINK_LP_IDLE:
		if (tag != link_st.lora_cur)	// the tag fell back on its own, or another station moved it
			link_st.lora_cur = link_st.lora_req = tag;
		break;
	case LINK_LP_REQ:
		if (r->ack_seq == link_st.seq && link_st.echo_pending) {
			link_st.lora_phase = LINK_LP_CONF;
			link_st.lora_tries = 0;
		}
		break;
	case LINK_LP_CONF:
		if (tag == link_st.lora_req && !link_st.echo_pending) {	// proof: the tag runs the new power
			link_st.lora_cur = tag;
			link_st.lora_phase = LINK_LP_IDLE;
			link_st.lora_step_tick = now;
			link_st.lora_good = 0;
		}
		break;
	case LINK_LP_UP:
		if (tag >= link_st.lora_req) {
			link_st.lora_cur = link_st.lora_req = tag;
			link_st.lora_phase = LINK_LP_IDLE;
			link_st.lora_step_tick = now;	// no quieter step right after a louder one
			link_st.lora_good = 0;
		}
		break;
	}
}

/* ---------------- range profiles (#7, main.h LINK_PROFILES) ----------------
 * The host sends the same {seq, profile} to every anchor of a station (opcode 25). The LoRa owner
 * tells the tag (prepare, then commit, over the configuration channel); the tag switches after
 * answering the commit. Every anchor then finds the tag the same way: polls stay on the profile in
 * force until they miss, then alternate between the new and the old profile until a reply decides.
 * No reply at all for PROF_ROTATE_MS -> profile 0, which the tag reaches on its own within 1 s.
 * Two stations, a tag change, silence and FOTA put the anchor on profile 0 at once. */
static const struct { uint8_t br; uint16_t plen, pac, sfdto; uint8_t lora_sf; } prof_tab[LINK_PROFILES] = {
	{ DWT_BR_110K, DWT_PLEN_2048, DWT_PAC64, 2048 + 1 + 64 - 64, 0x0C },	// 0 far (ground): SF12
	{ DWT_BR_850K, DWT_PLEN_1024, DWT_PAC32, 1024 + 1 + 16 - 32, 0x0A },	// 1 mid: SF10
	{ DWT_BR_850K, DWT_PLEN_512,  DWT_PAC16, 512 + 1 + 16 - 16,  0x08 },	// 2 near: SF8
};

static void link_use_profile(uint8_t p) {
	if (p >= LINK_PROFILES)
		p = 0;
	if (p == link_st.prof_rx)
		return;
	config.dataRate = prof_tab[p].br;
	config.txPreambLength = prof_tab[p].plen;
	config.rxPAC = prof_tab[p].pac;
	config.sfdTO = prof_tab[p].sfdto;
	dwt_forcetrxoff();
	dwConfig(&config);		// antenna delays, LDE preset, timeouts: all re-applied
	link_st.prof_rx = p;
}

uint8_t Link_ProfileSF(void) {
	return prof_tab[link_st.prof_cur < LINK_PROFILES ? link_st.prof_cur : 0].lora_sf;
}

static void link_profile_settle(uint8_t p, uint8_t reason) {
	if (p != link_st.prof_cur && reason != PROF_R_HOST)
		link_st.prof_reason = reason;
	link_st.prof_cur = p;
	link_st.prof_phase = PROF_P_IDLE;
	link_use_profile(p);
	Lora_SetBeaconSF(Link_ProfileSF());
}

void Link_ProfileGround(uint8_t reason) {
	bool was = link_st.prof_cur != 0 || link_st.prof_phase != PROF_P_IDLE;
	link_st.prof_target = 0;
	link_profile_settle(0, reason);
	if (was && reason != PROF_R_NONE)
		link_st.prof_reason = reason;
}

// FOTA and other full re-initialisations: profile 0 in state and radio, now.
void dwProfileGround(void) {
	Link_ProfileGround(PROF_R_FOTA);
	link_st.prof_rx = 0xFF;
	link_use_profile(0);
}

// Main loop, between exchanges (dwScanMode): with the tag gone quiet, the radio goes back to
// profile 0 even when the host has stopped polling, so backoff listening runs on profile 0.
void Link_IdleTick(void) {
	if (link_st.prof_rx != 0 && HAL_GetTick() - link_st.last_uwb_tick > LINK_UWB_LOST_MS) {
		Link_ProfileGround(PROF_R_SILENCE);
		link_use_profile(0);
	}
}

static bool link_profile_allowed(void) {
	return (link_cfg.features & LINK_F_PROFILE) && link_st.ext && link_st.tag_ver >= 3 && !Link_Coexisting();
}

bool Link_ProfileRequest(uint8_t seq, uint8_t profile) {
	if (seq == link_st.prof_seq && seq != 0)
		return true;		// a repeat of the request in progress
	link_st.prof_seq = seq;
	if (profile >= LINK_PROFILES)
		return false;
	if (profile == 0) {		// toward the ground state: no negotiation needed
		if (link_cfg.lora_owner && link_st.prof_cur != 0)
			Link_ItemQueue(dwmInfo.tag_dest, LITEM_PROFILE_COMMIT, 0);
		Link_ProfileGround(PROF_R_HOST);
		return true;
	}
	if (!link_profile_allowed()) {
		link_st.prof_reason = PROF_R_UNSUPPORTED;
		return false;
	}
	link_st.prof_target = profile;
	if (profile == link_st.prof_cur)
		return true;
	link_st.prof_t0 = HAL_GetTick();
	if (link_cfg.lora_owner) {
		Link_ItemQueue(dwmInfo.tag_dest, LITEM_PROFILE_PREP, profile);
		link_st.prof_phase = PROF_P_PREP;
	} else {
		link_st.prof_phase = PROF_P_ARMED;
	}
	return true;
}

static void link_profile_rotate(uint8_t target, uint8_t partner) {
	link_st.prof_target = target;
	link_st.prof_prev = partner;
	link_st.prof_phase = PROF_P_ROTATE;
	link_st.prof_rot = 0;	// toggled before each poll: the first one goes out on the new profile
	link_st.prof_t0 = HAL_GetTick();
}

// Before every poll: phase progress, then the configuration this poll goes out on.
static void link_profile_tick(void) {
	uint32_t now = HAL_GetTick();
	if ((link_st.prof_cur != 0 || link_st.prof_phase != PROF_P_IDLE) && Link_Coexisting()) {
		Link_ProfileGround(PROF_R_MULTI);
		return;
	}
	switch (link_st.prof_phase) {
	case PROF_P_PREP:
		if (link_item.item == LITEM_PROFILE_PREP && link_item.state == LITEM_DONE) {
			if (!(link_item.flags & LACK_F_REJECTED) && link_item.readback == link_st.prof_target) {
				Link_ItemQueue(dwmInfo.tag_dest, LITEM_PROFILE_COMMIT, link_st.prof_target);
				link_st.prof_phase = PROF_P_COMMIT;
			} else {
				link_st.prof_phase = PROF_P_IDLE;
				link_st.prof_reason = PROF_R_NO_PROOF;
			}
		} else if (link_item.state == LITEM_FAILED) {
			link_st.prof_phase = PROF_P_IDLE;
			link_st.prof_reason = PROF_R_NO_PROOF;
		}
		break;
	case PROF_P_COMMIT:
		if (link_item.item == LITEM_PROFILE_COMMIT && link_item.state == LITEM_DONE) {
			if (link_item.flags & LACK_F_REJECTED) {
				link_st.prof_phase = PROF_P_IDLE;
				link_st.prof_reason = PROF_R_NO_PROOF;
			} else {
				link_profile_rotate(link_st.prof_target, link_st.prof_cur);	// the tag switches now
			}
		} else if (link_item.state == LITEM_FAILED) {
			link_st.prof_phase = PROF_P_IDLE;
			link_st.prof_reason = PROF_R_NO_PROOF;
		}
		break;
	case PROF_P_ARMED:
		if (now - link_st.prof_t0 > PROF_ARMED_MS) {
			link_st.prof_phase = PROF_P_IDLE;
			link_st.prof_reason = PROF_R_NO_PROOF;
		}
		break;
	case PROF_P_ROTATE:
		if (now - link_st.prof_t0 > PROF_ROTATE_MS) {	// nobody answers on either: ground state
			if (link_cfg.lora_owner)
				Link_ItemQueue(dwmInfo.tag_dest, LITEM_PROFILE_COMMIT, 0);
			Link_ProfileGround(PROF_R_MISSES);
			return;
		}
		link_st.prof_rot ^= 1;
		link_use_profile(link_st.prof_rot ? link_st.prof_target : link_st.prof_prev);
		return;
	default:
		break;
	}
	link_use_profile(link_st.prof_cur);
}

// After every poll that was answered or missed (first exchange of a round).
static void link_profile_after(bool answered) {
	if (link_st.prof_phase == PROF_P_ROTATE) {
		if (answered) {
			uint8_t p = link_st.prof_rx;
			link_profile_settle(p, p == link_st.prof_target ? PROF_R_HOST : PROF_R_NO_PROOF);
			if (p != link_st.prof_target && !link_cfg.lora_owner) {	// the tag has not switched yet
				link_st.prof_phase = PROF_P_ARMED;
			}
		}
		return;
	}
	if (answered)
		return;
	// missed: an armed anchor, or a switched one that lost the tag, starts looking on both
	if (link_st.uwb_miss >= 2) {
		if (link_st.prof_phase == PROF_P_ARMED)
			link_profile_rotate(link_st.prof_target, link_st.prof_cur);
		else if (link_st.prof_cur != 0)
			link_profile_rotate(link_st.prof_cur, link_st.prof_prev);
	}
}

/* ---------------- configuration items (opcode 24, main.h LCMD_* / LITEM_*) ----------------
 * One item for one tag at a time. LoRa first: after a beacon on a slot the tag listens to, the
 * command goes out with inverted IQ and the tag acks right away. Once LoRa has not answered for
 * LITEM_UWB_AFTER_MS the item also rides the extended UWB poll v2 (tag3.2.1+), whose answer comes
 * after the tag has applied it ("applying" first, the read-back on a later poll). */
LinkItem_t link_item = { .state = LITEM_IDLE };

bool Link_ItemQueue(uint16_t tag, uint8_t item, uint16_t value) {
	if (item == LITEM_NONE)
		return false;
	link_item.tag = tag & 0x7FFF;
	link_item.item = item;
	link_item.value = value;
	if (++link_item.seq == 0)
		link_item.seq = 1;
	link_item.readback = 0;
	link_item.flags = 0;
	link_item.via = 0;
	link_item.lora_tries = 0;
	link_item.start_tick = HAL_GetTick();
	link_item.state = LITEM_PENDING;
	return true;
}

static bool link_item_active(uint16_t tag) {
	if (link_item.state != LITEM_PENDING || link_item.tag != tag)
		return false;
	if (HAL_GetTick() - link_item.start_tick > LITEM_GIVE_UP_MS) {
		link_item.state = LITEM_FAILED;
		return false;
	}
	return true;
}

// Slots the tag listens to: multiples of its period. Unknown or stale -> the ground period, whose
// slots every period lands on, so no command is sent into a window that is not open.
bool Link_ItemLoraDue(uint8_t beacon_index) {
	if (!link_item_active(lora_rxInfo.tag_id))
		return false;
	uint8_t n = link_item.tag_listen_n;
	if (n == 0 || HAL_GetTick() - link_item.listen_tick > 10000)
		n = LCMD_LISTEN_GROUND;
	return (beacon_index % n) == 0;
}

void Link_ItemFillCmd(sLoRaCmd *c) {
	c->magic = LCMD_MAGIC;
	c->tag_id = link_item.tag;
	c->src = tx_poll_msg.src;
	c->seq = link_item.seq;
	c->item = link_item.item;
	c->value = link_item.value;
	c->spare = 0;
	c->checksum = checksum((uint8_t*)c, sizeof(*c));
	link_item.lora_tries++;
}

static void link_item_done(uint16_t readback, uint8_t flags, uint8_t via) {
	link_item.readback = readback;
	link_item.flags = flags;
	link_item.via = via;
	link_item.state = LITEM_DONE;
}

void Link_ItemOnLoraAck(const sLoRaAck *a) {
	if (a->tag_id == link_item.tag) {
		link_item.tag_listen_n = a->listen_n;
		link_item.listen_tick = HAL_GetTick();
	}
	if (link_item.state == LITEM_PENDING && a->tag_id == link_item.tag && a->seq == link_item.seq
			&& a->item == link_item.item)
		link_item_done(a->value, a->flags, LITEM_VIA_LORA);
}

bool Link_ItemUwbDue(void) {
	return link_st.tag_ver >= 2 && link_item_active(dwmInfo.tag_dest)
			&& HAL_GetTick() - link_item.start_tick > LITEM_UWB_AFTER_MS;
}

void Link_ItemFillPoll(twr_anchor_ext2_msg_t *p) {
	p->item_seq = link_item.seq;
	p->item = link_item.item;
	p->item_value = link_item.value;
}

void Link_ItemOnUwb(const twr_tag_ext2_msg_t *r) {
	if (link_item.state == LITEM_PENDING && r->item == link_item.item && r->item_seq == link_item.seq
			&& (!(r->item_flags & LACK_F_APPLYING) || r->item == LITEM_PROFILE_COMMIT))
		link_item_done(r->item == LITEM_PROFILE_COMMIT ? link_item.value : r->item_value,
				r->item_flags & ~LACK_F_APPLYING, LITEM_VIA_UWB);	// a commit is applied after this answer
}

// rx_power_dbm and first-path power, DW1000 UM 4.7.1/4.7.2 (PRF 64 MHz); N without the SFD
// correction (< 0.4 dB at 2048 preamble symbols)
static float link_rx_power_dbm(void) {
	float n = dwmInfo.diagnostics.rxPreamCount;
	if (n == 0 || dwmInfo.diagnostics.cirPwr == 0)
		return 0;
	return 10.0f * log10f(((float)dwmInfo.diagnostics.cirPwr * 131072.0f) / (n * n)) - PRF_64Mhz;
}

static float link_fp_power_dbm(void) {
	float n = dwmInfo.diagnostics.rxPreamCount;
	float f1 = dwmInfo.diagnostics.firstPathAmp1, f2 = dwmInfo.diagnostics.firstPathAmp2,
			f3 = dwmInfo.diagnostics.firstPathAmp3;
	float e = f1 * f1 + f2 * f2 + f3 * f3;
	if (n == 0 || e == 0)
		return 0;
	return 10.0f * log10f(e / (n * n)) - PRF_64Mhz;
}

// 9G: LOS / NLOS LDE preset from RX-FP, dead band 2 dB, LINK_LDE_HOLD exchanges to flip. A weak
// signal counts as LOS: there RX-FP measures noise, and the more sensitive NLOS threshold would
// trigger on it.
static void link_lde_update(float rxfp, float rx) {
	if (!(link_cfg.features & LINK_F_LDE_AUTO))
		return;
	if (link_cfg.lde_station <= 1) {	// the host sets the station's preset; the tag follows it
		if (link_st.lde_nlos != link_cfg.lde_station) {
			link_st.lde_nlos = link_cfg.lde_station;
			link_st.lde_count = 0;
			dwt_write8bitoffsetreg(LDE_IF_ID, LDE_CFG1_OFFSET, dwLdeCfg1());
			dwt_rxreset();
		}
		return;
	}
	bool nlos = (rx > LINK_STRONG_DBM) && (rxfp > LINK_RXFP_SOFT_DB + 1.0f);
	bool los = (rx <= LINK_STRONG_DBM) || (rxfp < LINK_RXFP_SOFT_DB - 1.0f);
	bool flip = link_st.lde_nlos ? los : nlos;
	if (!flip) {
		link_st.lde_count = 0;
		return;
	}
	if (++link_st.lde_count < LINK_LDE_HOLD)
		return;
	link_st.lde_count = 0;
	link_st.lde_nlos = !link_st.lde_nlos;
	dwt_write8bitoffsetreg(LDE_IF_ID, LDE_CFG1_OFFSET, dwLdeCfg1());
	dwt_rxreset();	// LDE change takes effect from a fresh RX
}

uint8_t dwLdeCfg1(void) {
	return link_st.lde_nlos ? LINK_LDE_NLOS : lde_cfg1;
}

// After each exchange: UWB loop, 9G and miss handling.
void Link_OnExchange(uint8_t result, bool first, bool ext_poll, bool ext_resp) {
	uint32_t now = HAL_GetTick();
	if (result == EX_VALID) {
		float rx = link_rx_power_dbm();
		float fp = link_fp_power_dbm();
		link_st.last_rxfp_db = rx - fp;
		link_st.last_rx_dbm = rx;
		link_lde_update(link_st.last_rxfp_db, rx);
	}
	if (!ext_poll)
		return;

	if (result == EX_NONE) {
		if (!first)		// a repeat that goes unanswered (tag still re-arming) proves nothing
			return;
		if (link_st.prof_phase == PROF_P_ROTATE) {	// half of a rotation misses by design: its own deadline decides
			link_profile_after(false);
			return;
		}
		// extended polls go unanswered: legacy frames. Only the LoRa owner falls back quickly; for
		// the others a miss costs their own exchange, not the station's link state (issue #5). A
		// bench level pin rides out a weak level.
		uint8_t limit = (link_cfg.uwb_level_pin < LINK_UWB_LEVELS) ? 20
				: (link_cfg.lora_owner ? LINK_MISS_OWNER : LINK_MISS_OTHER);
		if (++link_st.ext_miss >= limit) {
			link_fallback();
			return;
		}
		{
			link_st.uwb_good = 0;
			if (++link_st.uwb_miss >= 2) {
				link_st.uwb_level = link_st.uwb_prev = 0;
				if (link_cfg.uwb_level_station < LINK_UWB_LEVELS) {	// the station level waits too
					link_st.station_hold = 1;
					link_st.station_hold_tick = HAL_GetTick();
				}
			}
			else if (link_st.uwb_prev < link_st.uwb_level)
				link_st.uwb_level = link_st.uwb_prev;	// undo the last quieter step
		}
		link_profile_after(false);
		return;
	}
	link_st.uwb_miss = 0;
	if (first)
		link_profile_after(true);
	if (result != EX_VALID || !ext_resp || link_st.reverting || !(link_cfg.features & LINK_F_POWER))
		return;
	if (link_st.echo_uwb_level != link_st.uwb_level)
		return;

	if (link_cfg.uwb_level_pin < LINK_UWB_LEVELS || link_cfg.uwb_level_station < LINK_UWB_LEVELS)
		return;		// pinned for a bench sweep, or the host sets the station's level
	float rx = link_rx_power_dbm();
	int target = link_cfg.uwb_target_dbm, band = link_cfg.uwb_band_db;
	if (rx < target - band) {	// louder at once
		if (link_st.uwb_level > 0) {
			link_st.uwb_prev = link_st.uwb_level;
			link_st.uwb_level--;
		}
		link_st.uwb_good = 0;
	} else if (rx > target + band && link_st.uwb_good >= 2 && now - link_st.uwb_step_tick >= link_step_ms()
			&& link_st.uwb_level < link_st.uwb_floor && rx - 3.0f >= LINK_UWB_SENS_DBM + 10) {
		link_st.uwb_prev = link_st.uwb_level;
		link_st.uwb_level++;
		link_st.uwb_step_tick = now;
		link_st.uwb_good = 0;
	} else if (link_st.uwb_good < 255) {
		link_st.uwb_good++;
	}
}

// Each valid LoRa packet from the tag being ranged (owner anchor runs the LoRa loop).
void Link_OnLoraPacket(int8_t signal_rssi_dbm, int8_t snr_db) {
	uint32_t now = HAL_GetTick();
	link_st.last_lora_tick = now;
	if (!link_lora_active() || link_st.echo_lora_dbm == LINK_LORA_UNKNOWN)
		return;
	if (link_st.lora_phase == LINK_LP_IDLE && link_st.lora_good < 255)
		link_st.lora_good++;

	int target = link_cfg.lora_target_dbm, band = link_cfg.lora_band_db;
	if (snr_db < LINK_LORA_MIN_SNR || signal_rssi_dbm < target - 3 * band) {	// in trouble: maximum at once
		if (link_st.lora_cur < LINK_LORA_MAX_DBM || link_st.lora_phase != LINK_LP_IDLE)
			if (link_st.lora_phase != LINK_LP_UP || link_st.lora_req < LINK_LORA_MAX_DBM)
				link_lora_up(LINK_LORA_MAX_DBM);
	} else if (signal_rssi_dbm < target - band) {	// louder at once
		if (link_st.lora_phase != LINK_LP_UP
				&& (link_st.lora_cur < LINK_LORA_MAX_DBM || link_st.lora_phase != LINK_LP_IDLE))
			link_lora_up(link_st.lora_cur + LINK_LORA_STEP_DB);	// also cancels a pending quieter step
	} else if (signal_rssi_dbm > target + band && link_st.lora_phase == LINK_LP_IDLE
			&& link_st.lora_good >= 2 && now - link_st.lora_step_tick >= link_step_ms()
			&& link_st.lora_cur - LINK_LORA_STEP_DB >= LINK_LORA_MIN_DBM) {
		link_next_seq();
		link_st.lora_req = link_st.lora_cur - LINK_LORA_STEP_DB;
		link_st.lora_phase = LINK_LP_REQ;
		link_st.lora_tries = 0;
	}
}

// Opcode 22: bytes in LinkCfg_t order; a shorter payload changes only the leading fields.
void Link_SetConfig(const uint8_t *cfg, uint8_t len) {
	LinkCfg_t c = link_cfg;
	if (len > sizeof(c))
		len = sizeof(c);
	memcpy(&c, cfg, len);
	if (c.k_max > LINK_K_MAX)
		c.k_max = LINK_K_MAX;
	if (c.lora_band_db == 0)
		c.lora_band_db = 1;
	if (c.uwb_band_db == 0)
		c.uwb_band_db = 1;

	uint8_t was = link_cfg.features;
	link_cfg = c;
	if (link_st.ext && !link_wants_ext()) {
		// off means revert: ask for maximum and LOS until the tag shows it, then legacy polls
		link_st.reverting = 1;
		link_st.revert_tries = 0;
		link_st.uwb_level = link_st.uwb_prev = 0;
		link_next_seq();
	} else if ((was & LINK_F_POWER) && !(c.features & LINK_F_POWER)) {
		// 9G stays on, frames stay extended: UWB back to maximum now; LoRa requests stop, so the
		// tag returns to maximum by itself within LINK_SILENCE_MS
		link_st.uwb_level = link_st.uwb_prev = 0;
		link_st.lora_phase = LINK_LP_IDLE;
		link_st.lora_cur = link_st.lora_req = LINK_LORA_MAX_DBM;
	}
	if (!c.lora_owner && link_st.lora_phase != LINK_LP_IDLE) {
		link_st.lora_phase = LINK_LP_IDLE;	// the tag falls back by itself without owner polls
		link_st.lora_req = link_st.lora_cur;
	}
	if ((was & LINK_F_LDE_AUTO) && !(c.features & LINK_F_LDE_AUTO) && link_st.lde_nlos) {
		link_st.lde_nlos = 0;
		dwt_write8bitoffsetreg(LDE_IF_ID, LDE_CFG1_OFFSET, lde_cfg1);
		dwt_rxreset();
	}
	link_st.lde_count = 0;
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
		if (rx_buffer.des == tx_resp_msg.src) {	// src is the 15-bit id (dwSetFrameIds)
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
					+ tx_ant_dly;	// the delay applied by dwApplyAntennaDelays()

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
