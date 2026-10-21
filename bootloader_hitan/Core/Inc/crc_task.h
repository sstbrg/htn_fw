/*
 * crc_task.h
 *
 *  Created on: Nov 4, 2025
 *      Author: p9260792
 */

#ifndef INC_CRC_TASK_H_
#define INC_CRC_TASK_H_

#include "stdbool.h"

static inline void CRC_Reset(void);
uint32_t FlashCRC_Calc(uint32_t start_addr, uint32_t stop_addr);
static bool FlashRange_Valid(uint32_t start_addr, uint32_t stop_addr);
void Clear_Header_File(void);
#endif /* INC_CRC_TASK_H_ */
