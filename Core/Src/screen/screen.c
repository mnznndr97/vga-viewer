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

/// \brief Fix the font color using the glyph level provided
/// \param glyphLevel Glyph level ranging from 0 to 64 (included)
/// \param fontColor Color of the font
/// \return Modified font color
/// \remarks The function only changes the alpha of the color to have a "better" result on our super scaled screen
ARGB8Color FixPixelColorWithGlyphLevel(BYTE glyphLevel, ARGB8Color fontColor) {
	// Simple level calculation. Our glyph bitmap levels range from 0 .. 64
	float glyphPixelLevel = glyphLevel / 64.0f;

	// To better display the glyph, I assumed that is better to directly work only on the alpha value
	// In our case, the font bitmaps are exported from Window$. Some of the pixels at the border of the glyph
	// may have a level that is near zero (but not zero). If we modify the RGB components leaving the alpha unaltered
	// for these pixels, we get on the display some black pixels. So i think that is better to only remap the alpha level using 
	// the glyph level
	fontColor.components.A = fontColor.components.A * glyphPixelLevel;
	return fontColor;
}

void ScreenDrawCharacter(const ScreenBuffer *buffer, char character, PointS point, GlyphMetrics *charMetrics, const Pen *pen) {
	// We are currently supporting only simple ASCII characters
	if (character < 0 || character >= 128) {
		// Let's just clear the metrics since they are used from the caller to increment the drawing point
		charMetrics = NULL;
		return;
	}

	// First we get all the information about our character
	PCBYTE glyphBufferPtr;
	GetGlyphOutline(character, charMetrics, &glyphBufferPtr);

	if (charMetrics->bufferSize <= 0) {
		// Character has no graphics associated. No need to draw it
		// The character may still have some enclosing box pixels but 
		// we have not the need to "display" these pixels at the moment
		return;
	}

	// We calculate our glyph origin in screen cordinates
	Int16 glyphOriginX = point.x + charMetrics->glyphOrigin.x;
	Int16 glyphOriginY = point.y + charMetrics->glyphOrigin.y;

	// If glyph is outside the screen (before origin) we need to skip some lines
	BYTE glyphLine = 0;
	BYTE glyphPixelCache = 0;
	if (glyphOriginY < 0 && (-glyphOriginY) >= charMetrics->blackBoxY) {
		// Glyph is totally outside the vertical position of the screen. We can exit
		return;
	} else if (glyphOriginY < 0) {
		// Glyph is partially outside the screen. We simply start drawing from the 0 coordinate
		// ignoring the hidden lines
		// glyphOriginY is smaller that 
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

	// We need to copy the pen since each pixel color will be potentially different
	Pen glyphPixelPen = *pen;

	// The row length of the buffer is a multiple of a Word (32bit), so it must be multiple of 4
	// We simply add 3 to our box width and we clear the 2 LSBs. In this case all the numbers where the
	// LSB are != 0b00 are rounded to the next WORD-aligned number
	BYTE glyphBufferRowWidth = (charMetrics->blackBoxX + 3) & (~0x3);
	// Make sure we are doing fine with our maths
	DebugAssert(glyphBufferRowWidth * charMetrics->blackBoxY == charMetrics->bufferSize);

	// Classic line loop
	for (UInt16 line = vStart; line < vEnd; ++line, ++glyphLine) {
		// We recalculate the current starting pixel of the glyph
		BYTE glyphPixel = glyphPixelCache;

		// We begin our drawing loop
		UInt16 currentPixel = hStart;
		// Since glyph rows are aligned at word boundaries, we can read 4 levels at the time
		// to save little time
		// We just need to make sure we are not starting from an unaligned address
		for (; (glyphPixel & 0x3) != 0 && currentPixel < hEnd; ++currentPixel, ++glyphPixel) {
			// We read the glyph level from the glyph
			BYTE glyphLevel = glyphBufferPtr[glyphLine * glyphBufferRowWidth + glyphPixel];

			// If level is zero it is useless to draw the pixer
			if (glyphLevel != 0) {
				glyphPixelPen.color = FixPixelColorWithGlyphLevel(glyphLevel, pen->color);
				ScreenDrawPixel(buffer, (PointS ) { currentPixel, line }, &glyphPixelPen);
			}
		}

		// From here all accesses to glyphBuffer should be aligned
		for (; currentPixel < hEnd; glyphPixel += 4) {
			// We read the 4 glyph levels from the glyph
			UInt32 glyphLevels = *((UInt32*) &glyphBufferPtr[glyphLine * glyphBufferRowWidth + glyphPixel]);

			// NB: Our system is little endian so the 1st level is now on the LSB of the 32bit int
			// So we start with no right shift and we the increment the shift of 8 bits at each iteration
			// First iteration we read 000000XX (1st glyph level in little endian), then 0000XX00, 00XX0000, XX000000 (4th glyph level)
			BYTE i = 0;
			while (i < 32 && currentPixel < hEnd) {
				BYTE glyphLevel = (glyphLevels >> i) & 0xFF;

				// If level is zero it' s useless to draw the pixer
				if (glyphLevel != 0) {
					glyphPixelPen.color = FixPixelColorWithGlyphLevel(glyphLevel, pen->color);
					ScreenDrawPixel(buffer, (PointS ) { currentPixel, line }, &glyphPixelPen);
				}
				// Better than using i = 0 .. 3 and multipling by 8
				i += 8;
				++currentPixel;
			}
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
			for (UInt16 wPixel = hStart + drawnPixels; wPixel < (hEnd & (~0x03)); wPixel += packSize, drawnPixels += packSize) {
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
	// This is not safe but our project is just display some stuff on the screen, there is not
	// sensible data in it. It may still be hacked though
	int stringLength = strlen(str);

	if (stringLength <= 0) {
		// Nothing to draw
		return;
	}

	// Super simple loop here
	// For each character in our string we draw it's glyph on the screen and we move the point forward 
	GlyphMetrics charMetrics;
	for (int i = 0; i < stringLength; i++) {
		char character = str[i];
		ScreenDrawCharacter(buffer, character, point, &charMetrics, pen);

		// We move our "drawing cursor" forward using the font specifications
		// The font also has a Y increment but we are not interested 
		point.x += charMetrics.cellIncX;
	}
}

void ScreenDrawPixel(const ScreenBuffer *buffer, PointS point, const Pen *pen) {
	buffer->DrawCallback(point, pen);
}
void ScreenDrawPixelPack(const ScreenBuffer *buffer, PointS point, const Pen *pen) {
	buffer->DrawPackCallback(point, pen);
}

