/*
 * This file contains the headers of the abstraction layer that writes shapes into a
 * native framebuffer
 *
 * The screen buffer is represented as a pointer to the ScreenBuffer structure. The structure contains
 * the callbacks to the native later for drawing a pixel or a group of pixels
 *
 * The native screen buffer depth (in bpp) is a "public" info since in is used by the "Palette" application
 * and since it can be used to optimize some draw calls
 *
 * Basic shape drawing are supported
 * -> Clear screen
 * -> Filled Rect
 * -> Text/Char drawing and measurement
 *
 *  Created on: Oct 18, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */


#ifndef INC_SCREEN_SCREEN_H_
#define INC_SCREEN_SCREEN_H_

#include <typedefs.h>

 /// Calculates a color from the RGB values (alpha set at 0xFF)
#define SCREEN_RGB(r, b, g) ((0xFF000000) | (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))
 /// Calculates a color from the ARGB values
#define SCREEN_ARGB(a, r, b, g) ((((a) & 0xFF) << 24) | (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

/// Bit depth of the native frame buffer
typedef enum _Bpp {
    /// Green and Blue pixels are represented with 3 bits, 2 for the red.
    /// A single byte define the color for a single pixel
    Bpp8,
    /// Standard 24 bits per pixels (one byte per color)
    Bpp24
} Bpp;

/// Definition for a point. Buffer size is limited to a Int16 size (with our max resolution
/// of 400x300 this should not be a problem) so we are using Int16 coordinates
typedef struct _PointS {
    /// X-coordinate
    Int16 x;
    /// Y-coordinate
    Int16 y;
} PointS, * PPointS;

/// Definition for a size with width and height component
typedef struct _SizeS {
    Int16 width;
    Int16 height;
} SizeS, * PSizeS;

/// Definition of a screen color with ARGB components at 32bpp
/// Native frame buffer implementation will convert this into its native 
/// format 
typedef union {
    /// Single color components
    struct _Components {
        BYTE B;
        BYTE G;
        BYTE R;
        BYTE A;
    } components;
    /// UInt32 color in the format AARRGGBB -> In byte view BB GG RR AA since
    /// we are in a little-endian machine
    UInt32 argb;
} ARGB8Color;

/// Screen pen to define the drawing settings
typedef struct _Pen {
    /// Color associated to the pen that will be used by the screen buffer to draw pixels
    ARGB8Color color;
} Pen;

/// Delegate definition for the native callback used to draw a pixel on the screen
typedef void (*DrawPixelCallback)(PointS point, const Pen* pen);

/// Main screen buffer information structure
typedef struct _ScreenBuffer {
    /// Screen size. No pixel will be draw out of this bounds
    SizeS screenSize;
    /// Bits per pixels used by the native implementation
    Bpp bitsPerPixel;

    /// Most common and "universal" function that a native screen driver must implement to draw a single pixel
    DrawPixelCallback DrawCallback;
    /// Optimized draw method from the driver. If the color depth is not a multiple of a byte or the processor supports some intrinsics,
    /// we can optimize the memory access when filling some "big" area (size multiple of a pack) by writing the same pixel in adjacent locations
    /// For example, when using 8bpp we can write 4 pixels in one shot using a 32 bit store instruction
    /// \remarks Not all the underlying drivers may support such optimization so this reference can be an exact copy of the DrawCallback
    DrawPixelCallback DrawPackCallback;
    /// Size of the group of pixels that the optimized driver draw callback supports. The value indicates the power of two of the pack size
    /// packSizePower == 0 -> packSize = 1; packSizePower == 1 -> packSize = 2; packSizePower == 2 -> packSize = 4;  
    BYTE packSizePower;
} ScreenBuffer, * PScreenBuffer;

extern void Error_Handler(void);

/// Clears the underlying screen buffer using the color specified in the pen
void ScreenClear(const ScreenBuffer* buffer, const Pen* pen);
/// Fills a rectangle using the color specified in the pen
/// \param buffer Screen buffer destination of the draw call
/// \param point Origin of the rectangle
/// \param size Size of the rectangle
/// \param pen Pen informations
void ScreenFillRectangle(const ScreenBuffer* buffer, PointS point, SizeS size, const Pen* pen);
/// Returns the max height that a character can have in the screen
UInt16 ScreenGetCharMaxHeight();
/// Measure the smallest rectangle enclosing a string
void ScreenMeasureString(const char* str, SizeS* size);
/// Draw a string on the screen buffer at the specified point using the specified pen
void ScreenDrawString(const ScreenBuffer* buffer, const char* str, PointS point, const Pen* pen);
/// Lower lever abstraction API for drawing a single pixel on the screen using the underlying hardware API
/// \param buffer Pointer to the current ScreenBuffer we are drawing on
/// \param point Pixel position on the screen
/// \param pixel Pixel color
/// \remarks The function (to avoid too much overhead) WILL NOT perform any checks on the parameter passed to it if called directly
/// The behavior is undefined in such case
void ScreenDrawPixel(const ScreenBuffer* buffer, PointS point, const Pen* pen);
/// "Optmized" lower lever abstraction API for drawing a pack of pixel on the screen using the underlying hardware API given the
/// specified pack size
/// \param buffer Pointer to the current ScreenBuffer we are drawing on
/// \param point Pixel position on the screen
/// \remarks The function (to avoid too much overhead) WILL NOT perform any checks on the parameter passed to it if called directly.
/// The behavior is undefined in such case
void ScreenDrawPixelPack(const ScreenBuffer* buffer, PointS point, const Pen* pen);

#endif /* INC_SCREEN_SCREEN_H_ */
