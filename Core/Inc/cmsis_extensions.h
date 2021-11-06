/*
 * cmsis_extensions.h
 *
 *  Created on: Nov 6, 2021
 *      Author: mnznn
 */

#ifndef INC_CMSIS_EXTENSIONS_H_
#define INC_CMSIS_EXTENSIONS_H_

#include <typedefs.h>
#include <cmsis_os2.h>


enum osExErrorFlags {
    osExFlagsErrorUnknown = osFlagsErrorUnknown,
    osExFlagsErrorTimeout = osFlagsErrorTimeout,
    osExFlagsErrorResource = osFlagsErrorResource,
    osExFlagsErrorParameter = osFlagsErrorParameter,
    osExFlagsErrorISR = osFlagsErrorISR
};


/// Checks if a value correspond to a flag error
/// \remarks The function should be used only with values returned from osEventFlags* and osThreadFlags* functions
/// The documentation () states that an error has the MSB set
#define osExResultIsFlagsErrorCode(value) (((value) & osFlagsError) != 0) 

osStatus_t osExDelayMs(uint32_t ms);

#endif /* INC_CMSIS_EXTENSIONS_H_ */
