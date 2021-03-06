/*
 * Contains the definition for the debugging assertion and print functions
 * Debug print is done only with characters and is done via ITM
 * 
 *  Created on: Oct 23, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_ASSERTION_H_
#define INC_ASSERTION_H_

#include <stdbool.h>
#include <inttypes.h>

#define SUPPRESS_WARNING(a) (void)a

/// Checks for a condition; if the condition is false, calls the Error_Handler() 
/// function if CUSTOMASSERT is defined; calls assert() otherwhise
/// @param condition Condition to verify
void DebugAssert(bool condition);

/// Writes a single character on the device debug port (ITM Port 0 for STM32F407)
/// @param c Characher to write
/// @remarks The function should not cross the bus matrix (the ITM should be directly connected to the
/// Cortex-M4 as shown in the Debug support block diagram [RM0090 - Section 38.1])
void DebugWriteChar(uint32_t c);

#endif /* INC_ASSERTION_H_ */
