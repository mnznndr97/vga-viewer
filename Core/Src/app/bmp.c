#include <app/bmp.h>
#include <assertion.h>

/// The BITMAPFILEHEADER structure contains information about the type, size, and layout of a file
/// that contains a DIB.
/// @remarks A BITMAPINFO or BITMAPCOREINFO structure immediately follows the BITMAPFILEHEADER structure
/// in the DIB file. For more information, see Bitmap Storage (https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-storage)
/// As stated in wingdi.h, this structure is 2-byte padded
/// NB: don't know how to differentiate between DIB that have the INFO or CORINFO header
typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} __attribute__((aligned(2), packed)) BITMAPFILEHEADER, * PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, * PBITMAPINFOHEADER;

#define DIB_OFFSET 14
#define BI_RGB 0L

/// Validates the bitmap identifier in the Bmp description
static BmpResult ValidateIdentifier(const Bmp* pBmp);

/// Reads the image data offset from the BMP file in the Bmp description
/// @return Status of the operation
static BmpResult ReadBufferOffset(Bmp* pBmp);
/// Display a bitmap on the screen that have the exact resolution of the destination frame buffer
static BmpResult FastDisplayBitmap(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer);
/// Display a bitmap on the screen applying a resize algorithm
static BmpResult SlowDisplayBitmap(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer);

BmpResult FastDisplayBitmap(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer) {
    // We seek to the data section of the BMP
    FRESULT result = f_lseek(cpBmp->fileHandle, cpBmp->dataOffset);
    if (result != FR_OK) {
        return BmpResultFailure;
    }

    Pen pen = { 0 };
    pen.color.components.A = 0xFF;

    PointS point = { 0 };
    // From our beloved MSDN https://docs.microsoft.com/en-us/windows/win32/gdi/about-bitmaps
    // The bitmap scanline are stored in reverse order
    UInt32 currentRowPos = f_tell(cpBmp->fileHandle);
    for (int row = cpScreenBuffer->screenSize.height - 1; row >= 0; row--) {
        point.y = (Int16)row;

        // We seek to the current row pointer
        result = f_lseek(cpBmp->fileHandle, currentRowPos);
        if (result != FR_OK) {
            return BmpResultFailure;
        }

        // Loop for each pixel
        for (int col = 0; col < cpScreenBuffer->screenSize.width; col++) {
            UINT read;

            // We directly read the 3 components (that are stored in "little-endian")
            // directly into the pen color
            // Alpha component should remain untouched since we are reading only
            //  3 bytes

            result = f_read(cpBmp->fileHandle, &pen.color.argb, 3, &read);
            if (result != FR_OK) {
                return BmpResultFailure;
            }
            point.x = (Int16)col;
            ScreenDrawPixel(cpScreenBuffer, point, &pen);
        }

        currentRowPos += cpBmp->rowByteSize;
    }
    return BmpResultOk;
}

BmpResult GetPixelNN(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer, PointS pixelPt, float invScaleX, float invScaleY, UInt32* color) {
    // Algorithm from https://towardsdatascience.com/image-processing-image-scaling-algorithms-ae29aaa6b36c
    // We only use the inverse scaling factor calculation to deal with multiplications here

    int nearestX = (int)((float)pixelPt.x * invScaleX);
    int nearestY = (int)((float)pixelPt.y * invScaleY);

    // Let's ignore some possible border pixel if the image has a strange resolution
    if (nearestX < 0 || nearestX >= cpBmp->width) return BmpResultOk;
    if (nearestY < 0 || nearestY >= cpBmp->height) return BmpResultOk;

    int scanLineIndex = (int)cpBmp->height - nearestY;
    size_t pixelOffset = (size_t)((scanLineIndex * (int)cpBmp->rowByteSize) + (nearestX * 3));
    // We seek to the data section of the BMP
    FRESULT result = f_lseek(cpBmp->fileHandle, cpBmp->dataOffset + pixelOffset);
    if (result != FR_OK) {
        return BmpResultFailure;
    }

    UINT read = 0;
    result = f_read(cpBmp->fileHandle, color, 3, &read);
    if (result != FR_OK) {
        return BmpResultFailure;
    }

    return BmpResultOk;
}

BmpResult SlowDisplayBitmap(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer) {
    float invScaleX = (float)cpBmp->width / (float)cpScreenBuffer->screenSize.width;
    float invScaleY = (float)cpBmp->height / (float)cpScreenBuffer->screenSize.height;

    PointS pixelPt = { 0 };
    Pen pen = { 0 };
    pen.color.components.A = 0xFF;

    DebugWriteChar('n');
    // Scanlines are stored in reverse order in out Bmp
    for (int y = cpScreenBuffer->screenSize.height - 1; y >= 0; y--)
    {
        pixelPt.y = (Int16)y;
        for (int x = 0; x < cpScreenBuffer->screenSize.width; x++)
        {
            pixelPt.x = (Int16)x;
            BmpResult result = GetPixelNN(cpBmp, cpScreenBuffer, pixelPt, invScaleX, invScaleY, &pen.color.argb);
            if (result != BmpResultOk) {
                // Error: let's interrupt the drawing loop
                return result;
            }

            ScreenDrawPixel(cpScreenBuffer, pixelPt, &pen);
        }
    }
    DebugWriteChar('N');

    return BmpResultOk;
}

BmpResult ReadBufferOffset(Bmp* pBmp) {
    // First we seek to the correct file point
    FRESULT result = f_lseek(pBmp->fileHandle, offsetof(BITMAPFILEHEADER, bfOffBits));
    if (result != FR_OK) {
        // Cannot seek. Some I/O problem?
        return BmpResultFailure;
    }

    // We simply read the data offset field (already stored in little-endian)
    UINT read;
    result = f_read(pBmp->fileHandle, &pBmp->dataOffset, sizeof(DWORD), &read);
    if (result != FR_OK) {
        return BmpResultFailure;
    }
    DebugAssert(read == sizeof(DWORD));
    return BmpResultOk;
}

BmpResult ReadWindowsBitmapInfoHeader(Bmp* pBmp) {
    // Wikipedia link: https://en.wikipedia.org/wiki/BMP_file_format (not updated)
    // Main document: https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-storage
    // A bitmap file is composed of a BITMAPFILEHEADER structure followed by a BITMAPINFOHEADER

    // NB: Wikipedia is referring to the old BITMAPCOREHEADER structure in the DIB description
    // but, as stated in https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-header-types,
    // the type has been deprecated
    // So let's focus on current windows implementation

    FRESULT result = f_lseek(pBmp->fileHandle, DIB_OFFSET + offsetof(BITMAPINFOHEADER, biWidth));
    if (result != FR_OK) {
        return BmpResultFailure;
    }

    // We only care for the size and bpp at the moment
    UINT read;
    result = f_read(pBmp->fileHandle, &pBmp->width, sizeof(DWORD), &read);
    if (result != FR_OK) {
        return BmpResultFailure;
    }
    if (result != FR_OK) {
        return BmpResultFailure;
    }
    result = f_read(pBmp->fileHandle, &pBmp->height, sizeof(DWORD), &read);
    if (result != FR_OK) {
        return BmpResultFailure;
    }

    // We read the bit count
    result = f_lseek(pBmp->fileHandle, DIB_OFFSET + offsetof(BITMAPINFOHEADER, biBitCount));
    if (result != FR_OK) {
        return BmpResultFailure;
    }
    result = f_read(pBmp->fileHandle, &pBmp->bitCount, sizeof(WORD), &read);
    if (result != FR_OK) {
        return BmpResultFailure;
    }

    // Let's check image is not compressed
    DWORD compression;
    result = f_read(pBmp->fileHandle, &compression, sizeof(compression), &read);
    if (result != FR_OK || compression != BI_RGB) {
        return BmpResultFailure;
    }

    DWORD clrUsed;
    result = f_lseek(pBmp->fileHandle, DIB_OFFSET + offsetof(BITMAPINFOHEADER, biClrUsed));
    if (result != FR_OK) {
        return BmpResultFailure;
    }
    result = f_read(pBmp->fileHandle, &clrUsed, sizeof(clrUsed), &read);
    if (result != FR_OK || clrUsed != 0) {
        return BmpResultFailure;
    }

    // Let's precalculate the row size
    pBmp->rowByteSize = ((pBmp->bitCount * pBmp->width) + 31) / 32; // DWord size
    pBmp->rowByteSize *= 4;

    return BmpResultOk;
}

BmpResult ValidateIdentifier(const Bmp* pBmp) {
    if (pBmp->identifier != BmpIdentifierBM)
        return BmpResultFailure;
    return BmpResultOk;
}

/* Public section */

BmpResult BmpReadFromFile(FIL* file, Bmp* pBmp) {
    static_assert(sizeof(BmpIdentifier) == sizeof(WORD));

    if (file == NULL)
        return BmpResultFailure;
    if (pBmp == NULL)
        return BmpResultFailure;

    // First thing to do: read and validate the bitmap identifier
    // We support only the Windows header, which is BM
    UINT read;
    FRESULT result = f_read(file, &pBmp->identifier, sizeof(BmpIdentifier), &read);
    if (result != FR_OK) {
        // Cannot read from the file. We cannot exit
        return BmpResultFailure;
    }
    DebugAssert(read == sizeof(BmpIdentifier));
    // Let' s validate the header
    BmpResult bmpResult;
    if ((bmpResult = ValidateIdentifier(pBmp)) != BmpResultOk) {
        return bmpResult;
    }

    // Here header is valid. Let's Immediately register the file handle in the destination struct
    // Maybe it's not the best for security, etc but now the focus is not on this
    pBmp->fileHandle = file;

    // After the identifier, we have to read the image data offset, which is still a field of the header
    // (the main header is shared by all the strange BMP implementation)
    if ((bmpResult = ReadBufferOffset(pBmp)) != BmpResultOk) {
        return bmpResult;
    }

    // We read the header information that we need 
    if (pBmp->identifier == BmpIdentifierBM) {
        if ((bmpResult = ReadWindowsBitmapInfoHeader(pBmp)) != BmpResultOk) {
            // Some problem while reading 
            return bmpResult;
        }
    }
    else
    {
        // Should never get here
    }

    if (pBmp->bitCount != 24) {
        // Not supported
        return BmpResultFailure;
    }

    return BmpResultOk;
}

BmpResult BmpDisplay(const Bmp* cpBmp, const ScreenBuffer* cpScreenBuffer) {
    static_assert(sizeof(BmpIdentifier) == sizeof(WORD));

    if (cpBmp == NULL)
        return BmpResultFailure;
    if (cpScreenBuffer == NULL)
        return BmpResultFailure;
    if (cpBmp->width == 0 || cpBmp->height == 0)
        return BmpResultFailure;

    // If the bitmap we want to display is of the same size as the screen , we don't have to apply any scaling and
    // we can directly copy pixel by pixel
    if (cpScreenBuffer->screenSize.width == cpBmp->width && cpScreenBuffer->screenSize.height == cpBmp->height) {
        FastDisplayBitmap(cpBmp, cpScreenBuffer);
    }
    else {
        SlowDisplayBitmap(cpBmp, cpScreenBuffer);
    }

    return BmpResultOk;
}
