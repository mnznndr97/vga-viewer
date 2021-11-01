/*
 * screenbuffer.h
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#ifndef INC_SCREENBUFFER_H_
#define INC_SCREENBUFFER_H_

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

#endif /* INC_SCREENBUFFER_H_ */
