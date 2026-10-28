/*
 * fota.c
 *
 * FOTA over UWB, flasher and target sides. Shared, identical copy in Anchor
 * and Tag; board specifics live in fota_port.cpp. Protocol in fota.h.
 */

#include "fota.h"
#include "main.h"
#include <string.h>
#include "decadriver/deca_device_api.h"
#include "decadriver/deca_regs.h"

#define FOTA_TARGET_IDLE_MS		6000	// target gives up a session after this much silence
#define FOTA_COMMIT_LINGER_MS	300		// target re-acks duplicate COMMITs before writing flash
#define FOTA_HELLO_WAIT_MS		60		// HELLO_ACK window (normal 110k config)
#define FOTA_HELLO_TIMEOUT_MS	10000
#define FOTA_QUERY_TIMEOUT_MS	3000
#define FOTA_VERIFY_DELAY_MS	5000	// target erases, writes, verifies and reboots
#define FOTA_VERIFY_TIMEOUT_MS	20000
#define FOTA_LINK_TIMEOUT_MS	3000	// no DATA_ACK for this long -> fall back to HELLO
#define FOTA_SESSION_TRIES		4		// 6.8M, 850k, 110k, 110k
#define FOTA_COMMIT_TRIES		10
#define FOTA_FAST_BLINK_MS		50
#define FOTA_SLOW_BLINK_MS		500
#define FOTA_DISCOVER_ROUNDS	3
#define FOTA_DISCOVER_SLOTS		16		// a target answers in one random slot
#define FOTA_DISCOVER_SLOT_MS	20
#define FOTA_DISCOVER_WINDOW_MS	(FOTA_DISCOVER_SLOTS * FOTA_DISCOVER_SLOT_MS + 80)

/* Data-phase UWB settings. Channel, PRF and preamble code match the normal
 * ranging config; extended PHR allows frames up to 1023 bytes. */
static dwt_config_t rate_cfg[FOTA_RATE_NUM] = {
	{2, DWT_PRF_64M, DWT_PLEN_2048, DWT_PAC64, 9, 9, 1, DWT_BR_110K, DWT_PHRMODE_EXT, (2048 + 1 + 64 - 64)},
	{2, DWT_PRF_64M, DWT_PLEN_256,  DWT_PAC16, 9, 9, 1, DWT_BR_850K, DWT_PHRMODE_EXT, (256 + 1 + 16 - 16)},
	{2, DWT_PRF_64M, DWT_PLEN_128,  DWT_PAC8,  9, 9, 0, DWT_BR_6M8,  DWT_PHRMODE_EXT, (128 + 1 + 8 - 8)},
};
static const uint16_t rate_chunk[FOTA_RATE_NUM] = {256, 512, 512};
static const uint16_t rate_ack_ms[FOTA_RATE_NUM] = {120, 30, 15};

static uint8_t img[FOTA_MAX_IMAGE + 16] __attribute__((aligned(16)));
static uint8_t txf[sizeof(fota_hdr_t) + sizeof(fota_data_t)];
static uint8_t rxf[1024];
static uint8_t seq;

enum { T_IDLE = 0, T_ARMED };

static struct {
	uint8_t state;
	uint8_t rate;
	uint16_t session;
	uint16_t peer;
	uint32_t size;
	uint32_t crc;
	uint32_t next;
	uint32_t armed_at;
	uint32_t t_led;
	uint8_t led_on;
} tg;

static struct {
	uint8_t mode;
	uint8_t state;
	uint8_t sessions;		// HELLO/data attempts in this run
	uint8_t last_err;
	uint8_t rate;
	uint8_t kind;
	uint8_t fast;			// data-phase config applied
	uint8_t t_valid;
	uint16_t target;
	uint16_t query_target;
	uint16_t session;
	uint16_t countdown_ms;
	uint32_t size;
	uint32_t crc;
	uint32_t loaded;
	uint32_t acked;
	uint32_t retries;
	uint32_t fails;			// consecutive lost frames
	uint32_t tries;
	uint32_t t_start;
	uint32_t t_state;
	uint32_t t_ack;
	uint32_t t_led;
	uint8_t led_on;
	fota_hello_ack_t tinfo;
	uint8_t ndisc;
	uint8_t disc_round;
	uint8_t disc_prev_state;
	fota_disc_entry_t disc[FOTA_DISCOVER_MAX];
} fl;

static uint32_t rnd_state;

static uint32_t rnd(void)
{
	rnd_state = rnd_state * 1664525u + 1013904223u + HAL_GetTick() + fota_port_self_addr();
	return rnd_state >> 16;
}

static int battery_ok(uint8_t b)
{
	return b == 0xFF || b >= FOTA_MIN_BATTERY;
}

/* ---------------- helpers ---------------- */

static uint8_t sum8(const uint8_t *p, uint32_t n)
{
	uint8_t s = 0;
	while (n--)
		s += *p++;
	return s;
}

/* Same CRC as the bootloader: STM32 CRC-32 (poly 0x04C11DB7, init 0xFFFFFFFF,
 * no reflection), byte-fed, over the image padded with 0xFF to 255 pages. */
static uint32_t crc_image(const uint8_t *p, uint32_t size)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
	(void)RCC->AHB1ENR;
	CRC->INIT = 0xFFFFFFFFu;
	CRC->POL = 0x04C11DB7u;
	CRC->CR = CRC_CR_RESET;		// 32-bit polynomial, no input/output reversal
	volatile uint8_t *dr8 = (volatile uint8_t *)&CRC->DR;
	for (uint32_t i = 0; i < size; i++)
		*dr8 = p[i];
	uint32_t pad = FOTA_CRC_LEN - size;
	for (; pad & 3u; pad--)
		*dr8 = 0xFF;
	for (pad >>= 2; pad; pad--)
		CRC->DR = 0xFFFFFFFFu;
	return CRC->DR;
}

static int image_vectors_ok(const uint8_t *p)
{
	uint32_t sp, pc;
	memcpy(&sp, p, 4);
	memcpy(&pc, p + 4, 4);
	return sp >= 0x20000000u && sp <= 0x20270000u
		&& pc >= FOTA_APP_ADDRESS && pc < FOTA_HEADER_ADDRESS && (pc & 1u);
}

static uint8_t *frame_begin(uint16_t des, uint8_t type, uint16_t session)
{
	fota_hdr_t *h = (fota_hdr_t *)txf;
	h->flag = FOTA_FLAG;
	h->fc = 0x88;
	h->seq = seq++;
	h->pan_lo = 0xCA;
	h->pan_hi = 0xDE;
	h->src = fota_port_self_addr();
	h->des = des;
	h->type = type;
	h->session = session;
	return txf + sizeof(fota_hdr_t);
}

/* len excludes the 2-byte FCS the DW1000 appends */
static void uwb_send(uint16_t len, int expect_response)
{
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX);
	dwt_writetxdata(len + 2, txf, 0);
	dwt_writetxfctrl(len + 2, 0, 0);
	dwt_starttx(DWT_START_TX_IMMEDIATE | (expect_response ? DWT_RESPONSE_EXPECTED : 0));
	if (!expect_response) {
		uint32_t t0 = HAL_GetTick();
		while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS) && HAL_GetTick() - t0 < 50) {
		}
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_TX);
	}
}

/* Wait for a frame (receiver already enabled). Returns payload length without
 * FCS, 0 on timeout, -1 on an RX error. */
static int uwb_wait_rx(uint32_t timeout_ms)
{
	uint32_t t0 = HAL_GetTick();
	uint32_t st;
	while (!((st = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {
		if (HAL_GetTick() - t0 > timeout_ms) {
			dwt_forcetrxoff();
			return 0;
		}
	}
	if (st & SYS_STATUS_RXFCG) {
		uint32_t len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
		dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD);
		if (len < 2 + sizeof(fota_hdr_t) || len - 2 > sizeof(rxf))
			return -1;
		dwt_readrxdata(rxf, len - 2, 0);
		return len - 2;
	}
	dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_GOOD);
	dwt_rxreset();
	return -1;
}

/* Frame in rxf addressed to us, of the given type and session, with at least
 * min_payload bytes of payload. */
static const uint8_t *rx_payload(int n, uint8_t type, uint16_t session, uint32_t min_payload)
{
	const fota_hdr_t *h = (const fota_hdr_t *)rxf;
	if (n < (int)(sizeof(fota_hdr_t) + min_payload))
		return 0;
	if (h->flag != FOTA_FLAG || h->des != fota_port_self_addr() || h->type != type || h->session != session)
		return 0;
	return rxf + sizeof(fota_hdr_t);
}

static void uwb_fast(uint8_t rate)
{
	dwt_forcetrxoff();
	dwt_configure(&rate_cfg[rate]);
	dwt_setrxtimeout(0);
	dwt_setrxaftertxdelay(0);
}

/* Normal ranging config, but the receiver waits for software timeouts. */
static void uwb_normal_flasher(void)
{
	fota_port_uwb_restore();
	dwt_setrxtimeout(0);
	dwt_setrxaftertxdelay(0);
}

/* ---------------- write bank 2 from RAM ---------------- */

/* Runs from RAM with interrupts off: erases bank 2, writes the image and the
 * bootloader header, reads everything back and retries up to 3 times, then
 * resets. Must not call anything that lives in flash. */
__attribute__((section(".RamFunc"), noinline, long_call, noreturn))
static void ram_flash_and_reset(const uint32_t *src, uint32_t nwords, uint32_t crc)
{
	__disable_irq();
	MPU->CTRL = 0;		// the app maps bank 2 read-only
	__DSB();
	__ISB();
	ICACHE->CR &= ~ICACHE_CR_EN;

	for (uint32_t attempt = 0; attempt < 3; attempt++) {
		while (FLASH->NSSR & FLASH_NSSR_BSY) {
		}
		if (FLASH->NSCR & FLASH_NSCR_LOCK) {
			FLASH->NSKEYR = 0x45670123u;
			FLASH->NSKEYR = 0xCDEF89ABu;
		}
		FLASH->NSSR = FLASH_NSSR_EOP | FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR
				| FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR;

		FLASH->NSCR = FLASH_NSCR_MER2;
		FLASH->NSCR = FLASH_NSCR_MER2 | FLASH_NSCR_STRT;
		while (FLASH->NSSR & FLASH_NSSR_BSY) {
		}

		FLASH->NSCR = FLASH_NSCR_PG;
		volatile uint32_t *dst = (volatile uint32_t *)FOTA_APP_ADDRESS;
		for (uint32_t i = 0; i < nwords; i += 4) {
			dst[i] = src[i];
			dst[i + 1] = src[i + 1];
			dst[i + 2] = src[i + 2];
			dst[i + 3] = src[i + 3];
			while (FLASH->NSSR & (FLASH_NSSR_BSY | FLASH_NSSR_WDW)) {
			}
		}
		volatile uint32_t *hdr = (volatile uint32_t *)FOTA_HEADER_ADDRESS;
		hdr[0] = crc;
		hdr[1] = FOTA_CRC_LEN;
		hdr[2] = 0xFFFFFFFFu;
		hdr[3] = 0xFFFFFFFFu;
		while (FLASH->NSSR & (FLASH_NSSR_BSY | FLASH_NSSR_WDW)) {
		}
		FLASH->NSCR = 0;

		uint32_t ok = (hdr[0] == crc) && (hdr[1] == FOTA_CRC_LEN);
		for (uint32_t i = 0; ok && i < nwords; i++)
			if (dst[i] != src[i])
				ok = 0;
		if (ok)
			break;
	}

	FLASH->NSCR = FLASH_NSCR_LOCK;
	__DSB();
	SCB->AIRCR = (0x5FAUL << SCB_AIRCR_VECTKEY_Pos) | (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) | SCB_AIRCR_SYSRESETREQ_Msk;
	__DSB();
	for (;;) {
	}
}

/* ---------------- target ---------------- */

static void target_reply_info(uint8_t type, uint16_t des, uint16_t session, uint8_t status, uint16_t countdown);

static void target_reply_hello(uint16_t des, uint16_t session, uint8_t status, uint16_t countdown)
{
	target_reply_info(FOTA_HELLO_ACK, des, session, status, countdown);
}

static void target_reply_info(uint8_t type, uint16_t des, uint16_t session, uint8_t status, uint16_t countdown)
{
	fota_hello_ack_t *a = (fota_hello_ack_t *)frame_begin(des, type, session);
	memset(a, 0, sizeof(*a));
	a->status = status;
	a->countdown_ms = countdown;
	a->chunk = rate_chunk[tg.rate];
	a->kind = fota_port_kind();
	a->hw = fota_port_hw();
	a->battery = fota_port_battery();
	strncpy(a->sw, VERSION, sizeof(a->sw));
	a->app_crc = *(volatile uint32_t *)FOTA_HEADER_ADDRESS;
	uwb_send(sizeof(fota_hdr_t) + sizeof(*a), 0);
	fota_port_target_answered();
}

/* Called from the normal UWB receive loop for a frame starting with FOTA_FLAG. */
void fota_uwb_rx(uint32_t frame_len)
{
	if (fl.mode || frame_len < 2 + sizeof(fota_hdr_t) || frame_len - 2 > sizeof(rxf))
		return;
	dwt_readrxdata(rxf, frame_len - 2, 0);
	const fota_hdr_t *h = (const fota_hdr_t *)rxf;
	if (h->type == FOTA_DISCOVER && h->des == FOTA_BROADCAST) {
		// answer in a random slot so several targets don't collide
		uint16_t src = h->src, session = h->session;
		uint32_t t0 = HAL_GetTick(), wait = (rnd() % FOTA_DISCOVER_SLOTS) * FOTA_DISCOVER_SLOT_MS;
		while (HAL_GetTick() - t0 < wait)
			fota_port_kick_wd();
		target_reply_info(FOTA_DISCOVER_ACK, src, session, FOTA_OK, 0);
		return;
	}
	if (h->des != fota_port_self_addr() || h->type != FOTA_HELLO
			|| frame_len - 2 < sizeof(fota_hdr_t) + sizeof(fota_hello_t))
		return;
	fota_hello_t hello;
	memcpy(&hello, rxf + sizeof(fota_hdr_t), sizeof(hello));
	uint16_t src = h->src;
	uint16_t session = h->session;
	uint32_t now = HAL_GetTick();

	if (hello.flags & FOTA_HELLO_QUERY) {
		target_reply_hello(src, session, FOTA_OK, 0);
		return;
	}
	uint8_t status = FOTA_OK;
	if (hello.kind != fota_port_kind())
		status = FOTA_ERR_KIND;
	else if (hello.size == 0 || hello.size > FOTA_MAX_IMAGE)
		status = FOTA_ERR_SIZE;
	else if (hello.rate >= FOTA_RATE_NUM)
		status = FOTA_ERR_RATE;
	else if (!battery_ok(fota_port_battery()))
		status = FOTA_ERR_BATTERY;
	else if (tg.state == T_ARMED && (session != tg.session || src != tg.peer))
		status = FOTA_ERR_BUSY;
	if (status != FOTA_OK) {
		target_reply_hello(src, session, status, 0);
		return;
	}
	if (tg.state != T_ARMED) {
		tg.state = T_ARMED;
		tg.session = session;
		tg.peer = src;
		tg.size = hello.size;
		tg.crc = hello.crc;
		tg.rate = hello.rate;
		tg.next = 0;
		tg.armed_at = now;
	}
	uint32_t left = FOTA_COUNTDOWN_MS - (now - tg.armed_at);
	target_reply_hello(src, session, FOTA_OK, (uint16_t)left);
}

static void target_session(void)
{
	uwb_fast(tg.rate);
	uint32_t now = HAL_GetTick();
	uint32_t last_rx = now, last_data = 0, t_led = now, t_slow = now, commit_at = 0;
	int commit_ok = 0, status_on = 0, link_on = 0;

	for (;;) {
		now = HAL_GetTick();
		fota_port_kick_wd();
		if (now - t_slow >= FOTA_SLOW_BLINK_MS) {
			t_slow = now;
			status_on = !status_on;
			fota_port_status_led(status_on);
		}
		if (now - t_led >= FOTA_FAST_BLINK_MS) {
			t_led = now;
			link_on = (now - last_data < 200) ? !link_on : 0;
			fota_port_link_led(link_on);
		}
		if (commit_ok && now - commit_at > FOTA_COMMIT_LINGER_MS)
			break;
		if (now - last_rx > FOTA_TARGET_IDLE_MS)
			break;

		dwt_rxenable(DWT_START_RX_IMMEDIATE);
		int n = uwb_wait_rx(50);
		if (n <= 0)
			continue;
		const fota_hdr_t *h = (const fota_hdr_t *)rxf;
		if (h->flag != FOTA_FLAG || h->des != fota_port_self_addr() || h->src != tg.peer || h->session != tg.session)
			continue;
		last_rx = now;

		if (h->type == FOTA_DATA) {
			const fota_data_t *d = (const fota_data_t *)rx_payload(n, FOTA_DATA, tg.session, 6);
			if (!d || d->len > rate_chunk[tg.rate] || n < (int)(sizeof(fota_hdr_t) + 6 + d->len))
				continue;
			if (d->offset == tg.next && d->offset + d->len <= tg.size) {
				memcpy(img + d->offset, d->data, d->len);
				tg.next += d->len;
			}
			last_data = now;
			fota_data_ack_t *a = (fota_data_ack_t *)frame_begin(tg.peer, FOTA_DATA_ACK, tg.session);
			a->next = tg.next;
			a->status = FOTA_OK;
			uwb_send(sizeof(fota_hdr_t) + sizeof(*a), 0);
		} else if (h->type == FOTA_COMMIT) {
			const fota_commit_t *c = (const fota_commit_t *)rx_payload(n, FOTA_COMMIT, tg.session, sizeof(fota_commit_t));
			if (!c)
				continue;
			uint8_t status = FOTA_OK;
			uint32_t crc = 0;
			if (tg.next != tg.size || c->size != tg.size || c->crc != tg.crc) {
				status = FOTA_ERR_STATE;
			} else {
				crc = crc_image(img, tg.size);
				if (crc != tg.crc)
					status = FOTA_ERR_CRC;
				else if (!image_vectors_ok(img))
					status = FOTA_ERR_IMAGE;
			}
			if (status == FOTA_OK && !commit_ok) {
				commit_ok = 1;
				commit_at = now;
			}
			fota_commit_ack_t *a = (fota_commit_ack_t *)frame_begin(tg.peer, FOTA_COMMIT_ACK, tg.session);
			a->status = status;
			a->crc = crc;
			uwb_send(sizeof(fota_hdr_t) + sizeof(*a), 0);
		} else if (h->type == FOTA_ABORT) {
			break;
		}
	}

	if (commit_ok) {
		uint32_t padded = (tg.size + 15u) & ~15u;
		memset(img + tg.size, 0xFF, padded - tg.size);
		fota_port_status_led(1);
		fota_port_link_led(1);
		ram_flash_and_reset((const uint32_t *)img, padded / 4u, tg.crc);
	}
	tg.state = T_IDLE;
	fota_port_status_led(0);
	fota_port_link_led(0);
	fota_port_uwb_restore();
}

/* ---------------- flasher ---------------- */

static void flasher_set(uint8_t state)
{
	fl.state = state;
	fl.t_state = HAL_GetTick();
	fl.tries = 0;
	fl.fails = 0;
}

static void flasher_fail(uint8_t err)
{
	if (fl.fast) {
		uwb_normal_flasher();
		fl.fast = 0;
	}
	fl.last_err = err;
	flasher_set(FL_FAILED);
}

static void flasher_hello(uint16_t target, uint8_t flags)
{
	fota_hello_t *p = (fota_hello_t *)frame_begin(target, FOTA_HELLO, fl.session);
	p->size = fl.size;
	p->crc = fl.crc;
	p->kind = fl.kind;
	p->rate = fl.rate;
	p->flags = flags;
	uwb_send(sizeof(fota_hdr_t) + sizeof(*p), 1);
	int n = uwb_wait_rx(FOTA_HELLO_WAIT_MS);
	const fota_hello_ack_t *a = (const fota_hello_ack_t *)rx_payload(n, FOTA_HELLO_ACK, fl.session, sizeof(fota_hello_ack_t));
	if (a && ((const fota_hdr_t *)rxf)->src == target) {
		memcpy(&fl.tinfo, a, sizeof(fl.tinfo));
		fl.t_valid = 1;
		fl.t_ack = HAL_GetTick();
	}
}

static void flasher_step(void)
{
	uint32_t now = HAL_GetTick();
	uint32_t in_state = now - fl.t_state;

	switch (fl.state) {
	case FL_HELLO:
		fl.t_valid = 0;
		flasher_hello(fl.target, 0);
		if (fl.t_valid) {
			if (fl.tinfo.status != FOTA_OK) {
				flasher_fail(fl.tinfo.status);
			} else {
				fl.countdown_ms = fl.tinfo.countdown_ms;
				flasher_set(FL_COUNTDOWN);
			}
		} else if (in_state > FOTA_HELLO_TIMEOUT_MS) {
			flasher_fail(FOTA_ERR_NO_TARGET);
		}
		break;

	case FL_COUNTDOWN:
		if (in_state >= (uint32_t)fl.countdown_ms + 300u) {
			uwb_fast(fl.rate);
			fl.fast = 1;
			fl.acked = 0;
			fl.t_ack = now;
			flasher_set(FL_DATA);
		}
		break;

	case FL_DATA: {
		uint32_t len = fl.size - fl.acked;
		if (len > rate_chunk[fl.rate])
			len = rate_chunk[fl.rate];
		fota_data_t *d = (fota_data_t *)frame_begin(fl.target, FOTA_DATA, fl.session);
		d->offset = fl.acked;
		d->len = (uint16_t)len;
		memcpy(d->data, img + fl.acked, len);
		uwb_send(sizeof(fota_hdr_t) + 6 + len, 1);
		int n = uwb_wait_rx(rate_ack_ms[fl.rate]);
		const fota_data_ack_t *a = (const fota_data_ack_t *)rx_payload(n, FOTA_DATA_ACK, fl.session, sizeof(fota_data_ack_t));
		if (a && a->next <= fl.size) {
			fl.acked = a->next;		// cumulative: also rewinds if the target missed a chunk
			fl.fails = 0;
			fl.t_ack = now;
			if (fl.acked == fl.size)
				flasher_set(FL_COMMIT);
		} else {
			fl.retries++;
			fl.fails++;
			if (now - fl.t_ack > FOTA_LINK_TIMEOUT_MS) {
				// lost the target: back to the normal config, it times out and listens again
				uwb_normal_flasher();
				fl.fast = 0;
				if (++fl.sessions >= FOTA_SESSION_TRIES) {
					flasher_fail(FOTA_ERR_LINK);
				} else {
					if (fl.rate > FOTA_RATE_110K)
						fl.rate--;		// slower and longer range; the next HELLO tells the target
					fl.session++;
					flasher_set(FL_HELLO);
				}
			}
		}
		break;
	}

	case FL_COMMIT: {
		fota_commit_t *c = (fota_commit_t *)frame_begin(fl.target, FOTA_COMMIT, fl.session);
		c->size = fl.size;
		c->crc = fl.crc;
		uwb_send(sizeof(fota_hdr_t) + sizeof(*c), 1);
		int n = uwb_wait_rx(150);
		const fota_commit_ack_t *a = (const fota_commit_ack_t *)rx_payload(n, FOTA_COMMIT_ACK, fl.session, sizeof(fota_commit_ack_t));
		if (a) {
			if (a->status != FOTA_OK) {
				flasher_fail(a->status);
				break;
			}
		} else if (++fl.tries < FOTA_COMMIT_TRIES) {
			break;
		}
		// acked, or no answer because the target is already writing: check after it reboots
		uwb_normal_flasher();
		fl.fast = 0;
		flasher_set(FL_VERIFY_WAIT);
		break;
	}

	case FL_VERIFY_WAIT:
		if (in_state >= FOTA_VERIFY_DELAY_MS)
			flasher_set(FL_VERIFY);
		break;

	case FL_VERIFY:
		fl.t_valid = 0;
		flasher_hello(fl.target, FOTA_HELLO_QUERY);
		if (fl.t_valid) {
			if (fl.tinfo.app_crc == fl.crc) {
				fl.last_err = FOTA_OK;
				flasher_set(FL_DONE);
			} else {
				flasher_fail(FOTA_ERR_VERIFY);
			}
		} else if (in_state > FOTA_VERIFY_TIMEOUT_MS) {
			flasher_fail(FOTA_ERR_VERIFY);
		}
		break;

	case FL_DISCOVER: {
		frame_begin(FOTA_BROADCAST, FOTA_DISCOVER, fl.session);
		uwb_send(sizeof(fota_hdr_t), 1);
		uint32_t t0 = HAL_GetTick();
		for (;;) {
			uint32_t elapsed = HAL_GetTick() - t0;
			if (elapsed >= FOTA_DISCOVER_WINDOW_MS)
				break;
			fota_port_kick_wd();
			int n = uwb_wait_rx(FOTA_DISCOVER_WINDOW_MS - elapsed);
			const fota_hello_ack_t *a = (const fota_hello_ack_t *)rx_payload(n, FOTA_DISCOVER_ACK, fl.session, sizeof(fota_hello_ack_t));
			if (a) {
				uint16_t id = ((const fota_hdr_t *)rxf)->src;
				uint8_t i;
				for (i = 0; i < fl.ndisc && fl.disc[i].id != id; i++) {
				}
				if (i == fl.ndisc && fl.ndisc < FOTA_DISCOVER_MAX)
					fl.ndisc++;
				if (i < FOTA_DISCOVER_MAX) {
					fl.disc[i].id = id;
					fl.disc[i].kind = a->kind;
					fl.disc[i].hw = a->hw;
					fl.disc[i].battery = a->battery;
					memcpy(fl.disc[i].sw, a->sw, sizeof(fl.disc[i].sw));
					fl.disc[i].app_crc = a->app_crc;
				}
			}
			if (n != 0)
				dwt_rxenable(DWT_START_RX_IMMEDIATE);	// keep listening for more answers
		}
		if (++fl.disc_round >= FOTA_DISCOVER_ROUNDS) {
			fl.state = fl.disc_prev_state;
			fl.t_state = now;
		}
		break;
	}

	case FL_QUERY:
		fl.t_valid = 0;
		flasher_hello(fl.query_target, FOTA_HELLO_QUERY);
		if (fl.t_valid || in_state > FOTA_QUERY_TIMEOUT_MS) {
			fl.state = fl.query_target == fl.target && fl.loaded == fl.size && fl.size ? FL_LOADED : FL_IDLE;
			fl.t_state = now;
		}
		break;

	default:
		break;
	}
}

static void flasher_leds(void)
{
	uint32_t now = HAL_GetTick();
	if (now - fl.t_led < FOTA_FAST_BLINK_MS)
		return;
	fl.t_led = now;
	fl.led_on = !fl.led_on;
	fota_port_status_led(fl.led_on);
	fota_port_link_led((fl.state == FL_LOADING || fl.state == FL_DATA || fl.state == FL_COMMIT) ? fl.led_on : 0);
}

static int flasher_in_session(void)
{
	return fl.state >= FL_HELLO && fl.state <= FL_DISCOVER;
}

/* ---------------- public ---------------- */

void fota_poll(void)
{
	if (fl.mode) {
		flasher_step();
		flasher_leds();
		return;
	}
	if (tg.state == T_ARMED) {
		uint32_t now = HAL_GetTick();
		if (now - tg.armed_at >= FOTA_COUNTDOWN_MS) {
			target_session();
		} else if (now - tg.t_led >= FOTA_SLOW_BLINK_MS) {
			// handshake accepted: show it during the countdown, before the data phase
			tg.t_led = now;
			tg.led_on = !tg.led_on;
			fota_port_status_led(tg.led_on);
		}
	}
}

int fota_busy(void)
{
	return fl.mode;
}

int fota_keep_awake(void)
{
	return fl.mode || tg.state == T_ARMED;
}

static uint8_t urep[256];

static void uart_reply(uint8_t opcode, const void *payload, uint8_t len)
{
	urep[0] = opcode;
	urep[1] = len + 3;
	memcpy(&urep[2], payload, len);
	urep[len + 2] = sum8(urep, len + 2);
	fota_port_uart_send(urep, len + 3);
}

static void uart_status(uint8_t opcode, uint8_t status)
{
	uart_reply(opcode, &status, 1);
}

int fota_uart_command(const uint8_t *msg, uint8_t len)
{
	uint8_t op = msg[0];
	const uint8_t *p = msg + 2;
	uint32_t plen = len - 3u;
	uint32_t now = HAL_GetTick();

	if (op == UART_OPCODE_DW_SPI_TEST) {	// DW1000 SPI check: 1024-byte RX buffer read at 2.5 MHz (baseline), 10 MHz, 20 MHz
		extern SPI_HandleTypeDef hspi3;
		static uint8_t base[1024], t16[1024], t8[1024];
		static const uint32_t presc[3] = {SPI_BAUDRATEPRESCALER_64, SPI_BAUDRATEPRESCALER_16, SPI_BAUDRATEPRESCALER_8};
		uint8_t *bufs[3] = {base, t16, t8};
		uint8_t r[1 + 3 * 2 + 3 * 2];
		dwt_forcetrxoff();
		for (int k = 0; k < 3; k++) {
			hspi3.Init.BaudRatePrescaler = presc[k];
			HAL_SPI_Init(&hspi3);
			dwt_readfromdevice(RX_BUFFER_ID, 0, 1024, bufs[k]);
		}
		hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
		HAL_SPI_Init(&hspi3);
		r[0] = 0;
		for (int k = 0; k < 3; k++) {	// first differing offset vs baseline (1024 = identical), last nonzero byte
			uint16_t diff = 1024, last = 0;
			for (int n = 0; n < 1024; n++) {
				if (diff == 1024 && bufs[k][n] != base[n]) diff = n;
				if (bufs[k][n]) last = n;
			}
			memcpy(&r[1 + 2 * k], &diff, 2);
			memcpy(&r[7 + 2 * k], &last, 2);
		}
		uart_reply(op, r, sizeof(r));
		return 1;
	}
	if (op == UART_OPCODE_LED_TEST) {
		uint8_t r[2] = {FOTA_ERR_STATE, 0};
		if (plen >= 2) {
			int n = fota_port_led_set(p[0], p[1]);
			r[0] = n < 0 ? FOTA_ERR_STATE : FOTA_OK;
			r[1] = (uint8_t)(n < 0 ? 0 : n);
		}
		uart_reply(op, r, sizeof(r));
		return 1;
	}
	if (op < UART_OPCODE_FOTA_ENTER || op > UART_OPCODE_FOTA_DISCOVER_LIST)
		return 0;

	if (op == UART_OPCODE_FOTA_STATUS) {
		fota_uart_status_t s;
		memset(&s, 0, sizeof(s));
		s.mode = fl.mode;
		s.state = fl.state;
		s.last_err = fl.last_err;
		s.rate = fl.rate;
		s.target = fl.target;
		s.size = fl.size;
		s.loaded = fl.loaded;
		s.acked = fl.acked;
		s.retries = fl.retries;
		s.elapsed_ms = fl.t_start ? now - fl.t_start : 0;
		s.t_valid = fl.t_valid;
		s.t_status = fl.tinfo.status;
		s.t_kind = fl.tinfo.kind;
		s.t_hw = fl.tinfo.hw;
		s.t_battery = fl.tinfo.battery;
		memcpy(s.t_sw, fl.tinfo.sw, sizeof(s.t_sw));
		s.t_app_crc = fl.tinfo.app_crc;
		uart_reply(op, &s, sizeof(s));
		return 1;
	}
	if (op == UART_OPCODE_FOTA_ENTER) {
		if (!fl.mode) {
			memset(&fl, 0, sizeof(fl));
			fl.mode = 1;
			tg.state = T_IDLE;
			dwt_forcetrxoff();
			uwb_normal_flasher();
		}
		uart_status(op, FOTA_OK);
		return 1;
	}
	if (!fl.mode) {
		uart_status(op, FOTA_ERR_STATE);
		return 1;
	}

	switch (op) {
	case UART_OPCODE_FOTA_EXIT:
		fl.mode = 0;
		fl.state = FL_IDLE;
		fota_port_status_led(0);
		fota_port_link_led(0);
		fota_port_uwb_restore();
		uart_status(op, FOTA_OK);
		break;

	case UART_OPCODE_FOTA_LOAD_BEGIN: {
		fota_uart_load_begin_t b;
		if (plen < sizeof(b) || flasher_in_session()) {
			uart_status(op, flasher_in_session() ? FOTA_ERR_BUSY : FOTA_ERR_STATE);
			break;
		}
		memcpy(&b, p, sizeof(b));
		if (b.size == 0 || b.size > FOTA_MAX_IMAGE) {
			uart_status(op, FOTA_ERR_SIZE);
			break;
		}
		if (b.rate >= FOTA_RATE_NUM) {
			uart_status(op, FOTA_ERR_RATE);
			break;
		}
		fl.target = b.target;
		fl.kind = b.kind;
		fl.rate = b.rate;
		fl.size = b.size;
		fl.crc = b.crc;
		fl.loaded = 0;
		fl.acked = 0;
		fl.last_err = FOTA_OK;
		flasher_set(FL_LOADING);
		uart_status(op, FOTA_OK);
		break;
	}

	case UART_OPCODE_FOTA_LOAD_DATA: {
		uint8_t r[5];
		uint32_t offset;
		r[0] = FOTA_OK;
		if (fl.state != FL_LOADING || plen < 5) {
			r[0] = FOTA_ERR_STATE;
		} else {
			memcpy(&offset, p, 4);
			uint32_t n = plen - 4;
			if (offset != fl.loaded || fl.loaded + n > fl.size) {
				r[0] = FOTA_ERR_SIZE;		// PC resends from the returned offset
			} else {
				memcpy(img + offset, p + 4, n);
				fl.loaded += n;
				if (fl.loaded == fl.size) {
					if (crc_image(img, fl.size) != fl.crc) {
						fl.last_err = FOTA_ERR_CRC;
						flasher_set(FL_FAILED);
						r[0] = FOTA_ERR_CRC;
					} else if (!image_vectors_ok(img)) {
						fl.last_err = FOTA_ERR_IMAGE;
						flasher_set(FL_FAILED);
						r[0] = FOTA_ERR_IMAGE;
					} else {
						flasher_set(FL_LOADED);
					}
				}
			}
		}
		memcpy(&r[1], &fl.loaded, 4);
		uart_reply(op, r, sizeof(r));
		break;
	}

	case UART_OPCODE_FOTA_START:
		if (flasher_in_session() || fl.size == 0 || fl.loaded != fl.size
				|| (fl.state == FL_FAILED && (fl.last_err == FOTA_ERR_CRC || fl.last_err == FOTA_ERR_IMAGE))) {
			uart_status(op, FOTA_ERR_STATE);
			break;
		}
		if (!battery_ok(fota_port_battery())) {
			uart_status(op, FOTA_ERR_BATTERY);
			break;
		}
		fl.session = (uint16_t)(now ^ ((uint32_t)fota_port_self_addr() << 8)) | 1u;
		fl.acked = 0;
		fl.retries = 0;
		fl.t_valid = 0;
		fl.sessions = 0;
		fl.last_err = FOTA_OK;
		fl.t_start = now;
		fl.t_ack = now;
		flasher_set(FL_HELLO);
		uart_status(op, FOTA_OK);
		break;

	case UART_OPCODE_FOTA_QUERY:
		if (plen < 2 || flasher_in_session()) {
			uart_status(op, flasher_in_session() ? FOTA_ERR_BUSY : FOTA_ERR_STATE);
			break;
		}
		memcpy(&fl.query_target, p, 2);
		fl.session = (uint16_t)(now ^ 0x5A5Au) | 1u;
		fl.t_valid = 0;
		flasher_set(FL_QUERY);
		uart_status(op, FOTA_OK);
		break;

	case UART_OPCODE_FOTA_DISCOVER:
		if (flasher_in_session()) {
			uart_status(op, FOTA_ERR_BUSY);
			break;
		}
		fl.ndisc = 0;
		fl.disc_round = 0;
		fl.disc_prev_state = fl.state;
		fl.session = (uint16_t)(rnd() | 1u);
		flasher_set(FL_DISCOVER);
		uart_status(op, FOTA_OK);
		break;

	case UART_OPCODE_FOTA_DISCOVER_LIST: {
		static uint8_t r[3 + 10 * sizeof(fota_disc_entry_t)];
		uint8_t start = plen ? p[0] : 0, count = 0;
		while (count < 10 && start + count < fl.ndisc) {
			memcpy(&r[3 + count * sizeof(fota_disc_entry_t)], &fl.disc[start + count], sizeof(fota_disc_entry_t));
			count++;
		}
		r[0] = fl.ndisc;
		r[1] = start;
		r[2] = count;
		uart_reply(op, r, (uint8_t)(3 + count * sizeof(fota_disc_entry_t)));
		break;
	}

	case UART_OPCODE_FOTA_ABORT:
		if (flasher_in_session()) {
			frame_begin(fl.target, FOTA_ABORT, fl.session);
			uwb_send(sizeof(fota_hdr_t), 0);
			flasher_fail(FOTA_ERR_ABORTED);
		}
		uart_status(op, FOTA_OK);
		break;

	default:
		uart_status(op, FOTA_ERR_STATE);
		break;
	}
	return 1;
}
