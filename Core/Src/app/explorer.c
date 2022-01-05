/*
 * explorer.c
 *
 *  Created on: 20 dic 2021
 *      Author: mnznn
 */

#include <app/explorer.h>
#include <inttypes.h>
#include <intmath.h>
#include <string.h>
#include <stdio.h>
#include <assertion.h>
#include "fatfs.h"
#include <binary.h>
#include <app/bmp.h>
#include <vga/vgascreenbuffer.h>

#define FORMAT_BUFFER_SIZE 120

static FATFS _fsMountData;
static DIR _dirHandle;
static FILINFO _fileInfoHandle;
static FIL _fileHandle;
static Bmp _bmpHandle;

static ScreenBuffer* _screenBuffer;

static UInt32 _fileListSelectedRow = 0;
static FILINFO _fileListSelectedFile;

static BOOL _displayingError;
const char* const FsRootDirectory = "";

char _errorFormatBuffer[FORMAT_BUFFER_SIZE];

/* Forward declaration section */


/// Display the "SD mounting message" on the screen
void DisplayMessage(const ScreenBuffer* screenBuffer, const char* message, UInt32 background);
/// Display a FatFS error on the screen together with the specified description
void DisplayFResultError(const ScreenBuffer* screenBuffer, FRESULT result, const char* description);
/// Display a generic error on the screen with the specified description
void DisplayGenericError(const ScreenBuffer* screenBuffer, const char* description);
/// Draws on the screen the selected BMP file
void DrawSelectedFile();

/* Private section */

void FormatError(FRESULT result) {
    const char* message;

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
        written = sprintf(_errorFormatBuffer, "Unknown error: 0x%08" PRIx32, (UInt32)result);
    else
        written = sprintf(_errorFormatBuffer, "%s", message);

    DebugAssert(written > 0 && written < FORMAT_BUFFER_SIZE);
}

void DisplayFResultError(const ScreenBuffer* screenBuffer, FRESULT result, const char* description) {
    // We flag that we are in error condition. This will prevent any other user command to be processed
    _displayingError = true;

    // Let's setup the pen
    Pen pen = { 0 };
    pen.color.components.A = 0xFF;

    // Retrieve the error code in the _errorFormatBuffer
    FormatError(result);

    // We measure the two strings to center them to the screen and draw the enclosing rectangles
    SizeS descrStrSize;
    ScreenMeasureString(description, &descrStrSize);
    SizeS errorStrSize;
    ScreenMeasureString(_errorFormatBuffer, &errorStrSize);

    // As usual, let's pad a little bit the enclosing rectangle
    descrStrSize.height = (Int16)(descrStrSize.height + 4);
    errorStrSize.height = (Int16)(errorStrSize.height + 4);
    int yPos = (screenBuffer->screenSize.height / 2) - ((descrStrSize.height + errorStrSize.height) / 2);

    // We reset the screen to black
    ScreenClear(screenBuffer, &pen);

    // We calculate the x position for our string rectangles
    int descXPos = (screenBuffer->screenSize.width / 2) - ((descrStrSize.width) / 2);
    int errXPos = (screenBuffer->screenSize.width / 2) - ((errorStrSize.width) / 2);

    // Our message should not go beyond screen borders since we are generating it
    DebugAssert(descXPos > 0 && descXPos < screenBuffer->screenSize.width);
    DebugAssert(errXPos > 0 && errXPos < screenBuffer->screenSize.width);
    DebugAssert(yPos > 0 && yPos < screenBuffer->screenSize.height);

    // We fill the two enclosing rectangle
    PointS point;
    point.x = (Int16)descXPos;
    point.y = (Int16)yPos;
    pen.color.argb = SCREEN_RGB(0xFF, 0, 0);
    ScreenFillRectangle(screenBuffer, point, descrStrSize, &pen);
    point.x = (Int16)errXPos;
    point.y = (Int16)(point.y + descrStrSize.height);
    ScreenFillRectangle(screenBuffer, point, errorStrSize, &pen);

    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);

    // We now draw the two strings
    point.x = (Int16)descXPos;
    point.y = (Int16)(yPos + 2);
    ScreenDrawString(screenBuffer, description, point, &pen);
    point.x = (Int16)errXPos;
    point.y = (Int16)(point.y + descrStrSize.height);
    ScreenDrawString(screenBuffer, _errorFormatBuffer, point, &pen);
}

void DisplayGenericError(const ScreenBuffer* screenBuffer, const char* description) {
    // We flag that we are in error condition. This will prevent any other user command to be processed
    _displayingError = true;

    DisplayMessage(screenBuffer, description, SCREEN_RGB(0xFF, 0x00, 0x00));
}

void DisplayMessage(const ScreenBuffer* screenBuffer, const char* message, UInt32 background) {
    Pen pen = { 0 };
    PointS point = { 0 };

    // Simple message that we want to display

    // First we measure the string to fill a rectangle around it
    SizeS msgSize;
    ScreenMeasureString(message, &msgSize);

    // Let's enlarge a bit the size of the enclosing rectangle
    msgSize.height = (Int16)(msgSize.height + 4);
    // Let's vertically center the rectangle in the screen
    int yPos = (screenBuffer->screenSize.height / 2) - ((msgSize.height) / 2);
    int xPos = (screenBuffer->screenSize.width / 2) - (msgSize.width / 2);

    DebugAssert(xPos >= 0);
    DebugAssert(yPos >= 0);

    // We reset the screen to black
    pen.color.argb = SCREEN_RGB(0, 0, 0);
    ScreenClear(screenBuffer, &pen);

    // We fill the rectangle area that encloses the string
    pen.color.argb = background;
    point.x = (Int16)xPos;
    point.y = (Int16)yPos;
    ScreenFillRectangle(screenBuffer, point, msgSize, &pen);

    // Eventually we draw the string
    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);
    point.y = (Int16)(point.y + 2);
    ScreenDrawString(screenBuffer, message, point, &pen);
}

void DrawSelectedFile() {
    // We try to pen the file
    FRESULT openResult = f_open(&_fileHandle, _fileListSelectedFile.fname, FA_READ | FA_OPEN_EXISTING);
    if (openResult != FR_OK) {
        DisplayFResultError(_screenBuffer, openResult, "Unable to open file");
        return;
    }

    // If opened, we need to read it as a Bitmap
    BmpResult result = BmpReadFromFile(&_fileHandle, &_bmpHandle);
    if (result != BmpResultOk) {
        DisplayGenericError(_screenBuffer, "Unable read file as bitmap");
        goto cleanup;
    }

    result = BmpDisplay(&_bmpHandle, _screenBuffer);
    if (result != BmpResultOk) {
        DisplayGenericError(_screenBuffer, "Unable display bitmap");
    }
    // If we cannot load the BMP, we still have to close the file
cleanup:
    f_close(&_fileHandle);
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
            BYTE* pColor = (BYTE*)&color;

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
    // First thing we have to do is to clean the screen
    Pen pen;
    pen.color.argb = SCREEN_RGB(0, 0, 0);
    ScreenClear(_screenBuffer, &pen);

    // Performance note here: it is not the best here to scan the directory each time
    // but our focus is now on reading "data" from SD and displaying stuff on the VGA
    // We have to enumerate a little list of file, not thousand files directories so we
    // can afford this waste; otherwise a much more complex caching system is needed
    FRESULT openResult = f_opendir(&_dirHandle, FsRootDirectory);
    if (openResult != FR_OK) {
        DisplayFResultError(_screenBuffer, openResult, "Unable to open root dir");
        return;
    }
    // Let's calculate a row using the max character height with the current font
    const int rowPadding = 3;
    int rowSize = ScreenGetCharMaxHeight() + (rowPadding * 2);
    // Let's fix the row text color
    pen.color.argb = SCREEN_RGB(0xb2, 0xdf, 0xdb);

    PointS rowPoint = { 0 };
    rowPoint.x = (Int16)(rowPoint.x + rowPadding); // let's start with a little offset to not draw directly on the border

    // Little performance note: to check if the enumeration is ended, we need to check if fname is not an empty string
    // If we use strlen(), the string is read entirely each time. We can simply check if the first char is not zero
    FRESULT dirReadResult = FR_OK;
    int fileIndex = 0;
    while ((rowPoint.y + rowSize < _screenBuffer->screenSize.height) && // Row in screen bound
        ((dirReadResult = f_readdir(&_dirHandle, &_fileInfoHandle)) == FR_OK) && // No Error
        _fileInfoHandle.fname[0] != '\0') // Enumeration not ended
    {
        if ((_fileInfoHandle.fattrib & AM_DIR) || (_fileInfoHandle.fattrib & AM_SYS) || (_fileInfoHandle.fattrib & AM_HID)) {
            // Not for us
            continue;
        }
        // We want do display only .bmp and .raw files
        // let's ignore case sensitivity for the moment
        if ((!EndsWith(_fileInfoHandle.fname, ".bmp")) &&
        	(!EndsWith(_fileInfoHandle.fname, ".raw"))) continue;

        // Let's draw the name a little bit shifted
        PointS nameDrawPoint = rowPoint;
        nameDrawPoint.x = (Int16)(nameDrawPoint.x + rowPadding);

        if (fileIndex == _fileListSelectedRow) {
            memcpy(&_fileListSelectedFile, &_fileInfoHandle, sizeof(FILINFO));
            UInt32 originalPenColor = pen.color.argb;
            SizeS stringRectSize;
            ScreenMeasureString(_fileInfoHandle.fname, &stringRectSize);

            // We draw the rectangle a little bit larger to correct the string offset
            stringRectSize.width = (Int16)(stringRectSize.width + rowPadding * 2);
            ScreenFillRectangle(_screenBuffer, rowPoint, stringRectSize, &pen);           

            // We draw the string and we reset the original color
            pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);
            ScreenDrawString(_screenBuffer, _fileInfoHandle.fname, nameDrawPoint, &pen);
            pen.color.argb = originalPenColor;
        }
        else {
            // Super simple string draw if the file is not selected
            ScreenDrawString(_screenBuffer, _fileInfoHandle.fname, nameDrawPoint, &pen);
        }
        rowPoint.y = (Int16)(rowPoint.y + rowSize);
        ++fileIndex;
    }

    // If something went wrong, we display the error
    if (dirReadResult != FR_OK) {
        DisplayFResultError(_screenBuffer, dirReadResult, "Enumeration failed");
    }

    // Eventually we close the directory handle
    f_closedir(&_dirHandle);
}

/* Public section */

void ExplorerOpen(ScreenBuffer* screenBuffer) {
    // We reset our state
    _screenBuffer = screenBuffer;
    _displayingError = false;

    // We display the SD message and we clear the selected FILINFO structure
    DisplayMessage(screenBuffer, "Mounting SD card ...", SCREEN_RGB(0x28, 0xB5, 0xF4));
    memset(&_fileListSelectedFile, 0, sizeof(FILINFO));

    // Eventually we mount the SD and we draw the list
    FRESULT mountResult = f_mount(&_fsMountData, FsRootDirectory, 1);
    if (mountResult != FR_OK) {
        DisplayFResultError(screenBuffer, mountResult, "Unable to mount SD CARD");
    }
    else {
        DrawFileList();
    }
}

void ExplorerProcessInput(char command) {
    // User can do nothing when the app is displaying an error
    if (_displayingError)
        return;

    if (command == '+') {
        ++_fileListSelectedRow;
        DrawFileList();
    }
    else if (command == '-') {
        _fileListSelectedRow = MAX(_fileListSelectedRow - 1, 0);
        DrawFileList();
    }
    else if (command == '\b') {
        DrawFileList();
    }
    else if (command == '\r' || command == '\n' || command == ' ') {
        VgaSuspendOutput();
        DrawSelectedFile();
        VgaResumeOutput();
    }

}

void ExplorerClose() {
    // Super simple here: we just need to unmount the file system and then we can exit
    f_mount(NULL, FsRootDirectory, 0);
    _screenBuffer = NULL;
}
