//TODO: Change name of file to sx1276_lora_structs.h

#ifndef SX1276_LORA_STRUCTS_H
#define SX1276_LORA_STRUCTS_H
#include "stdbool.h"

typedef enum {
    LORA_OK,
    LORA_ERROR_INVALID_VERSION,
    LORA_ERROR_TX_TIMEOUT,
    LORA_ERROR_RX_TIMEOUT,
} LoraStatus_t;

typedef enum {
    FSK = 0,
    LORA,
    
}RadioModems_t;

typedef enum {
	SLEEP = 0,
    STANDBY,
	TX,
	RX_SINGLE,
    RX_CONTINUOUS,
} LoraMode_t;

typedef enum {
    FRQ_868MHZ = 868 * 1000 * 1000, 
    FRQ_169MHZ = 169 * 1000 * 1000,

}RadioFrequency_t;


typedef enum {
    BW_125KHZ = 0x07,  // 0000 0111
    BW_250KHZ = 0x08,  // 0000 1000
    BW_500KHZ = 0x09,  // 0000 1001
}LoraBandwidth_t;


typedef enum {
    CR_4_OUT_OF_5 = 0x01,
    CR_4_OUT_OF_6 = 0x02,
    CR_4_OUT_OF_7 = 0x03,
    CR_4_OUT_OF_8 = 0x04,
}LoraCodingRate_t;

typedef enum {
    EXPLICIT_HEADER = 0,
    IMPLICIT_HEADER
}LoraHeaderMode;






#endif  /*SX1276_LORA_STRUCTS_H*/
