/*
 * fota.h
 *
 * Firmware-over-the-air (FOTA) over UWB. Shared, identical copy in Anchor
 * and Tag. Any board can be a FOTA flasher (enabled from the PC over UART)
 * or a FOTA target (always listening while it runs its normal UWB loop).
 *
 * Flow:
 *   PC  --UART-->  flasher : ENTER, LOAD_BEGIN, LOAD_DATA..., START
 *   flasher --UWB (normal config)--> target : HELLO
 *   target  --> flasher : HELLO_ACK ("receiving in 5 s")
 *   both switch to the fast UWB config after the countdown
 *   flasher --> target : DATA (chunk, offset)   target --> flasher : DATA_ACK (next offset)
 *   flasher --> target : COMMIT (size, crc)     target --> flasher : COMMIT_ACK
 *   target writes bank 2 from RAM (verify + retry), resets; bootloader checks CRC
 *   flasher --UWB (normal config)--> target : HELLO(query) to confirm new CRC/version
 *
 * The image is received into RAM and only written once its CRC is verified,
 * so a dropped link leaves the running firmware untouched.
 *
 * LEDs: flasher mode = status LED blinks fast, link LED blinks fast while
 * data is sent. Target receiving = status LED blinks slowly, link LED blinks
 * fast while data is received.
 */

#ifndef INC_FOTA_H_
#define INC_FOTA_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- UART opcodes (PC <-> flasher) ---------------- */
#define UART_OPCODE_FOTA_ENTER		0x40	// enter FOTA flasher mode
#define UART_OPCODE_FOTA_LOAD_BEGIN	0x41	// {target, kind, rate, size, crc}
#define UART_OPCODE_FOTA_LOAD_DATA	0x42	// {offset, data[<=128]}
#define UART_OPCODE_FOTA_START		0x43	// start the UWB session
#define UART_OPCODE_FOTA_STATUS		0x44	// read flasher state / progress / target info
#define UART_OPCODE_FOTA_QUERY		0x45	// {target} ask a target for its version/CRC
#define UART_OPCODE_FOTA_ABORT		0x46	// abort the running session
#define UART_OPCODE_FOTA_EXIT		0x47	// leave FOTA flasher mode (back to normal operation)
#define UART_OPCODE_DW_SPI_TEST		0x4F	// diagnostic: 41-byte DW1000 SPI write/read-back (USR_SFD, restored)

/* ---------------- UWB protocol ---------------- */
#define FOTA_FLAG			0xF7	// byte 0 of every FOTA frame (TAG_FLAG=0x41, ANCHOR_FLAG=0xE0)

enum fota_frame_type {
	FOTA_HELLO = 1,
	FOTA_HELLO_ACK,
	FOTA_DATA,
	FOTA_DATA_ACK,
	FOTA_COMMIT,
	FOTA_COMMIT_ACK,
	FOTA_ABORT,
};

enum fota_kind {
	FOTA_KIND_ANCHOR = 0,
	FOTA_KIND_TAG = 1,
};

enum fota_rate {		// UWB setting used for the data phase
	FOTA_RATE_110K = 0,
	FOTA_RATE_850K = 1,
	FOTA_RATE_6M8 = 2,
	FOTA_RATE_NUM
};

enum fota_status_code {
	FOTA_OK = 0,
	FOTA_ERR_BUSY,
	FOTA_ERR_KIND,		// image is for the other device kind
	FOTA_ERR_SIZE,
	FOTA_ERR_SESSION,
	FOTA_ERR_CRC,
	FOTA_ERR_IMAGE,		// vector table does not look like a 0x08200000 app
	FOTA_ERR_STATE,		// command not valid in the current state
	FOTA_ERR_NO_TARGET,	// no HELLO_ACK
	FOTA_ERR_LINK,		// too many lost frames in the data phase
	FOTA_ERR_VERIFY,	// target came back with a different CRC
	FOTA_ERR_ABORTED,
	FOTA_ERR_RATE,
};

#define FOTA_HELLO_QUERY	0x01	// HELLO flag: report info only, don't arm

#define FOTA_COUNTDOWN_MS	5000	// target starts receiving this long after HELLO_ACK
#define FOTA_MAX_IMAGE		(512u * 1024u)
#define FOTA_CHUNK_MAX		512
#define FOTA_APP_ADDRESS	0x08200000u
#define FOTA_HEADER_ADDRESS	0x083FE000u
#define FOTA_CRC_LEN		0x1FE000u	// bootloader CRCs 255 pages of bank 2

#pragma pack(push, 1)
typedef struct {
	uint8_t flag;		// FOTA_FLAG
	uint8_t fc;			// 0x88, as in the ranging frames
	uint8_t seq;
	uint8_t pan_lo;		// 0xCA
	uint8_t pan_hi;		// 0xDE
	uint16_t src;
	uint16_t des;
	uint8_t type;		// enum fota_frame_type
	uint16_t session;
} fota_hdr_t;

typedef struct {
	uint32_t size;
	uint32_t crc;
	uint8_t kind;
	uint8_t rate;
	uint8_t flags;		// FOTA_HELLO_QUERY
} fota_hello_t;

typedef struct {
	uint8_t status;		// enum fota_status_code
	uint16_t countdown_ms;
	uint16_t chunk;
	uint8_t kind;
	uint8_t hw;
	uint8_t battery;
	char sw[9];
	uint32_t app_crc;	// CRC currently stored in the bank 2 header
} fota_hello_ack_t;

typedef struct {
	uint32_t offset;
	uint16_t len;
	uint8_t data[FOTA_CHUNK_MAX];
} fota_data_t;

typedef struct {
	uint32_t next;		// next offset the target expects
	uint8_t status;
} fota_data_ack_t;

typedef struct {
	uint32_t size;
	uint32_t crc;
} fota_commit_t;

typedef struct {
	uint8_t status;
	uint32_t crc;		// CRC the target computed
} fota_commit_ack_t;

/* UART payloads */
typedef struct {
	uint16_t target;
	uint8_t kind;
	uint8_t rate;
	uint32_t size;
	uint32_t crc;
} fota_uart_load_begin_t;

typedef struct {
	uint8_t mode;		// 0 normal, 1 flasher
	uint8_t state;		// enum fota_flasher_state
	uint8_t last_err;
	uint8_t rate;
	uint16_t target;
	uint32_t size;
	uint32_t loaded;
	uint32_t acked;
	uint32_t retries;
	uint32_t elapsed_ms;
	uint8_t t_valid;
	uint8_t t_status;
	uint8_t t_kind;
	uint8_t t_hw;
	uint8_t t_battery;
	char t_sw[9];
	uint32_t t_app_crc;
} fota_uart_status_t;
#pragma pack(pop)

enum fota_flasher_state {
	FL_IDLE = 0,
	FL_LOADING,
	FL_LOADED,
	FL_HELLO,
	FL_COUNTDOWN,
	FL_DATA,
	FL_COMMIT,
	FL_VERIFY_WAIT,
	FL_VERIFY,
	FL_QUERY,
	FL_DONE,
	FL_FAILED,
};

/* ---------------- API ---------------- */
void fota_poll(void);						// call once per main-loop iteration
int fota_busy(void);						// flasher mode: skip normal UWB/LoRa/sleep
void fota_uwb_rx(uint32_t frame_len);		// a received frame starts with FOTA_FLAG
int fota_uart_command(const uint8_t *msg, uint8_t len);	// 1 if the opcode was a FOTA one

/* ---------------- per-project port (fota_port.cpp) ---------------- */
uint16_t fota_port_self_addr(void);
uint8_t fota_port_kind(void);
uint8_t fota_port_hw(void);
uint8_t fota_port_battery(void);
void fota_port_status_led(int on);
void fota_port_link_led(int on);
void fota_port_uwb_restore(void);			// back to the normal UWB configuration
void fota_port_uart_send(void *buf, uint32_t len);
void fota_port_kick_wd(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_FOTA_H_ */
