/*! ----------------------------------------------------------------------------
 * @file    deca_sleep.c
 * @brief   platform dependent sleep implementation
 *
 * @attention
 *
 * Copyright 2015 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */

#include "../decadriver/deca_device_api.h"
#include "port.h"
#include "sleep.h"

/* Wrapper function to be used by decadriver. Declared in deca_device_api.h */
__INLINE void deca_sleep(unsigned int time_ms)
{
	Sleep(time_ms);
}

