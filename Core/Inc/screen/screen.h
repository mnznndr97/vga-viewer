/*
 * screen.h
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#ifndef INC_SCREEN_SCREEN_H_
#define INC_SCREEN_SCREEN_H_

#include <typedefs.h>

#define SCREENBUF 100
#define DMA_SCREEN_STREAM DMA2_Stream0

extern void Error_Handler(void);

void ScreenCheckVideoLineEnded();
void ScreenDisableDMA();
void ScreenEnableDMA();

void ScreenInitialize();

#endif /* INC_SCREEN_SCREEN_H_ */
