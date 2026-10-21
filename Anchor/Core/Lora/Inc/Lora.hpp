#ifndef LORA_H
#define LORA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "Sx126x_Lib/radio.hpp"
#include "Sx126x_Lib/sx126x.hpp"
#include "Sx126x_Lib/sx126x-hal.hpp"
#include "main.h"

/*
 *  Global variables declarations
 */


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
	bool wakeup_seq;
    bool rxDone;
    bool rxError;
    bool rxTimeout;
    bool txDone;
    bool txTimeout;
    TransState_t RxOrTx;
}RadioFlags_t;

#define debug_if(x) printf(x)

extern SPI_HandleTypeDef hspi2;

#define LR_NO_VALID_DATA 0x0F
#define VALID_DATA 0x00

#define DETECT_ID 33
//#define MESSAGE_SIZE 5
//#define TAG_MESSAGE_SIZE 5
//#define BUFFER_SIZE  5       // Define the payload size here

#define MESSAGE_SIZE 6
#define TAG_MESSAGE_SIZE 6
#define BUFFER_SIZE  6       // Define the payload size here
//#define RX_BUFFER_SIZE 2
typedef uint8_t Messages_t[MESSAGE_SIZE];
typedef uint8_t TAG_Messages_t[TAG_MESSAGE_SIZE];


//const Messages_t PingMsg = {'L', 'I', 'R', 'O'};
#define MAX_INDEX 255

/* Set this flag to '1' to use the LoRa modulation or to '0' to use FSK modulation */
#define USE_MODEM_LORA  1
#define USE_MODEM_FSK   !USE_MODEM_LORA



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
#define RF_FREQ_TAG(tag)							((uint32_t)(RF_FREQUENCY_MIN + (tag % RF_FREQUENCY_NUM_OF_CHANELS) * RF_FREQUENCY_BW))
//#define RF_FREQ_TAG(tag)							((uint32_t)(RF_FREQUENCY_MIN + RF_FREQUENCY_MAX)/2)


#define TX_OUTPUT_POWER                                 22         // max 22 dBm

#define LORA_BANDWIDTH                              LORA_BW_250   // [0: 125 kHz,
                                                                  //  1: 250 kHz,
                                                                  //  2: 500 kHz,
                                                                  //  3: Reserved]
#define LORA_SPREADING_FACTOR                       LORA_SF12      // [SF7..SF12]
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


#define GET_STATUS 										0xFE


void Lora_Status( void );
void Lora_init( LoraRxInfo_t* pLoraRxInfo, DwmInfo_t * inDwmInfoPtr);
int Lora_TXIdle(void);
int Lora_RXIdle(void);
void TAG_Lora_Operation_TX(status_bits errors);
void Lora_Operation_TX( void );
void Lora_Operation_RX( void );
void TAG_PrepareBuffer(SX126xHal *radio, const sLoRaMsg *messageToSend);
void PrepareBuffer(SX126xHal *radio, const sLoRaWakeupMsg *messageToSend);
void ConfigureRadioTx(SX126xHal *radio, RadioConfigurations_t *config);
void ConfigureRadioRx(SX126xHal *radio, RadioConfigurations_t *config);
void OnTxDone( void );
void OnTxTimeout( void );
void OnRxTimeout( void );
void OnRxDone( void );
void OnRxError( IrqErrorCode_t errCode );
void SetConfiguration(RadioConfigurations_t *config ,uint16_t tag_dest);
void ConfigureGeneralRadio(SX126xHal *radio, RadioConfigurations_t *config);
void RunTXStateMachine( void );
void RunRXStateMachine( void );
void GetRssiSnr(int8_t *rssi, int8_t *snr);
void Update_Lora_Config_Imposter(LoraRxInfo_t* pLoraRxInfo);
void Update_Lora_Config_Anchor(LoraRxInfo_t* pLoraRxInfo, DwmInfo_t * inDwmInfoPtr);
void Update_Lora_Config(LoraRxInfo_t* pLoraRxInfo, uint16_t tag_dest);

uint16_t scramble(uint16_t self_id);
void  Lora_Debug();
bool Lora_CmdService(void);		// main loop: a configuration command exchange owns the radio
void Lora_SendCommand(void);


#ifdef __cplusplus
}
#endif

#endif
