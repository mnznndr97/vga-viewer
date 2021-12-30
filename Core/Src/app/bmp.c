/*
 * bmp.c
 *
 *  Created on: 30 dic 2021
 *      Author: mnznn
 */

#include <app/bmp.h>
#include <assertion.h>

typedef struct tagBITMAPFILEHEADER {
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

#define DIB_OFFSET 14

BmpResult FastDisplayBitmap(const Bmp *cpBmp, const ScreenBuffer *cpScreenBuffer) {
	FRESULT result = f_lseek(cpBmp->fileHandle, cpBmp->dataOffset);
	if (result != FR_OK) {
		return BmpResultFailure;
	}

	UInt32 rowSize = ((cpBmp->bitCount * cpBmp->width) + 31) / 32; // DWord size
	rowSize *= 4;

	Pen pen = { 0 };
	pen.color.components.A = 0xFF;

	PointS point;

	UInt32 currentRowPos = f_tell(cpBmp->fileHandle);
	for (UInt16 row = 0; row < cpScreenBuffer->screenSize.height; row++) {
		point.y = row;

		result = f_lseek(cpBmp->fileHandle, currentRowPos);
		if (result != FR_OK) {
			return BmpResultFailure;
		}

		for (UInt16 col = 0; col < cpScreenBuffer->screenSize.width; col++) {
			UInt32 argb = 0;
			BYTE *ptr = ((BYTE*) &argb) + 1;
			UINT read;

			result = f_read(cpBmp->fileHandle, &pen.color.components.R, 1, &read);
			result = f_read(cpBmp->fileHandle, pen.color.components.G, 1, &read);
			result = f_read(cpBmp->fileHandle, pen.color.components.B, 1, &read);

//			pen.color.argb |= argb;
			point.x = col;

			ScreenDrawPixel(cpScreenBuffer, point, &pen);

		}

		currentRowPos += rowSize;
	}
	return BmpResultOk;
}

BmpResult ReadBufferOffset(Bmp *pBmp) {
	FRESULT result = f_lseek(pBmp->fileHandle, offsetof(BITMAPFILEHEADER, bfOffBits) - 2);
	if (result != FR_OK) {
		return BmpResultFailure;
	}

	// We only care for the size and bpp at the moment
	UINT read;
	result = f_read(pBmp->fileHandle, &pBmp->dataOffset, sizeof(DWORD), &read);
	if (result != FR_OK) {
		return BmpResultFailure;
	}
	return BmpResultOk;
}

BmpResult ReadWindowsBitmapCoreHeader(Bmp *pBmp) {
	// Wikipedia link: https://en.wikipedia.org/wiki/BMP_file_format (not updated)
	// Main document: https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-storage
	// A bitmap file is composed of a BITMAPFILEHEADER structure followed by a BITMAPINFOHEADER

	// NB: Wikipedia is referring to the old BITMAPCOREHEADER structure in the DIB description
	// but, as stated in https://docs.microsoft.com/en-us/windows/win32/gdi/bitmap-header-types,
	// the type has been deprecated
	// So let's focus on current windows implementation

	FRESULT result = f_lseek(pBmp->fileHandle, DIB_OFFSET + sizeof(DWORD));
	if (result != FR_OK) {
		return BmpResultFailure;
	}

	// We only care for the size and bpp at the moment
	UINT read;
	result = f_read(pBmp->fileHandle, &pBmp->width, sizeof(DWORD), &read);
	if (result != FR_OK) {
		return BmpResultFailure;
	}
	result = f_read(pBmp->fileHandle, &pBmp->height, sizeof(DWORD), &read);
	if (result != FR_OK) {
		return BmpResultFailure;
	}

	result = f_lseek(pBmp->fileHandle, f_tell(pBmp->fileHandle) + sizeof(WORD));
	if (result != FR_OK) {
		return BmpResultFailure;
	}

	result = f_read(pBmp->fileHandle, &pBmp->bitCount, sizeof(WORD), &read);
	if (result != FR_OK) {
		return BmpResultFailure;
	}

	return BmpResultOk;
}

BmpResult ValidateIdentifier(const Bmp *pBmp) {
	if (pBmp->identifier != BmpIdentifierBM)
		return BmpResultFailure;
	return BmpResultOk;
}

/* Public section */

BmpResult BmpReadFromFile(FIL *file, Bmp *pBmp) {
	static_assert(sizeof(BmpIdentifier) == sizeof(WORD));

	if (file == NULL)
		return BmpResultFailure;
	if (pBmp == NULL)
		return BmpResultFailure;

	UINT read;
	FRESULT result = f_read(file, &pBmp->identifier, sizeof(BmpIdentifier), &read);
	if (result != FR_OK) {
		return BmpResultFailure;
	}
	DebugAssert(read == sizeof(BmpIdentifier));

	BmpResult bmpResult;
	if ((bmpResult = ValidateIdentifier(pBmp)) != BmpResultOk) {
		return bmpResult;
	}

	pBmp->fileHandle = file;
	if ((bmpResult = ReadBufferOffset(pBmp)) != BmpResultOk) {
		return bmpResult;
	}
	if (pBmp->identifier == BmpIdentifierBM) {
		ReadWindowsBitmapCoreHeader(pBmp);
	}

	if (pBmp->bitCount != 24) {
		// Not supported
		return BmpResultFailure;
	}

	return BmpResultOk;
}

BmpResult BmpDisplay(const Bmp *cpBmp, const ScreenBuffer *cpScreenBuffer) {
	static_assert(sizeof(BmpIdentifier) == sizeof(WORD));

	if (cpBmp == NULL)
		return BmpResultFailure;
	if (cpScreenBuffer == NULL)
		return BmpResultFailure;

	if (cpScreenBuffer->screenSize.width == cpBmp->width && cpScreenBuffer->screenSize.height == cpBmp->height) {

		// Simple copy here
		FastDisplayBitmap(cpBmp, cpScreenBuffer);
	}

	return BmpResultOk;
}
