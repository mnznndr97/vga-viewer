/*
 * explorer.c
 *
 *  Created on: 20 dic 2021
 *      Author: mnznn
 */

#include <app/explorer.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <assertion.h>
#include "fatfs.h"

#define FORMAT_BUFFER_SIZE 120

static FATFS _fsMountData;
static FIL _fileHandle;

static BOOL _displayingError;
const char *const FsRootDirectory = "";

char errorFormatBuffer[FORMAT_BUFFER_SIZE];

/* Private section */

void FormatError(FRESULT result) {
	const char *message;

	switch (result) {
	case FR_OK:
		message = "OK";
		break;
	case FR_DISK_ERR:
		message = "I/O Error";
		break;
	case FR_INT_ERR:
		message = "Assertion Error";
		break;
	case FR_NOT_READY:
		message = "Physical drive not ready";
		break;
	case FR_NO_FILE:
	case FR_NO_PATH:
		message = "Path not found";
		break;
	case FR_INVALID_NAME:
		message = "Invalid path";
		break;
	case FR_DENIED:
		message = "Access denied";
		break;
	case FR_EXIST:
		message = "Name collision";
		break;
	case FR_INVALID_OBJECT:
		message = "Invalid object";
		break;
	case FR_WRITE_PROTECTED:
		message = "Write protected";
		break;
	case FR_INVALID_DRIVE:
		message = "Invalid drive";
		break;
	case FR_NOT_ENABLED:
		message = "Logical drive not mounted";
		break;
	case FR_NO_FILESYSTEM:
		message = "FAT volume could not be found";
		break;
	case FR_MKFS_ABORTED:
		message = "MKFS aborted";
		break;
	case FR_TIMEOUT:
		message = "Timeout";
		break;
	case FR_LOCKED:
		message = "Locked";
		break;
	case FR_NOT_ENOUGH_CORE:
		message = "Memory overflow";
		break;
	case FR_TOO_MANY_OPEN_FILES:
		message = "Too many objects";
		break;
	case FR_INVALID_PARAMETER:
		message = "Invalid parameter";
		break;
	default:
		message = NULL;
		break;
	}

	int written;
	if (message == NULL)
		written = sprintf(errorFormatBuffer, "Unknown error: 0x%08" PRIx32, (UInt32) result);
	else
		written = sprintf(errorFormatBuffer, "%s", message);

	DebugAssert(written > 0 && written < FORMAT_BUFFER_SIZE);
}

void DisplayFResultError(const ScreenBuffer *screenBuffer, FRESULT result, const char *description) {
	_displayingError = true;
	Pen pen = { 0 };
	pen.color.argb = 0xFF000000;

	FormatError(result);

	SizeS descrStrSize;
	ScreenMeasureString(description, &descrStrSize);
	SizeS errorStrSize;
	ScreenMeasureString(errorFormatBuffer, &errorStrSize);

	descrStrSize.height += 4;
	errorStrSize.height += 4;
	UInt16 vStartingPoint = (screenBuffer->screenSize.height / 2) - ((descrStrSize.height + errorStrSize.height) / 2);

	// We reset the screen to black
	ScreenClear(screenBuffer, &pen);

	UInt16 descXPos = (screenBuffer->screenSize.width / 2) - ((descrStrSize.width) / 2);
	UInt16 errXPos = (screenBuffer->screenSize.width / 2) - ((errorStrSize.width) / 2);

	pen.color.argb = 0xFFFF0000;
	ScreenDrawRectangle(screenBuffer, (PointS ) { descXPos, vStartingPoint }, descrStrSize, &pen);
	vStartingPoint += (descrStrSize.height);
	ScreenDrawRectangle(screenBuffer, (PointS ) { errXPos, vStartingPoint }, errorStrSize, &pen);

	pen.color.argb = 0xFFFFFFFF;

	vStartingPoint -= (descrStrSize.height - 2);
	ScreenDrawString(screenBuffer, description, (PointS ) { descXPos, vStartingPoint }, &pen);
	vStartingPoint += descrStrSize.height;
	ScreenDrawString(screenBuffer, errorFormatBuffer, (PointS ) { errXPos, vStartingPoint }, &pen);
}

void DisplayStartupMessage(const ScreenBuffer* screenBuffer) {
    _displayingError = true;
    Pen pen = { 0 };

    const char* message = "Mounting SD card ...";

    SizeS msgSize;
    ScreenMeasureString(message, &msgSize);
    
    msgSize.height += 4;
    UInt16 vStartingPoint = (screenBuffer->screenSize.height / 2) - ((msgSize.height) / 2);

    // We reset the screen to black
    pen.color.argb = 0xFF000000;
    ScreenClear(screenBuffer, &pen);

    UInt16 xPos = (screenBuffer->screenSize.width / 2) - (msgSize.width / 2);
    
    pen.color.argb = 0xFF28B5F4;
    ScreenDrawRectangle(screenBuffer, (PointS) { xPos, vStartingPoint }, msgSize, & pen);
    
    pen.color.argb = 0xFFFFFFFF;
    vStartingPoint += 2;
    ScreenDrawString(screenBuffer, message, (PointS) { xPos, vStartingPoint }, & pen);
}

/* Public section */

void ExplorerOpen(const ScreenBuffer *screenBuffer) {
	_displayingError = false;

    DisplayStartupMessage(screenBuffer);

	FRESULT mountResult = f_mount(&_fsMountData, FsRootDirectory, 1);
	if (mountResult != FR_OK) {
		DisplayFResultError(screenBuffer, mountResult, "Unable to mount SD CARD");
		return;
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
	if (_displayingError)
		return;

}

void ExplorerClose() {
	f_mount(NULL, FsRootDirectory, 0);
}
