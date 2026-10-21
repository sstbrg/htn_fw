#ifndef RRC2040_H
#define RRC2040_H

#include "stm32u5xx_hal.h"


#define RRC2040_ADDR 0x0B  // 7-bit SMBus address shifted left (SMBus Host) refer pg. 14

// SMBus Register Addresses
#define RRC2040_CMD_VOLTAGE             	0x09
#define RRC2040_CMD_CURRENT             	0x0A
#define RRC2040_CMD_TEMPERATURE         	0x08
#define RRC2040_CMD_REMAINING_CAPACITY  	0x0F
#define RRC2040_CMD_FULL_CAPACITY   	    0x10
#define RRC2040_CMD_BATTERY_STATUS     		0x16
#define RRC2040_CMD_ABSOLUTE_SOC			0x0E
#define RRC2040_CMD_DEVICE_NAME 			0X21

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define ANCHOR_WITH_BATTERY_CONNECTION 3

HAL_StatusTypeDef RRC2040_ReadWord(I2C_HandleTypeDef *hi2c, uint8_t cmd, uint16_t *data);
HAL_StatusTypeDef RRC2040_WriteWord(I2C_HandleTypeDef *hi2c, uint8_t cmd, uint16_t data);
HAL_StatusTypeDef DeviceName(I2C_HandleTypeDef *hi2c, char *nameBuffer, uint8_t maxLen);
HAL_StatusTypeDef RRC2040_ReadBlock(I2C_HandleTypeDef *hi2c, uint8_t cmd, uint8_t *buffer, uint8_t *length);
HAL_StatusTypeDef RRC2040_ReadDeviceName(I2C_HandleTypeDef *hi2c, char *nameBuffer, uint8_t maxLen);
uint16_t RRC2040_GetVoltage(I2C_HandleTypeDef *hi2c);
uint16_t RRC2040_GetCurrent(I2C_HandleTypeDef *hi2c);
uint16_t RRC2040_GetTemperature(I2C_HandleTypeDef *hi2c);
uint16_t RRC2040_GetRemainingCapacity(I2C_HandleTypeDef *hi2c);
uint16_t RRC2040_GetFullCapacity(I2C_HandleTypeDef *hi2c);
uint16_t RRC2040_GetBatteryStatus(I2C_HandleTypeDef *hi2c);
uint8_t AbsoluteStateOfCharge(I2C_HandleTypeDef *hi2c);
uint8_t Get_Battery_Soc(void);

#endif

