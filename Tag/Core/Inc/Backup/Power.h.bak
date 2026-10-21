/*
 * Power.h
 *
 *  Created on: Mar 31, 2025
 *      Author: LironAvrmov
 */

#ifndef SRC_POWER_H_
#define SRC_POWER_H_


#include "stm32u5xx_hal.h"


typedef enum{
	PWR_WakeupReason_POWER_ON,
	PWR_WakeupReason_LORA,
	PWR_WakeupReason_UWB
} PWR_WakeupReason;

typedef enum{
	PWR_Duration_PowerUpSleepDuration	= (6*1000),		//	6 Sec
	PWR_Duration_MyMessage			 	= (15*60*1000),	//	15 min
	PWR_Duration_LabDuration			= (60*60*1000),	//	 1 hour
	PWR_Duration_NEVER					= 0xffffffff, 	// never go to sleep
}	PWR_Duration_E;

/************
 * Utils
 */
void PWR_StandbyInit(void);
PWR_WakeupReason PWR_GoToStandby(void);
void Enable_Wakeup_Pin(uint32_t WakeUpPin);


/************
 * Oprtaional
 */
void PWR_SetTimeOut(PWR_Duration_E duration);
void PWR_Sleep(void);
uint8_t PWR_NeedToSleep(void);

#endif /* SRC_POWER_H_ */
