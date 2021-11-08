/*
 * assertion.c
 *
 *  Created on: Oct 23, 2021
 *      Author: mnznn
 */

#include <assertion.h>
#include <assert.h>

extern void Error_Handler(void);

void DebugAssert(bool condition) {
#ifdef CUSTOMASSERT
        if(!condition) Error_Handler();
#else
	assert(condition);
#endif
}
