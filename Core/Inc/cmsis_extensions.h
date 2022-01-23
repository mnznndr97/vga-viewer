/*
 * Header for CMSIS extension functions
 *
 *  Created on: Nov 6, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_CMSIS_EXTENSIONS_H_
#define INC_CMSIS_EXTENSIONS_H_

#include <typedefs.h>
#include <cmsis_os.h>
#include <cmsis_os2.h>

enum osExErrorFlags {
	osExFlagsErrorUnknown = osFlagsErrorUnknown, osExFlagsErrorTimeout = osFlagsErrorTimeout, osExFlagsErrorResource = osFlagsErrorResource, osExFlagsErrorParameter = osFlagsErrorParameter, osExFlagsErrorISR = osFlagsErrorISR
};

/// Checks if a value correspond to a flag error
/// \remarks The function should be used only with values returned from osEventFlags* and osThreadFlags* functions
/// The documentation states that an error has the MSB set
#define osExResultIsFlagsErrorCode(value) (((value) & osFlagsError) != 0)

/// Checks that a base CMSIS function returns osOK; otherwhise Error_Handler() is called
#define CHECK_OS_STATUS(result) if ((result) != osOK) { Error_Handler(); }

/// Waits for a timeout expressed in milliseconds
/// @param ms Milliseconds timeout
/// @return Status of the operation
osStatus_t osExDelayMs(uint32_t ms);

/// Enforce that a CMSIS thread stack is not corrupted using the underlying available API
void osExEnforeStackProtection(osThreadId_t handle);

#endif /* INC_CMSIS_EXTENSIONS_H_ */
