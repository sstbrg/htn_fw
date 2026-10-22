/*
 * fota_port.cpp
 *
 * Anchor side of the FOTA port (see fota.h).
 */

#include "fota.h"
#include "main.h"
#include "dw1000.h"
#include "Led.h"
#include "usart.h"
#include "stm32u5xx_it.h"
#include "i2c.h"
extern "C" {
#include "rrc2040driver.h"	// C header without its own extern "C"
}

extern uint8_t HW_Version;

// In Led.cpp: battery-bar LEDs, unused on anchors, so status/link keep their normal meaning
void Fota_Status_Led(int on);
void Fota_Link_Led(int on);
int Led_Set(int idx, int on);

extern "C" uint16_t fota_port_self_addr(void)
{
	return lora_rxInfo.self_id;		// same id the anchor uses as UWB source: PC_ID << 2 | anchor_id
}

extern "C" uint8_t fota_port_kind(void)
{
	return FOTA_KIND_ANCHOR;
}

extern "C" uint8_t fota_port_hw(void)
{
	return HW_Version;
}

extern "C" uint8_t fota_port_battery(void)
{
	return RRC2040_Soc(&hi2c2);	// 0xFF = no gauge answered (USB/externally powered anchor)
}

extern "C" void fota_port_status_led(int on)
{
	Fota_Status_Led(on);
}

extern "C" void fota_port_link_led(int on)
{
	Fota_Link_Led(on);
}

extern "C" int fota_port_led_set(uint8_t idx, int on)
{
	return Led_Set(idx, on);
}

extern "C" void fota_port_uwb_restore(void)
{
	dwt_forcetrxoff();
	dwProfileGround();	// profile 0 (#7): state and radio
}

extern "C" void fota_port_uart_send(void *buf, uint32_t len)
{
	UART_SendMessage(buf, (uint16_t)len);
}

extern "C" void fota_port_kick_wd(void)
{
	refresh_WD();
}
