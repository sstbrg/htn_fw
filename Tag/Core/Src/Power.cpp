/*
 * Power.c
 *
 *  Created on: Mar 31, 2025
 *      Author: LironAvrmov
 */

#include "main.h"
#include "Power.h"
#include "tim.h"
#include "stm32u5xx_nucleo.h"
#include "Led.h"
#include "dw1000.h"


#define  	LORA_WAKEUP_PIN		(1<<0)
#define 	UWB_WAKEUP_PIN		(1<<2)


static uint32_t	gDuration;
static uint32_t	gSavedTick;

/***********************
 *
 * 	Powerup Init / Utils
 *
 */
void Enable_Wakeup_Pin(uint32_t WakeUpPin){
	HAL_PWR_EnableWakeUpPin(WakeUpPin);
}

void PWR_StandbyInit(void) {
	/* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
	HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH_0);
	BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);
}

PWR_WakeupReason PWR_GoToStandby(void) {
	//	set UWB Timeout to "forever"
//	dw_PreSleep();
	// suspend timers
	HAL_SuspendTick();
	HAL_TIM_Base_Stop(&htim4);

	// prepare EXTI
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_SBF);  // Clear Standby flag
	__HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG1); // Clear wake-up flag
	__HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG8); // Clear wake-up flag

	// keep COM components active upon Sleep
	HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_A, PWR_GPIO_BIT_12);	//	Keep LoRa reset pin high upon sleep
	HAL_PWREx_EnablePullUpPullDownConfig();
	HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_D, PWR_GPIO_BIT_0);	//	Keep UWB reset pin high upon sleep
	HAL_PWREx_EnablePullUpPullDownConfig();

	/*******************
	 * 	Enter Sleep
	 *******************/
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);	//	<- Enter Stop mode
	/*******************
	 * 	After Sleep
	 *******************/

	// Exit Stop mode
	HAL_ResumeTick();
	HAL_TIM_Base_Start_IT(&htim4);

	//	restore set UWB Timeout
//	dw_PostSleep();

	if (PWR->WUSR & LORA_WAKEUP_PIN)
		return PWR_WakeupReason_LORA;

	if (PWR->WUSR & UWB_WAKEUP_PIN)
		return PWR_WakeupReason_UWB;

	return PWR_WakeupReason_POWER_ON;
}

/******************************
 * Operational
 */
void PWR_SetTimeOut(PWR_Duration_E duration)
{
	gSavedTick = HAL_GetTick();
	gDuration = duration;
}


uint8_t PWR_NeedToSleep(void)
{
	return ((HAL_GetTick() - gSavedTick) > gDuration)? 1:0;
}

//	returns 1 for wakeup
void PWR_Sleep(void) {
		status_bits errors = {0};
		Battary_Led_OFF();
		Status_Led_Color(OFF);
		Link_Led(OFF);
		Lora_Operation_RX(errors);

		PWR_GoToStandby();
		HAL_Delay(2);
}

