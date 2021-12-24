/*
 * explorer.c
 *
 *  Created on: 20 dic 2021
 *      Author: mnznn
 */

#include <app/explorer.h>
#include <assertion.h>
#include "fatfs.h"

static FATFS _fsMountData;
static FIL _fileHandle;

static BOOL _displayingError;
const char *const FsRootDirectory = "";

/* Private section */

void DisplayFResultError(const ScreenBuffer* screenBuffer, FRESULT result, const char* description) {
    _displayingError = true;
    Pen pen;
    pen.color.argb = 0xFF000000;

    // We reset the screen to black
    ScreenClear(screenBuffer, &pen);

    SizeS stringSize;
    ScreenMeasureString(description, &stringSize);

    pen.color.argb = 0xFFFF0000;
    ScreenDrawRectangle(screenBuffer, (PointS) { 0, 150 }, stringSize, & pen);

    pen.color.argb = 0xFFFFFFFF;
    ScreenDrawString(screenBuffer, description, (PointS) { 0, 150 }, &pen);
}

/* Public section */

void ExplorerOpen(const ScreenBuffer *screenBuffer) {
    _displayingError = false;

    DisplayFResultError(screenBuffer, FR_DISK_ERR, "Unable to mount SD CARD");
    return;

	FRESULT mountResult = f_mount(&_fsMountData, FsRootDirectory, 1);
	if (mountResult != FR_OK) {
		
	}

	FRESULT openResult = f_open(&_fileHandle, "Natale2_43.raw", FA_READ | FA_OPEN_EXISTING);
	if (openResult != FR_OK) {
		Error_Handler();
	}

	Pen pen;
	BYTE rgb[3];
	UINT read;

	pen.color.components.A = 0xFF;
	PointS point;
	for (size_t line = 0; line < 300; line++) {
		point.y = line;

		for (size_t j = 0; j < 400; j++) {
			FRESULT readResult = f_read(&_fileHandle, &rgb, 3, &read);
			DebugAssert(read == 3);
			if (readResult != FR_OK) {
				Error_Handler();
			}

			pen.color.components.R = rgb[0];
			pen.color.components.G = rgb[1];
			pen.color.components.B = rgb[2];

			point.x = j;
			ScreenDrawPixel(screenBuffer, point, &pen);
		}
	}

	f_close(&_fileHandle);
}

void ExplorerProcessInput(char command) {
    // User can do nothing when the app is displayin an error
    if (_displayingError) return;

}

void ExplorerClose() {
	f_mount(NULL, FsRootDirectory, 0);
}
