#include <rrc2040driver.h>
#include "stm32u5xx_hal_i2c.h"
#include "string.h"

uint16_t soc_battery = 101;

// Read 16-bit word from the battery
HAL_StatusTypeDef RRC2040_ReadWord(I2C_HandleTypeDef *hi2c, uint8_t cmd, uint16_t *data) {

	uint8_t buffer[2];
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(hi2c, RRC2040_ADDR << 1, &cmd, 1, 100);
    if (status != HAL_OK) {
        return status;
    }

    // Step 2: Read two bytes (read phase with repeated start)
    status = HAL_I2C_Master_Receive(hi2c, (RRC2040_ADDR << 1) | 0x01, buffer, 2, 10);
    if (status == HAL_OK) {
        // SMBus uses little endian (low byte first)
        *data = (buffer[1] << 8) | buffer[0];
    }
    return status;
}


uint8_t Get_Battery_Soc(void){
	return soc_battery;
}

// Get battery voltage (mV)
uint16_t RRC2040_GetVoltage(I2C_HandleTypeDef *hi2c) {
    uint16_t voltage = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_VOLTAGE, &voltage);
    return voltage;
}

// Get battery current (mA)
uint16_t RRC2040_GetCurrent(I2C_HandleTypeDef *hi2c) {
    uint16_t current = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_CURRENT, &current);
    return current;
}

// Get battery temperature (0.1K units)
uint16_t RRC2040_GetTemperature(I2C_HandleTypeDef *hi2c) {
    uint16_t temp = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_TEMPERATURE, &temp);
    return temp;
}

// Get remaining capacity (mAh)
uint16_t RRC2040_GetRemainingCapacity(I2C_HandleTypeDef *hi2c) {
    uint16_t capacity = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_REMAINING_CAPACITY, &capacity);
    return capacity;
}

// Get full charge capacity (mAh)
uint16_t RRC2040_GetFullCapacity(I2C_HandleTypeDef *hi2c) {
    uint16_t full_capacity = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_FULL_CAPACITY, &full_capacity);
    return full_capacity;
}

// Get battery status
uint16_t RRC2040_GetBatteryStatus(I2C_HandleTypeDef *hi2c) {
    uint16_t status = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_BATTERY_STATUS, &status);
    return status;
}

// Battery charge 0..100 %, or RRC2040_NO_GAUGE when no battery answers on I2C
// (AbsoluteStateOfCharge() returns 0 for both). A failed read costs up to
// ~110 ms of I2C timeouts, so after a failure it is retried at most every 5 s.
uint8_t RRC2040_Soc(I2C_HandleTypeDef *hi2c) {
	static uint8_t last = RRC2040_NO_GAUGE;
	static uint8_t tried = 0;
	static uint32_t last_fail = 0;
	uint16_t soc;

	if (tried && last == RRC2040_NO_GAUGE && HAL_GetTick() - last_fail < 5000)
		return last;
	tried = 1;
	if (RRC2040_ReadWord(hi2c, RRC2040_CMD_ABSOLUTE_SOC, &soc) == HAL_OK) {
		last = MIN(soc, 100);
	} else {
		last = RRC2040_NO_GAUGE;
		last_fail = HAL_GetTick();
	}
	return last;
}

uint8_t AbsoluteStateOfCharge(I2C_HandleTypeDef *hi2c) {
	soc_battery = 0;
    RRC2040_ReadWord(hi2c, RRC2040_CMD_ABSOLUTE_SOC, &soc_battery);
    return (MIN(soc_battery,100));
}

HAL_StatusTypeDef RRC2040_ReadDeviceName(I2C_HandleTypeDef *hi2c, char *nameBuffer, uint8_t maxLen) {

	uint8_t cmd = 0x21;          // DeviceName command
    uint8_t byteCount = 0;
    uint8_t data[32];            // Max SMBus block length
    HAL_StatusTypeDef status;

    // STEP 1: Send command code using Write
    status = HAL_I2C_Master_Transmit(hi2c, RRC2040_ADDR << 1, &cmd, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }

    // STEP 2: Read 1 byte (length)
    status = HAL_I2C_Master_Receive(hi2c, (RRC2040_ADDR << 1) | 0x01, &byteCount, 1, HAL_MAX_DELAY);
    if (status != HAL_OK || byteCount == 0 || byteCount > 32) {
        return HAL_ERROR;
    }

    // STEP 3: Read actual string data
    status = HAL_I2C_Master_Receive(hi2c, (RRC2040_ADDR << 1) | 0x01, data, byteCount, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }

    // STEP 4: Copy to user buffer (safe length)
    if (byteCount >= maxLen) {
        byteCount = maxLen - 1;
    }

    memcpy(nameBuffer, data, byteCount);
    nameBuffer[byteCount] = '\0'; // Null-terminate the string

    return HAL_OK;
}
