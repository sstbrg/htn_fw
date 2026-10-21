#ifndef LED
#define LED

#include <stdint.h>
#include "main.h"




#define LINK_LED_TIMEOUT 	5000 	//(msec)
#define STATUS_LED_TIMEOUT 	1000	//(msec)
#define BATTERY_LED_TIMEOUT 3000	//(msec)

#define FULL_BATTERY     	0xFF         		//FULL_BATTERY
#define INVALID_TIMEOUT  	0xFFFFFFFF 			//INVALID_TIMEOUT
#define SOC_TO_LED(soc)  	((soc) / 25)  	// Convert SOC to LED count

extern uint32_t _1ms_tick;


enum LedColor {
	OFF 	,
	RED 	,
	GREEN 	,
};
void Init_Leds(uint8_t HW_Version);
void Status_Led_Color(enum LedColor ,uint32_t Timeout);
void Link_Led(enum LedColor ,uint32_t Timeout);
void Battery_Led(uint8_t SOC ,uint32_t Timeout);
void Led_Maintanence(void);



#endif
