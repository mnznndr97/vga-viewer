/*
 * screen.h
 *
 *  Created on: Oct 18, 2021
 *      Author: Andrea Monzani
 * 
 * Main header file for the screen abstraction layer
 */

#ifndef INC_SCREEN_SCREEN_H_
#define INC_SCREEN_SCREEN_H_

#include <typedefs.h>

#define SCREEN_RGB(r, b, g) ((0xFF000000) | (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))
#define SCREEN_ARGB(a, r, b, g) ((((a) & 0xFF) << 24) | (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

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

typedef union {
	struct _Components {
		BYTE B;
		BYTE G;
		BYTE R;
		BYTE A;
	} components;
	UInt32 argb;
} ARGB8Color;

typedef struct _Pen {
	ARGB8Color color;
} Pen;

typedef void (*DrawPixelCallback)(PointS point, const Pen *pen);

typedef struct _ScreenBuffer {
	SizeS screenSize;
	Bpp bitsPerPixel;

	/// Most common and "universal" function that a screen driver must implement that provides a single pixels
	/// to the driver to be drawn
	DrawPixelCallback DrawCallback;

	/// Optmized draw method from the driver. If the color depth is not a multiple of a byte or the processor supports some intrinsics, 
	/// we can optimize the memory access when filling some "big" area (size multiple of a pack) by writing the same pixel in adjacent locations
	/// For example, when using 3bpp we can write 4 pixels in one shot using a 32 bit store instruction
	/// \remarks Not all the underlying drivers may support such optimizazion so this filed can be an exact copy of the DrawCallback
	DrawPixelCallback DrawPackCallback;
	/// Size of the group of pixels that the optmized driver draw callback supports
	BYTE packSize;
} ScreenBuffer, *PScreenBuffer;

extern void Error_Handler(void);

void ScreenClear(const ScreenBuffer* buffer, const Pen* pen);
void ScreenDrawRectangle(const ScreenBuffer *buffer, PointS point, SizeS size, const Pen *pen);
/// Returns the max height that a character can have in the screen
UInt16 ScreenGetCharMaxHeight();
void ScreenMeasureString(const char* str, SizeS* size);
void ScreenDrawString(const ScreenBuffer *buffer, const char* str, PointS point, const Pen *pen);

/// Lower lever abstraction API for drawing a single pixel on the screen using the underlying hardware API
/// \param buffer Pointer to the current ScreenBuffer we are drawing on
/// \param point Pixel position on the screen
/// \param pixel Pixel color
/// \remarks The function (to avoid too much overhead) WILL NOT perform any checks on the parameter passed to it if called directly
/// The behaviour is undefined in such case
void ScreenDrawPixel(const ScreenBuffer *buffer, PointS point, const Pen *pen);
/// "Optmized" lower lever abstraction API for drawing a pack of pixel on the screen using the underlying hardware API given the
/// scecified pack size
/// \param buffer Pointer to the current ScreenBuffer we are drawing on
/// \param point Pixel position on the screen
/// \remarks The function (to avoid too much overhead) WILL NOT perform any checks on the parameter passed to it if called directly.
/// The behaviour is undefined in such case
void ScreenDrawPixelPack(const ScreenBuffer *buffer, PointS point, const Pen *pen);

#endif /* INC_SCREEN_SCREEN_H_ */
