/*
 * assertion.h
 *
 *  Created on: Oct 23, 2021
 *      Author: mnznn
 */

#ifndef INC_ASSERTION_H_
#define INC_ASSERTION_H_

#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

void DebugAssert(bool condition);

/// \brief Writes a single characted on the device debug port
/// (ITM Port 0 for STM32F407)
/// \param c Characher to write
/// \remarks The function should not cross the bus matrix (the ITM should be directly connected to the
/// Cortex-M4 as shown in the Debug support block diagram [RM0090 - Section 38.1])
void DebugWriteChar(uint32_t c);
void DebugWriteString(const char *c);

#endif /* INC_ASSERTION_H_ */
