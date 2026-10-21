#ifndef LED
#define LED
#ifdef __cplusplus
extern "C" {
#endif

#include <rrc2040driver.h>
#include <stdint.h>
#include "main.h"
#include "i2c.h"

#define LINK_LED_TIMEOUT 	5000 	//(msec)
#define STATUS_LED_TIMEOUT 	3000	//(msec)
#define BATTERY_LED_TIMEOUT 5000	//(msec)
#define LOW_BATTERY_LED_BLINK_TIMEOUT 500	//(msec)


#define FULL_BATTERY     	0xFF         		//Full Battery
#define LOW_BATTERY 		25					//Low  Battery
#define INVALID_TIMEOUT  	0xFFFFFFFF 			//INVALID_TIMEOUT
#define SOC_TO_LED(soc)  	(((soc) / 25)+1)  	// Convert SOC to LED count

extern uint32_t _1ms_tick;
/*extern I2C_HandleTypeDef hsmbus3;*/


enum LedColor {
	OFF 	,
	RED 	,
	GREEN 	,
};

void Status_Led_Color(enum LedColor);
void Link_Led(enum LedColor);
void Battery_Led(uint8_t SOC);
void Led_Maintanence(void);
void Battary_Led_OFF(void);
void Led_Power_Up_State(uint8_t uwb_status , uint8_t lora_status);
void Led_show(void);
void Init_Leds(uint8_t HW_Version);
int Led_Set(int idx, int on);
#ifdef __cplusplus
}
#endif


#endif
