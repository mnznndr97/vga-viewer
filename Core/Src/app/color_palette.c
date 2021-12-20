/*
 * color_palette.c
 *
 *  Created on: Nov 29, 2021
 *      Author: mnznn
 */

#include <app/color_palette.h>
#include <stddef.h>

static ScreenBuffer *_activeBuffer = NULL;

static BYTE _redLevels = NULL;
static BYTE _redLevel = NULL;

static BYTE _redIncrement = NULL;

static void DrawPalette() {
	Pen currentPen = { };
	ScreenBuffer *screenBuffer = _activeBuffer;

	currentPen.color.components.A = 0xFF;
	currentPen.color.components.R = _redLevel * _redIncrement;

	// Simple loop to draw all the blue levels by line and all green levels by row
	float bluDivisions = screenBuffer->screenSize.height / 256.0f;
	float greenDivisions = screenBuffer->screenSize.width / 256.0f;
	for (size_t line = 0; line < screenBuffer->screenSize.height; line++) {
		currentPen.color.components.B = (BYTE) (line / bluDivisions);

		for (size_t pixel = 0; pixel < screenBuffer->screenSize.width; pixel++) {
			currentPen.color.components.G = (BYTE) (pixel / greenDivisions);
			ScreenDrawPixel(screenBuffer, (PointS ) { pixel, line }, &currentPen);
		}
	}
}

// ##### Public Function definitions #####

void AppPaletteInitialize(ScreenBuffer *screenBuffer) {
	_activeBuffer = screenBuffer;

	if (screenBuffer->bitsPerPixel == Bpp3) {
		_redLevels = 4;
	} else {
		_redLevels = 256;
	}
	_redIncrement = (BYTE) (256 / _redLevels);
	_redLevel = 0;

	DrawPalette();
}

void AppPaletteProcessInput(char command) {
	if (command == '+' && _redLevel < (_redLevels - 1)) {
		++_redLevel;
		DrawPalette();
	} else if (command == '-' && _redLevel > 0) {
		--_redLevel;
		DrawPalette();
	}

}

void AppPaletteClose() {
	_activeBuffer = NULL;
}
