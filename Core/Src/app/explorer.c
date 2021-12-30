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
#include <binary.h>
#include <app/bmp.h>

#define FORMAT_BUFFER_SIZE 120

static FATFS _fsMountData;
static DIR _dirHandle;
static FILINFO _fileInfoHandle;
static FIL _fileHandle;
static Bmp _bmpHandle;

static ScreenBuffer *_screenBuffer;

static UInt32 _fileListSelectedRow = 0;
static FILINFO _fileListSelectedFile;

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

void DisplayStartupMessage(const ScreenBuffer *screenBuffer) {
	Pen pen = { 0 };

	const char *message = "Mounting SD card ...";

	SizeS msgSize;
	ScreenMeasureString(message, &msgSize);

	msgSize.height += 4;
	UInt16 vStartingPoint = (screenBuffer->screenSize.height / 2) - ((msgSize.height) / 2);

	// We reset the screen to black
	pen.color.argb = 0xFF000000;
	ScreenClear(screenBuffer, &pen);

	UInt16 xPos = (screenBuffer->screenSize.width / 2) - (msgSize.width / 2);

	pen.color.argb = 0xFF28B5F4;
	ScreenDrawRectangle(screenBuffer, (PointS ) { xPos, vStartingPoint }, msgSize, &pen);

	pen.color.argb = 0xFFFFFFFF;
	vStartingPoint += 2;
	ScreenDrawString(screenBuffer, message, (PointS ) { xPos, vStartingPoint }, &pen);
}

void DrawSelectedFile() {
	FRESULT openResult = f_open(&_fileHandle, _fileListSelectedFile.fname, FA_READ | FA_OPEN_EXISTING);
	if (openResult != FR_OK) {
		DisplayFResultError(_screenBuffer, openResult, "Unable to open file");
		return;
	}

	BmpResult result = BmpReadFromFile(&_fileHandle, &_bmpHandle);
	if (result != BmpResultOk) {
		goto cleanup;
	}

	BmpDisplay(&_bmpHandle, _screenBuffer);

	cleanup: f_close(&_fileHandle);
}

void DrawSelectedRawFile() {
	FRESULT openResult = f_open(&_fileHandle, _fileListSelectedFile.fname, FA_READ | FA_OPEN_EXISTING);
	if (openResult != FR_OK) {
		DisplayFResultError(_screenBuffer, openResult, "Unable to open file");
		return;
	}

	Pen pen;
	pen.color.argb = SCREEN_RGB(0, 0, 0);
	ScreenClear(_screenBuffer, &pen);

	UINT read;
	pen.color.components.A = 0xFF;
	PointS point;
	for (size_t line = 0; line < 300; line++) {
		point.y = line;

		for (size_t j = 0; j < 400; j++) {
			UInt32 color = 0;
			BYTE *pColor = (BYTE*) &color;

			FRESULT readResult = f_read(&_fileHandle, pColor, 3, &read);
			DebugAssert(read == 3);

			pen.color.components.R = pColor[0];
			pen.color.components.G = pColor[1];
			pen.color.components.B = pColor[2];
			if (readResult != FR_OK) {
				Error_Handler();
			}

			point.x = j;
			ScreenDrawPixel(_screenBuffer, point, &pen);
		}
	}

	f_close(&_fileHandle);
}

void DrawFileList() {
	Pen pen;
	pen.color.argb = SCREEN_RGB(0, 0, 0);

	ScreenClear(_screenBuffer, &pen);

	FRESULT openResult = f_opendir(&_dirHandle, FsRootDirectory);
	if (openResult != FR_OK) {
		DisplayFResultError(_screenBuffer, openResult, "Unable to open root dir");
		return;
	}
	const BYTE rowPadding = 3;
	UInt32 rowSize = ScreenGetCharMaxHeight() + (rowPadding * 2);

	pen.color.argb = SCREEN_RGB(0xb2, 0xdf, 0xdb);

	PointS rowPoint = { 0 };

	rowPoint.x += rowPadding; // let's start with a little offset to not draw directly on the border
	FRESULT dirReadResult = FR_OK;
	int fileIndex = 0;
	while ((rowPoint.y + rowSize < _screenBuffer->screenSize.height) && // Row in screen bound
			((dirReadResult = f_readdir(&_dirHandle, &_fileInfoHandle)) == FR_OK) && // No Error
			strlen(_fileInfoHandle.fname) > 0) // Enumeration not ended
	{
		if ((_fileInfoHandle.fattrib & AM_DIR) || (_fileInfoHandle.fattrib & AM_SYS) || (_fileInfoHandle.fattrib & AM_HID)) {
			// Not for us
			continue;
		}

		PointS nameDrawPoint = rowPoint;
		nameDrawPoint.x += rowPadding;

		if (fileIndex == _fileListSelectedRow) {
			memcpy(&_fileListSelectedFile, &_fileInfoHandle, sizeof(FILINFO));
			UInt32 originalPenColor = pen.color.argb;
			SizeS stringRectSize;
			ScreenMeasureString(_fileInfoHandle.fname, &stringRectSize);

			ScreenDrawRectangle(_screenBuffer, rowPoint, stringRectSize, &pen);
			pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);

			ScreenDrawString(_screenBuffer, _fileInfoHandle.fname, nameDrawPoint, &pen);
			pen.color.argb = originalPenColor;
		} else {

			ScreenDrawString(_screenBuffer, _fileInfoHandle.fname, nameDrawPoint, &pen);
		}
		rowPoint.y += rowSize;
		++fileIndex;
	}

	f_closedir(&_dirHandle);
}

/* Public section */

void ExplorerOpen(const ScreenBuffer *screenBuffer) {
	_screenBuffer = screenBuffer;
	_displayingError = false;

	DisplayStartupMessage(screenBuffer);
	memset(&_fileListSelectedFile, 0, sizeof(FILINFO));

	FRESULT mountResult = f_mount(&_fsMountData, FsRootDirectory, 1);
	if (mountResult != FR_OK) {
		DisplayFResultError(screenBuffer, mountResult, "Unable to mount SD CARD");
		return;
	}

	DrawFileList();
}

void ExplorerProcessInput(char command) {
	// User can do nothing when the app is displayin an error
	if (_displayingError)
		return;

	if (command == '+') {
		++_fileListSelectedRow;
		DrawFileList();
	} else if (command == '-') {
		--_fileListSelectedRow;
		DrawFileList();
	} else if (command == '\b') {
		DrawFileList();
	} else if (command == '\r' || command == '\n' || command == ' ') {
		DrawSelectedFile();
	}

}

void ExplorerClose() {
	f_mount(NULL, FsRootDirectory, 0);
	_screenBuffer = NULL;
}
