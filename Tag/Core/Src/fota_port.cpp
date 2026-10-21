/*
 * fota_port.cpp
 *
 * Tag side of the FOTA port (see fota.h).
 */

#include "fota.h"
#include "main.h"
#include "dw1000.h"
#include "Led.h"
#include "usart.h"
#include "i2c.h"
#include "rrc2040driver.h"
#include "flash_handler.h"
#include "stm32u5xx_it.h"

extern uint8_t HW_Version;

extern "C" uint16_t fota_port_self_addr(void)
{
	return SELF_ID;		// the id the tag answers ranging polls with
}

extern "C" uint8_t fota_port_kind(void)
{
	return FOTA_KIND_TAG;
}

extern "C" uint8_t fota_port_hw(void)
{
	return HW_Version;
}

extern "C" uint8_t fota_port_battery(void)
{
	return AbsoluteStateOfCharge(&hi2c2);
}

extern "C" void fota_port_status_led(int on)
{
	Status_Led_Color(on ? GREEN : OFF);
}

extern "C" void fota_port_link_led(int on)
{
	Link_Led(on ? GREEN : OFF);
}

extern "C" void fota_port_uwb_restore(void)
{
	dwt_forcetrxoff();
	dwConfig(&config);
}

extern "C" void fota_port_uart_send(void *buf, uint32_t len)
{
	UART_SendMessage(buf, len);
}

extern "C" void fota_port_kick_wd(void)
{
	refresh_WD();
}
