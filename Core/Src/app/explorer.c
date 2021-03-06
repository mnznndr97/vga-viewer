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

 /// FatFs relative data for the mounted filesystem
static FATFS _fsMountData;
/// Static dir handle used by the DrawFileList() and CacheFileCount() functions.
/// @remarks Declared here as a global field to avoid using too much stack with the fs objects,
/// due to the limited space of FreeRTOS tasks
static DIR _dirHandle;
/// Static enumerated file handle used by as f_readdir() parameter
/// @remarks Declared here as a global field to avoid using too much stack with the fs objects,
/// due to the limited space of FreeRTOS tasks
static FILINFO _fileInfoHandle;
/// Static opened file handle used as f_open() parameter in drawing methods
/// @remarks Declared here as a global field to avoid using too much stack with the fs objects,
/// due to the limited space of FreeRTOS tasks
static FIL _fileHandle;
/// Static opened BMP handle used when opening *.bmp files in drawing methods
/// @remarks Declared here as a global field to avoid using too much stack,
/// due to the limited space of FreeRTOS tasks
static Bmp _bmpHandle;
/// @brief Active screen buffer reference
static ScreenBuffer* _screenBuffer;
/// Cached number of file in the main fs directory
static int _fileListCountCache = 0;
/// Index of the selected file in the directory
static int _fileListSelectedRow = 0;
/// FILINFO relative to the selected
static FILINFO _fileListSelectedFile;
/// Flag to indicates that the applciation is displaying an error and no user commands should be processed
static BOOL _displayingError;
/// Cached size of the application title box
static int _titleBoxHeight;
/// Flag that indicates that the output must be suspended when drawing an image
BOOL _suspendOutput = 0;
/// Static buffer for error string formatting
char _errorFormatBuffer[FORMAT_BUFFER_SIZE];

/// Root directory path
const char* const FsRootDirectory = "";

/* Forward declaration section */

/// Counts the valid file in our directory. Just to perform a check on the index
static bool CacheFileCount();
/// Writes the error relative to "result" in the _errorFormatBuffer
/// @param result FatFs error
static void FormatError(FRESULT result);
/// Display the specified single line message on the screen
static void DisplayMessage(const ScreenBuffer* screenBuffer, const char* message, UInt32 background);
/// Display a FatFS error on the screen together with the specified description
static void DisplayFResultError(const ScreenBuffer* screenBuffer, FRESULT result, const char* description);
/// Display a generic error on the screen with the specified description
static void DisplayGenericError(const ScreenBuffer* screenBuffer, const char* description);
/// Draws on the screen the selected file
static void DrawSelectedFile();
/// Draws on the screen the selected BMP file
static void DrawSelectedBmpFile();
/// Draws the file list in the root directory on the screen
static void DrawFileList();
/// Draws on the screen the selected RAW file
static void DrawSelectedRawFile();
/// Draws the tile of the application on the screen
static void DrawApplicationTitle();
/// Filter to check that a file in our list is valid
/// @return False if the file need to be ignored, true otherwhise
static bool FilterValidFile(const FILINFO* pInfo);

/* Private section */

bool CacheFileCount() {
    _fileListCountCache = 0;
    FRESULT openResult = f_opendir(&_dirHandle, FsRootDirectory);
    if (openResult != FR_OK) {
        DisplayFResultError(_screenBuffer, openResult, "Unable to open root dir");
        return false;
    }

    // Little performance note: to check if the enumeration is ended, we need to check if fname is not an empty string
    // If we use strlen(), the string is read entirely each time. We can simply check if the first char is not zero
    FRESULT dirReadResult = FR_OK;
    while (((dirReadResult = f_readdir(&_dirHandle, &_fileInfoHandle)) == FR_OK) && // No Error
        _fileInfoHandle.fname[0] != '\0') // Enumeration not ended
    {
        if (!FilterValidFile(&_fileInfoHandle)) {
            // Not for us
            continue;
        }

        ++_fileListCountCache;
    }

    bool result = true;
    // If something went wrong, we display the error
    if (dirReadResult != FR_OK) {
        DisplayFResultError(_screenBuffer, dirReadResult, "Enumeration failed");
        result = false;
    }

    // Eventually we close the directory handle
    f_closedir(&_dirHandle);
    return result;
}

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
    const int padding = 2;
    Pen pen = { 0 };
    PointS point = { 0 };

    // First we measure the string to fill a rectangle around it
    SizeS msgSize;
    ScreenMeasureString(message, &msgSize);

    // Let's enlarge a bit the size of the enclosing rectangle
    msgSize.height = (Int16)(msgSize.height + (padding * 2));
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
    point.y = (Int16)(point.y + padding);
    ScreenDrawString(screenBuffer, message, point, &pen);
}

void DrawApplicationTitle() {
    const char* title = "Explorer";
    const int padding = 3;

    // We measure the string to get the height
    SizeS titleStrBox = { 0 };
    ScreenMeasureString(title, &titleStrBox);

    // We adjust the height with some padding and calculate the title blox filling the screen in the
    // X axis
    _titleBoxHeight = titleStrBox.height + (padding * 2);
    SizeS titleBox = titleStrBox;
    titleBox.height = (Int16)_titleBoxHeight;
    titleBox.width = _screenBuffer->screenSize.width;

    // We fill the title rectangle
    Pen pen = { 0 };
    pen.color.argb = SCREEN_RGB(0x53, 0x6D, 0xFE);
    PointS point = { 0 };
    ScreenFillRectangle(_screenBuffer, point, titleBox, &pen);

    // Eventually we draw the string inside the rectangle
    point.x = (Int16)((_screenBuffer->screenSize.width / 2) - (titleStrBox.width / 2));
    point.y = (Int16)padding;
    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);
    ScreenDrawString(_screenBuffer, title, point, &pen);
}

void DrawSelectedFile() {
    if (EndsWith(_fileListSelectedFile.fname, ".raw"))
    {
        DrawSelectedRawFile();
    }
    else
    {
        DrawSelectedBmpFile();
    }
}

void DrawSelectedBmpFile() {
    // We try to pen the file
    FRESULT openResult = f_open(&_fileHandle, _fileListSelectedFile.fname, FA_READ | FA_OPEN_EXISTING);
    if (openResult != FR_OK) {
        DisplayFResultError(_screenBuffer, openResult, "Unable to open file");
        return;
    }

    // If opened, we need to read it as a Bitmap
    BmpResult result = BmpReadFromFile(&_fileHandle, &_bmpHandle);
    if (result != BmpResultOk) {
        DisplayGenericError(_screenBuffer, "Unable to read file as bitmap");
        // We need to close the file handle in any case
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
    // We try to pen the file
    FRESULT openResult = f_open(&_fileHandle, _fileListSelectedFile.fname, FA_READ | FA_OPEN_EXISTING);
    if (openResult != FR_OK) {
        DisplayFResultError(_screenBuffer, openResult, "Unable to open raw file");
        return;
    }

    UINT read;
    Pen pen = { 0 };
    pen.color.components.A = 0xFF;
    PointS point;
    // Raw files are similar to bitmap but with no headers, word alignment, no reverse scanline
    for (int line = 0; line < _screenBuffer->screenSize.height; line++) {
        point.y = (Int16)line;

        for (int x = 0; x < _screenBuffer->screenSize.width; x++) {
            UInt32 color = 0;

            // We read the big endian RGB code into our local color variable
            FRESULT readResult = f_read(&_fileHandle, &color, 3, &read);
            if (readResult != FR_OK) {
                DisplayGenericError(_screenBuffer, "Unable to read raw file");
                goto cleanup;
            }
            DebugAssert(read == 3);

            // Raw components are not in little endian
            pen.color.components.R = MASKI2BYTE(color);
            pen.color.components.G = MASKI2BYTE(color >> 8);
            pen.color.components.B = MASKI2BYTE(color >> 16);
            
            point.x = (Int16)x;
            ScreenDrawPixel(_screenBuffer, point, &pen);
        }
    }

    // If we cannot load the RAW, we still have to close the file
cleanup:
    f_close(&_fileHandle);
}

void DrawFileList() {
    // First thing we have to do is to clean the screen
    Pen pen;
    pen.color.argb = SCREEN_RGB(0, 0, 0);
    ScreenClear(_screenBuffer, &pen);

    DrawApplicationTitle();

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
    rowPoint.y = (Int16)(_titleBoxHeight + rowPadding);

    int rowsInPage = (_screenBuffer->screenSize.height - _titleBoxHeight) / rowSize;
    int pageOffset = _fileListSelectedRow / rowsInPage;
    int rowsToSkip = pageOffset * rowsInPage;

    // Little performance note: to check if the enumeration is ended, we need to check if fname is not an empty string
    // If we use strlen(), the string is read entirely each time. We can simply check if the first char is not zero
    FRESULT dirReadResult = FR_OK;
    int fileIndex = 0;
    while ((rowPoint.y + rowSize < _screenBuffer->screenSize.height) && // Row in screen bound
        ((dirReadResult = f_readdir(&_dirHandle, &_fileInfoHandle)) == FR_OK) && // No Error
        _fileInfoHandle.fname[0] != '\0') // Enumeration not ended
    {
        if (!FilterValidFile(&_fileInfoHandle)) {
            // Not for us
            continue;
        }

        // We need to skip the files that are in the "pages" before the one we are displaying
        if (rowsToSkip-- > 0) {
            ++fileIndex;
            continue;
        }

        // Let's draw the name a little bit shifted
        PointS nameDrawPoint = rowPoint;
        nameDrawPoint.x = (Int16)(nameDrawPoint.x + rowPadding);

        if (fileIndex == _fileListSelectedRow) {
            // We copy the selected file info to a static object so it can be used in the 
            // application command subroutines
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

static bool FilterValidFile(const FILINFO* pInfo) {
    if ((_fileInfoHandle.fattrib & AM_DIR) || (_fileInfoHandle.fattrib & AM_SYS) || (_fileInfoHandle.fattrib & AM_HID)) {
        // Not for us
        return false;
    }
    // We want do display only .bmp and .raw files
    // let's ignore case sensitivity for the moment
    return EndsWith(_fileInfoHandle.fname, ".bmp") || EndsWith(_fileInfoHandle.fname, ".raw");
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
    else if (CacheFileCount()) {
        DrawFileList();
    }
}

void ExplorerProcessInput(char command) {
    // User can do nothing when the app is displaying an error
    if (_displayingError)
        return;

    if (command == '+' && (_fileListSelectedRow + 1) < _fileListCountCache) {
        ++_fileListSelectedRow;
        DrawFileList();
    }
    else if (command == '-' && _fileListSelectedRow > 0) {
        _fileListSelectedRow = _fileListSelectedRow - 1;
        DrawFileList();
    }
    else if (command == 'e') {
        // We redraw the file list. this is necessary to avoid exiting and re-opening the applciation
        DrawFileList();
    }
    else if (command == 'o') {
        // Toggles output suspension
        _suspendOutput = !_suspendOutput;
        printf("Output suspend: %s\r\n", _suspendOutput ? "enabled" : "disabled");
    }
    else if (command == '\r' || command == '\n' || command == ' ') {
        // Let's handle the Enter or Space key to draw a file
        if (_suspendOutput)
            VgaSuspendOutput();
        DrawSelectedFile();
        if (_suspendOutput)
            VgaResumeOutput();
    }

}

void ExplorerClose() {
    // Super simple here: we just need to unmount the file system and then we can exit
    f_mount(NULL, FsRootDirectory, 0);
    _screenBuffer = NULL;
    _fileListCountCache = 0;
    _fileListSelectedRow = 0;
}
