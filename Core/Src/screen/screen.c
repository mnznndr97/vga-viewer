/*
 * screen.c
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#include <screen\screen.h>
#include <fonts\glyph.h>
#include <assertion.h>
#include <string.h>
#include <math.h>
#include <intmath.h>
#include "stm32f4xx_hal.h"

extern UInt32 _currentLineAddr;
extern BYTE _currentLinePrescaler;

void ScreenDrawCharacter(const ScreenBuffer *buffer, char character, PointS point, GlyphMetrics *charMetrics, const Pen *pen) {
	// First we get all the information about our character
	PCBYTE glyphBufferPtr;
	GetGlyphOutline(character, charMetrics, &glyphBufferPtr);

	if (charMetrics->bufferSize <= 0) {
		// Character has no graphics associated
		return;
	}

	// We calculate our glyph origin in screen cordinates
	Int16 glyphOriginX = point.x + charMetrics->glyphOrigin.x;
	Int16 glyphOriginY = point.y + 36 - charMetrics->glyphOrigin.y;

	// If glyph is outside the screen (before origin) we need to skip some lines
	// TODO: check byte cast
	BYTE glyphLine = 0;
	BYTE glyphPixelCache = 0;
	if (glyphOriginY < 0) {
		glyphLine = glyphLine - (BYTE) glyphOriginY;
		glyphOriginY = 0;
	}

	if (glyphOriginX < 0) {
		glyphPixelCache = glyphPixelCache - (BYTE) glyphOriginX;
		glyphOriginX = 0;
	}

	// Calculating our character visible rectangle
	// Origin is just clipped to 0 if was negative
	int hStart = glyphOriginX;
	int vStart = glyphOriginY;
	int hEnd = MIN(glyphOriginX + charMetrics->blackBoxX, buffer->screenSize.width);
	int vEnd = MIN(glyphOriginY + charMetrics->blackBoxY, buffer->screenSize.height);

	// We need to copy the pen since each pixel must be different
	Pen glyphPixelPen = *pen;
	ARGB8Color originalColor = glyphPixelPen.color;

	// The row length of the buffer is a multiple of a Word (32bit), so it must be multiple of 4
	// We simply add 3 to our box width and we clear the 2 LSBs. In this case all the numbers where the
	// LSB are != 0b00 are rounded to the next WORD-ligned number
	BYTE glyphBufferRowWidth = (charMetrics->blackBoxX + 3) & (~0x3);
	DebugAssert(glyphBufferRowWidth * charMetrics->blackBoxY == charMetrics->bufferSize);

	// Classic line loop
	for (UInt16 line = vStart; line < vEnd; ++line, ++glyphLine) {
		// We recalculate the current starting pixel of the glyph in case we was startng out of the screen
		BYTE glyphPixel = glyphPixelCache;
		for (UInt16 pixel = hStart; pixel < hEnd; ++pixel, ++glyphPixel) {
			// We read the pixel level from the glyph
			BYTE pixelLevel = glyphBufferPtr[glyphLine * glyphBufferRowWidth + glyphPixel];

			// Simple level calculation
			float glyphPixelLevel = pixelLevel / 64.0f;

			glyphPixelPen.color.components.R = originalColor.components.R * glyphPixelLevel;
			glyphPixelPen.color.components.G = originalColor.components.G * glyphPixelLevel;
			glyphPixelPen.color.components.B = originalColor.components.B * glyphPixelLevel;
			ScreenDrawPixel(buffer, (PointS ) { pixel, line }, &glyphPixelPen);
		}
	}
}

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

void ScreenDrawString(const ScreenBuffer *buffer, const char *str, PointS point, const Pen *pen) {
	// TODO: I know this is not safe :(
	int stringLength = strlen(str);

	if (stringLength <= 0) {
		// Nothing to draw
		return;
	}

	PointS currentDrawingPoint = point;
	GlyphMetrics charMetrics;
	for (int i = 0; i < stringLength; i++) {
		char character = str[i];

		ScreenDrawCharacter(buffer, character, currentDrawingPoint, &charMetrics, pen);

		currentDrawingPoint.x += charMetrics.cellIncX;
	}
}

void ScreenDrawPixel(const ScreenBuffer *buffer, PointS point, const Pen *pen) {
	buffer->DrawCallback(point, pen);
}
void ScreenDrawPixelPack(const ScreenBuffer *buffer, PointS point, const Pen *pen) {
	buffer->DrawPackCallback(point, pen);
}

