#include "Led.h"

uint32_t StatusTimeout = 0;
uint32_t LinkTimeout = 0;
uint32_t batteryTimeout = 0;


#define PORT(LED)	Leds_GPIO[LED].GPIO_Port
#define PIN(LED)	Leds_GPIO[LED].GPIO_Pin

typedef struct{
	GPIO_TypeDef *GPIO_Port;
	uint16_t GPIO_Pin;
}LED_PIN;

enum
{
	LED_BAT_0,
	LED_BAT_1,
	LED_BAT_2,
	LED_BAT_3,
	LED_BIT_FAIL,
	LED_BIT_PASS,
	LED_LINKUP,

	LED_NUM
};
LED_PIN RevC1_Leds[] = {
		{GPIOE, GPIO_PIN_14},	// LED_BAT_0
		{GPIOE, GPIO_PIN_11},	// LED_BAT_1
		{GPIOE, GPIO_PIN_8 },	// LED_BAT_2
		{GPIOF, GPIO_PIN_15},	// LED_BAT_3
		{GPIOB, GPIO_PIN_1},	// LED_BIT_FAIL
		{GPIOF, GPIO_PIN_12},	// LED_BIT_PASS
		{GPIOA, GPIO_PIN_0},	// LED_LINKUP
};

LED_PIN RevC2_Leds[] = {
		{GPIOE, GPIO_PIN_14},	// LED_BAT_0
		{GPIOE, GPIO_PIN_11},	// LED_BAT_1
		{GPIOE, GPIO_PIN_8 },	// LED_BAT_2
		{GPIOF, GPIO_PIN_15},	// LED_BAT_3
		{GPIOB, GPIO_PIN_2},	// LED_BIT_FAIL
		{GPIOB, GPIO_PIN_1},	// LED_BIT_PASS
		{GPIOF, GPIO_PIN_12},	// LED_LINKUP
};

LED_PIN *Leds_GPIO;

void Init_Leds(uint8_t HW_Version)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if (HW_Version == 0)	//	rev C
		Leds_GPIO = RevC1_Leds;
	else
		Leds_GPIO = RevC2_Leds;


	for (int i = 0 ; i < LED_NUM; i++)
	{	//	Set LED as output
		GPIO_InitStruct.Pin = PIN(i);
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(PORT(i), &GPIO_InitStruct);
		HAL_GPIO_WritePin(PORT(i),PIN(i), GPIO_PIN_SET);
	}
}


void Status_Led_Color(enum LedColor color) {
	switch (color) {
	case RED: //BIT Failed
		HAL_GPIO_WritePin(PORT(LED_BIT_FAIL),PIN(LED_BIT_FAIL), GPIO_PIN_RESET);
		HAL_GPIO_WritePin(PORT(LED_BIT_PASS),PIN(LED_BIT_PASS), GPIO_PIN_SET);
		break;
	case GREEN: //BIT success
		HAL_GPIO_WritePin(PORT(LED_BIT_PASS),PIN(LED_BIT_PASS), GPIO_PIN_RESET);
		HAL_GPIO_WritePin(PORT(LED_BIT_FAIL),PIN(LED_BIT_FAIL),GPIO_PIN_SET);
		break;
	case OFF:
	default:
		HAL_GPIO_WritePin(PORT(LED_BIT_PASS),PIN(LED_BIT_PASS), GPIO_PIN_SET);
		HAL_GPIO_WritePin(PORT(LED_BIT_FAIL),PIN(LED_BIT_FAIL), GPIO_PIN_SET);
		break;
	}
	StatusTimeout = HAL_GetTick();
}

void Link_Led(enum LedColor color) {
	switch (color) {
	case GREEN:
		HAL_GPIO_WritePin(PORT(LED_LINKUP),PIN(LED_LINKUP), GPIO_PIN_RESET);
		break;
	case OFF:
		HAL_GPIO_WritePin(PORT(LED_LINKUP),PIN(LED_LINKUP), GPIO_PIN_SET);
		break;
	default:
		HAL_GPIO_WritePin(PORT(LED_LINKUP),PIN(LED_LINKUP), GPIO_PIN_SET);
		break;
	}
	LinkTimeout = HAL_GetTick();
}

void Battary_Led_OFF(void){
	HAL_GPIO_WritePin(PORT(LED_BAT_0),PIN(LED_BAT_0), GPIO_PIN_SET);
	HAL_GPIO_WritePin(PORT(LED_BAT_1),PIN(LED_BAT_1), GPIO_PIN_SET);
	HAL_GPIO_WritePin(PORT(LED_BAT_2),PIN(LED_BAT_2), GPIO_PIN_SET);
	HAL_GPIO_WritePin(PORT(LED_BAT_3),PIN(LED_BAT_3), GPIO_PIN_SET);
}

// FOTA indication on the battery bar, which externally powered anchors don't use:
// LED_BAT_0 = FOTA status, LED_BAT_3 = FOTA data. Refreshing batteryTimeout keeps
// Led_Maintanence from switching them off while FOTA drives them.
void Fota_Status_Led(int on) {
	HAL_GPIO_WritePin(PORT(LED_BAT_0),PIN(LED_BAT_0), on ? GPIO_PIN_RESET : GPIO_PIN_SET);
	batteryTimeout = HAL_GetTick();
}

void Fota_Link_Led(int on) {
	HAL_GPIO_WritePin(PORT(LED_BAT_3),PIN(LED_BAT_3), on ? GPIO_PIN_RESET : GPIO_PIN_SET);
	batteryTimeout = HAL_GetTick();
}

void Led_Maintanence(void) {

	if (HAL_GetTick() - StatusTimeout > STATUS_LED_TIMEOUT) {
		Status_Led_Color(OFF);
		StatusTimeout = INVALID_TIMEOUT;
	}

	if (HAL_GetTick() - LinkTimeout > LINK_LED_TIMEOUT) {
		Link_Led(OFF);
		LinkTimeout = INVALID_TIMEOUT;
	}

	if (HAL_GetTick() - batteryTimeout > BATTERY_LED_TIMEOUT) {
		Battary_Led_OFF();
		batteryTimeout = INVALID_TIMEOUT;
	}
}


#define LEDS_SHOW 5

void Led_show(void){
	HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);//Turn off RGB led
	HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);//Turn off RGB led
	HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET); //Turn off RGB led
	int max = LEDS_SHOW;

	for (int led =0; led < max ; led++){
		HAL_GPIO_WritePin(PORT(led),PIN(led), GPIO_PIN_SET);
	}

	HAL_GPIO_WritePin(PORT(LED_BIT_PASS),PIN(LED_BIT_PASS), GPIO_PIN_SET);
	for (int led =0; led < max ; led++){
		HAL_GPIO_WritePin(PORT(led),PIN(led), GPIO_PIN_RESET);
		HAL_Delay(100);
	}
	HAL_GPIO_WritePin(PORT(LED_BIT_PASS),PIN(LED_BIT_PASS), GPIO_PIN_SET);

	HAL_GPIO_WritePin(PORT(LED_BIT_FAIL),PIN(LED_BIT_FAIL), GPIO_PIN_RESET);
	for (int led =0; led < max ; led++){
		HAL_GPIO_WritePin(PORT(led),PIN(led), GPIO_PIN_SET);
		HAL_Delay(100);
	}
	HAL_GPIO_WritePin(PORT(LED_BIT_FAIL),PIN(LED_BIT_FAIL), GPIO_PIN_SET);

}

