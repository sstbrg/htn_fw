#ifndef LORA_H
#define LORA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "./Sx126x_Lib/radio.hpp"
#include "./Sx126x_Lib/sx126x.hpp"
#include "./Sx126x_Lib/sx126x-hal.hpp"
#include "main.h"
#include "dw1000.h"
#include "uart_communication.h"

/*
 *  Global variables declarations
 */
extern uint8_t lora_msg_index;



typedef enum
{
	SEND_PACKET,
	WAIT_SEND_DONE,
    RECEIVE_PACKET,
	WAIT_RECEIVE_DONE,
	PACKET_RECEIVED,
}AppStates_t;

typedef struct{
    RadioPacketTypes_t packetType;
    int8_t txPower;
    RadioRampTimes_t txRampTime;
    ModulationParams_t modParams;
    PacketParams_t packetParams;
    uint32_t rfFrequency;
    uint16_t irqTx;
    uint16_t irqRx;
    uint32_t txTimeout;
    uint32_t rxTimeout;
}RadioConfigurations_t;



typedef struct{
    bool rxDone;
    bool rxError;
    bool txDone;
    bool rxTimeout;
    bool txTimeout;
}RadioFlags_t;

#define debug_if(x) printf(x)

extern SPI_HandleTypeDef hspi2;

//#define MESSAGE_SIZE 5
//#define RX_BUFFER_SIZE 5
#define MESSAGE_SIZE 6
#define RX_BUFFER_SIZE 6
typedef uint8_t Messages_t[MESSAGE_SIZE];

#pragma pack(push, 1)
typedef struct {
	uint8_t msg_index;
	uint16 tag_id;
	uint8_t soc;
	status_bits errors;
	uint8_t checksum;
} sLoRaMsg;
#pragma pack(pop)

#define MAX_INDEX 255

/* Set this flag to '1' to use the LoRa modulation or to '0' to use FSK modulation */
#define USE_MODEM_LORA  1
#define USE_MODEM_FSK   !USE_MODEM_LORA

//#define RF_FREQUENCY                                    915000000 // Hzroot
//#define RF_FREQUENCY_868Mhz							868000000	//868Mhz

#ifndef RF_FREQUENCY_868Mhz
	#define RF_FREQUENCY_MAX                        928000000 	// 928MHz
	#define RF_FREQUENCY_MIN                        902000000 	// 902MHz
#else
	#define RF_FREQUENCY_MAX                        870000000 	// 870MHz
	#define RF_FREQUENCY_MIN                        863000000 	// 863MHz
#endif

#define RF_FREQUENCY_BW                             275000*2 		//  (250KHz + 10%) 275KHz
#define RF_FREQUENCY_BW_HALF						(RF_FREQUENCY_BW / 2)

#define RF_FREQUENCY_START							(RF_FREQUENCY_MIN + RF_FREQUENCY_BW_HALF) // moving the frequency by 1 channel
#define RF_FREQUENCY_STOP							(RF_FREQUENCY_MAX - RF_FREQUENCY_BW_HALF) // moving the frequency by 1 channel

#define RF_FREQUENCY_NUM_OF_CHANELS					((RF_FREQUENCY_STOP - RF_FREQUENCY_START)/RF_FREQUENCY_BW) // preventing from reaching max value
#define RF_FREQ_TAG(tag)							(RF_FREQUENCY_MIN + (tag % RF_FREQUENCY_NUM_OF_CHANELS) * RF_FREQUENCY_BW)

#define TX_OUTPUT_POWER                                 22         // max 22 dBm

#define LORA_BANDWIDTH                              LORA_BW_250   // [0: 125 kHz,
                                                                  //  1: 250 kHz,
                                                                  //  2: 500 kHz,
                                                                  //  3: Reserved]
#define LORA_SPREADING_FACTOR                       LORA_SF12    // [SF7..SF12]
#define LORA_LOWDATARATEOPTIMIZE                    0
#define LORA_CODINGRATE                             LORA_CR_4_5   // [1: 4/5,
                                                                  //  2: 4/6,
                                                                  //  3: 4/7,
                                                                  //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        16        // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         5         // Symbols
#define LORA_HEADER_TYPE                            LORA_PACKET_VARIABLE_LENGTH
#define LORA_FHSS_ENABLED                           false
#define LORA_NB_SYMB_HOP                            4
#define LORA_IQ                                     LORA_IQ_NORMAL
#define LORA_CRC_MODE                               LORA_CRC_OFF

#define RX_TIMEOUT_US 								50000
#define TX_TIMEOUT_US 								50000
#define BUFFER_SIZE                                 MESSAGE_SIZE        // Define the payload size here






void Lora_init( void );
void Lora_Operation_RX(status_bits errors);
void PrepareBuffer(SX126xHal *radio, const sLoRaMsg * messageToSend);
//void PrepareBuffer(SX126xHal *radio, const Messages_t *messageToSend);
void ConfigureRadioTx(SX126xHal *radio, RadioConfigurations_t *config);
void ConfigureRadioRx(SX126xHal *radio, RadioConfigurations_t *config);
void OnTxDone( void );
void OnTxTimeout( void );
void OnRxTimeout( void );
void OnRxDone( void );
void OnRxError( IrqErrorCode_t errCode );
void SetConfiguration(RadioConfigurations_t *config,uint16_t tagId);
void ConfigureGeneralRadio(SX126xHal *radio, RadioConfigurations_t *config);
void RunTXStateMachine(status_bits errors );
void RunRXStateMachine( void );
void GetRssiSnr(int8_t *rssi, int8_t *snr);
void Lora_Status( void );
int Lora_TXidle( void );
// LoRa TX power under the anchor's control (doc section 9D/9J), applied at the next packet
void Lora_LinkMax(void);
uint8_t Lora_LinkRequest(uint16_t src, uint8_t seq, int8_t req, uint8_t confirm, bool multi);
bool Lora_LinkPending(void);
int8_t Lora_LinkPower(void);
// Configuration channel (main.h LCMD_*): beacons with a listen window after some of them
void Lora_TagService(status_bits errors);
void Lora_NoteUwbPoll(bool extended);	// every UWB poll addressed to this tag
void Lora_SetListenN(uint8_t n);		// host override, back to automatic after 30 s without commands
uint8_t Lora_ListenN(void);
uint16_t scramble(uint16_t self_id);

#ifdef __cplusplus
}
#endif

#endif
