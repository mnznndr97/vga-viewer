/*
 * Header for the BMP image manipulation.
 * The unit only handles the loading of a basic BMP file that is saved without any compression
 * in the current Windows format
 * The BMP file can also be displayed on the screen via the BmpDisplay() function
 *
 * The windows BMP specification can be found at https://docs.microsoft.com/en-us/windows/win32/gdi/about-bitmaps
 *
 * The API does NOT define a BmpXXXClose() method since no memory is allocated for the public 
 * Bmp struct
 * 
 *  Created on: Dic 30, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_APP_BMP_H_
#define INC_APP_BMP_H_

#include <typedefs.h>
#include <screen/screen.h>
#include "fatfs.h"

/// Bitmap operation result
typedef enum _BmpResult {
    BmpResultOk, BmpResultFailure
} BmpResult;

/// Enums of supported bitmap identifiers
typedef enum _BmpIdentifier {
    /// Windows Bitmap Identifier
    BmpIdentifierBM = 0x4D42
} BmpIdentifier;

/// BMP file description struct
typedef struct _Bmp {
    /// Handle to the BMP file
    FIL* fileHandle;
    /// Type of the BMP
    BmpIdentifier identifier;
    /// Offset in bytes inside the file for the actual data
    UInt32 dataOffset;
    /// Width of the bitmap
    UInt32 width;
    /// Height of the bitmap
    UInt32 height;
    /// Bitmap BPP
    UInt16 bitCount;
    /// Row size in bytes, which is the width padded with zero to be a multiple of DWORD
    UInt32 rowByteSize;
} Bmp;

/// Tries to read a BMP from a already opened file handle
/// @param file File handle
/// @param pBmp Destination pointer for the opened BMP
/// @return Status of the operation
BmpResult BmpReadFromFile(FIL* file, Bmp* pBmp);
/// Display a BMP image to the entire screen
/// @param cpBmp Pointer to the BMP description
/// @param cpScreenBuffer Destination screen buffer
/// @return Status of the operation
BmpResult BmpDisplay(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer);
#endif /* INC_APP_BMP_H_ */
