/*
 * screen.c
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#include <screen\screen.h>
#include <assertion.h>
#include "stm32f4xx_hal.h"

extern UInt32 _currentLineAddr;
extern BYTE _currentLinePrescaler;

void ScreenDrawPixel(const ScreenBuffer *buffer, PointS point, UInt32 pixel) {
	while (1) {

	}
}
void ScreenDrawPixelRGB(const ScreenBuffer *buffer, PointS point, BYTE r, BYTE g, BYTE b) {

	while (1) {

	}
}

