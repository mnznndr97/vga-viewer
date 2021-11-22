/*
 * screen.c
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#include <screen\screen.h>
#include <assertion.h>
#include <math.h>
#include <intmath.h>
#include "stm32f4xx_hal.h"

extern UInt32 _currentLineAddr;
extern BYTE _currentLinePrescaler;

void ScreenDrawRectangle(const ScreenBuffer *buffer, PointS point, SizeS size, const Pen *pen) {
	int hStart = MAX(point.x, 0);
	int hEnd = MIN(point.x + size.width, buffer->screenSize.width);

	int vStart = MAX(point.y, 0);
	int vEnd = MIN(point.y + size.height, buffer->screenSize.height);

	// TODO FIX
	/*BYTE packSize = buffer->packSize;*/

	BYTE packSize = 4;
	// We do de check here to limit the number of branch instruction in the code
	DebugWriteChar('s');
	if (packSize <= 1) {
		for (UInt16 line = vStart; line < vEnd; line++) {
			for (UInt16 pixel = hStart; pixel < hEnd; ++pixel) {
				ScreenDrawPixel(buffer, (PointS ) { pixel, line }, pen);
			}
		}
	} else {
        PointS pixelPoint;

		for (UInt16 line = vStart; line < vEnd; line++) {
            pixelPoint.y = line;
			
            UInt16 drawnPixels = 0;
			for (UInt16 unPixel = hStart & 0x3; unPixel != 0; unPixel = (unPixel + 1) & 0x3, ++drawnPixels) {
                pixelPoint.x = hStart + drawnPixels;
				ScreenDrawPixel(buffer, pixelPoint, pen);
			}

			// TODO Fix 2 constant
			for (UInt16 wPixel = hStart + drawnPixels; wPixel < (hEnd & 0x03); wPixel += packSize, drawnPixels += packSize) {
                pixelPoint.x = wPixel;
				ScreenDrawPixelPack(buffer, pixelPoint, pen);
			}

			for (UInt16 unPixel = hStart + drawnPixels; unPixel < hEnd; ++unPixel) {
                pixelPoint.x = unPixel;
				ScreenDrawPixel(buffer, pixelPoint, pen);
			}
		}
	}
	DebugWriteChar('e');
}

void ScreenDrawPixel(const ScreenBuffer *buffer, PointS point, const Pen *pen) {
	buffer->DrawCallback(point, pen);
}
void ScreenDrawPixelPack(const ScreenBuffer *buffer, PointS point, const Pen *pen) {
	buffer->DrawPackCallback(point, pen);
}

