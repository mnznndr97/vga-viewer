/*
 * bmp.h
 *
 *  Created on: 30 dic 2021
 *      Author: mnznn
 */

#ifndef INC_APP_BMP_H_
#define INC_APP_BMP_H_

#include <typedefs.h>
#include <screen/screen.h>
#include "fatfs.h"

typedef enum _BmpResult {
	BmpResultOk, BmpResultFailure
} BmpResult;

typedef enum _BmpIdentifier {
	BmpIdentifierBM = 0x4D42
} BmpIdentifier;

typedef struct _Bmp {
	FIL *fileHandle;

	BmpIdentifier identifier;
	UInt32 fileSize;
	UInt32 dataOffset;

	UInt32 width;
	UInt32 height;

	UInt16 bitCount;
} Bmp;

BmpResult BmpReadFromFile(FIL *file, Bmp *pBmp);
BmpResult BmpDisplay(const Bmp *cpBmp, const ScreenBuffer *cpScreenBuffer);
#endif /* INC_APP_BMP_H_ */
