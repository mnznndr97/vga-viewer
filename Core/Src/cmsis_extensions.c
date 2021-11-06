/*
 * cmsis_extensions.c
 *
 *  Created on: Nov 6, 2021
 *      Author: mnznn
 */

#include <cmsis_extensions.h>

osStatus_t osExDelayMs(uint32_t ms) {
	// One tick is equal to 1 ms
	return osDelay(ms);
}
