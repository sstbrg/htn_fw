/*
 * burn_mode.c
 *
 *  Created on: May 13, 2025
 *      Author: Pituah
 */

#include "main.h"
#include "uart_communication.h"
#include "usart.h"
#include <string.h>

//void Burn_mode(S_UART_MESSAGE *msg, S_UART_MESSAGE *reply){
//		UART_Reply_Burn_msg(msg,reply);
//		while(1){
//			uint8_t RxLen = UART_GetMessage((uint8_t*) msg, sizeof(*msg));
//			if ((RxLen != RX_EMPTY) && Validate_UART(msg, RxLen)){
//				switch(msg->header.opcode){
//
//					case UART_OPCODE_SET_ID:
//							{
//								uint64_t Data = msg->set_id_req.new_id;
//								uint8_t error;
//								error = Flash_Erase(FLASH_SELF_ID_PAGE) != HAL_OK;
//								if (error){
//									UART_Reply_Set_Self_ID(msg, reply, error);
//									break;
//								}
//								error = Flash_Write(FLASH_SELF_ID_ADDRESS, &Data, 1) != HAL_OK;
//								UART_Reply_Set_Self_ID(msg, reply, error);
//								break;
//							}
//					case UART_OPCODE_RESET:
//							{
//								UART_Reply_Status(msg,reply);
//								HAL_Delay(100);
//								HAL_NVIC_SystemReset();
//								break;
//							}
//					default:{
//						UART_Reply_error(msg, reply);
//						break;
//					}
//				}
//
//			}
//		}
//}

