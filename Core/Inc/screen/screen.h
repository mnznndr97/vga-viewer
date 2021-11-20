/*
 * screen.h
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#ifndef INC_SCREEN_SCREEN_H_
#define INC_SCREEN_SCREEN_H_

#include <typedefs.h>

typedef enum _Bpp {
	Bpp3, Bpp8
} Bpp;

typedef struct _PointS {
	Int16 x;
	Int16 y;
} PointS, *PPointS;

typedef struct _SizeS {
	Int16 width;
	Int16 height;
} SizeS, *PSizeS;

typedef struct _ScreenBuffer {
	SizeS screenSize;
	Bpp bitsPerPixel;

} ScreenBuffer, *PScreenBuffer;

extern void Error_Handler(void);

void ScreenDrawPixel(const ScreenBuffer *buffer, PointS point, UInt32 pixel);
void ScreenDrawPixelRGB(const ScreenBuffer *buffer, PointS point, BYTE r, BYTE g, BYTE b);

#endif /* INC_SCREEN_SCREEN_H_ */
