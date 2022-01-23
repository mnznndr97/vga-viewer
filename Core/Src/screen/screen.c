#include <screen\screen.h>
#include <fonts\glyph.h>
#include <assertion.h>
#include <string.h>
#include <math.h>
#include <intmath.h>
#include "stm32f4xx_hal.h"

/// Fix the font color using the glyph level provided
/// \param glyphLevel Glyph level ranging from 0 to 64 (included)
/// \param fontColor Color of the font
/// \return Modified font color
/// \remarks The function only changes the alpha of the color to have a "better" result on our super scaled screen
static ARGB8Color FixPixelColorWithGlyphLevel(BYTE glyphLevel, ARGB8Color fontColor) {
    // To better display the glyph, I assumed that is better to directly work only on the alpha value
    // In our case, the font bitmaps are exported from Window$. Some of the pixels at the border of the glyph
    // may have a level that is near zero (but not zero). If we modify the RGB components leaving the alpha unaltered
    // for these pixels, we get on the display some black pixels. So i think that is better to only remap the alpha level using
    // the glyph level

    // Simple level calculation using int math. Our glyph bitmap levels range from 0 .. 64
    // Compiler will optimize division with the shift right
    int newAlpha = (fontColor.components.A * glyphLevel) / 64;
    DebugAssert(newAlpha >= 0 && newAlpha <= 255);

    fontColor.components.A = (BYTE)(newAlpha);
    return fontColor;
}

/// Draws a character glyph onto the screen at the specified coordinates
static void ScreenDrawCharacter(const ScreenBuffer* buffer, char character, PointS point, GlyphMetrics* charMetrics, const Pen* pen) {
    // We are currently supporting only simple ASCII characters
    if (character < 0 || character >= 128) {
        // Let's just clear the metrics since they are used from the caller to increment the drawing point
        charMetrics = NULL;
        return;
    }

    // First we get all the information about our character
    PCBYTE glyphBufferPtr;
    GetGlyphOutline(character, charMetrics, &glyphBufferPtr);

    if (charMetrics->bufferSize <= 0) {
        // Character has no graphics associated. No need to draw it
        // The character may still have some enclosing box pixels but
        // we have not the need to "display" these pixels at the moment
        return;
    }

    // We calculate our glyph origin in screen coordinates
    int glyphOriginX = point.x + charMetrics->glyphOrigin.x;
    int glyphOriginY = point.y + charMetrics->glyphOrigin.y;

    if (glyphOriginX > buffer->screenSize.width || glyphOriginY > buffer->screenSize.height) {
        // Glyph is outside screen bounds. Ignoring
        return;
    }

    // If glyph is outside the screen (before origin) we need to skip some lines
    int glyphBufferLineOffset = 0; // Offset (in lines) where to start reading pixels in the glyph buffer
    int glyphXOffset = 0;
    if (glyphOriginY < 0 && (-glyphOriginY) >= charMetrics->blackBoxY) {
        // Glyph is totally outside the vertical position of the screen. We can exit
        return;
    }
    else if (glyphOriginY < 0) {
        // Glyph is partially outside the screen. We simply start drawing from the 0 coordinate
        // ignoring the hidden lines
        // glyphOriginY is smaller that
        glyphBufferLineOffset = -glyphOriginY;
        glyphOriginY = 0;
    }

    // Glyph Y starting point is now settled
    // Let's fix the X
    if (glyphOriginX < 0 && (-glyphOriginX) >= charMetrics->blackBoxX) {
        // Charater is totally outsize the screen in the X axis
        // Nothing to do
        return;
    }
    else if (glyphOriginX < 0) {
        glyphXOffset = glyphXOffset - (BYTE)glyphOriginX;
        glyphOriginX = 0;
    }

    // Calculating our character visible rectangle
    // Origin is just clipped to 0 if was negative
    int hStart = glyphOriginX; // Already clipped to be >= 0
    int vStart = glyphOriginY; // Already clipped to be >= 0
    int hEnd = MIN(glyphOriginX + charMetrics->blackBoxX, buffer->screenSize.width);
    int vEnd = MIN(glyphOriginY + charMetrics->blackBoxY, buffer->screenSize.height);

    // We need to copy the pen since each pixel color will be potentially different
    Pen glyphPixelPen = *pen;

    // The row length of the buffer is a multiple of a Word (32bit), so it must be multiple of 4
    // We simply add 3 to our box width and we clear the 2 LSBs. In this case all the numbers where the
    // LSB are != 0b00 are rounded to the next WORD-aligned number
    int glyphBufferRowWidth = (charMetrics->blackBoxX + 3) & (~0x3);
    // Make sure we are doing fine with our math
    DebugAssert(glyphBufferRowWidth * charMetrics->blackBoxY == charMetrics->bufferSize);

    // Classic line loop
    PointS pixelPt = { 0 };
    for (int line = vStart; line < vEnd; ++line, ++glyphBufferLineOffset) {
        pixelPt.y = (Int16)line; // Cast should be safe since line is between [0; screenHeight)
        pixelPt.x = (Int16)hStart; // Cast should be safe since hStart is between [0; screenWidth)

        // We recalculate the current starting pixel of the glyph
        int glyphX = glyphXOffset;
        // Since glyph rows are aligned at word boundaries, we can read 4 levels at the time
        // to save little time
        // We just need to make sure we are not starting from an unaligned glyph address
        for (; (glyphX & 0x3) != 0 && pixelPt.x < hEnd; pixelPt.x++, glyphX++) {
            // We read the glyph level from the glyph
            BYTE glyphLevel = glyphBufferPtr[glyphBufferLineOffset * glyphBufferRowWidth + glyphX];

            // If level is zero it is useless to draw the pixel
            if (glyphLevel != 0) {
                glyphPixelPen.color = FixPixelColorWithGlyphLevel(glyphLevel, pen->color);
                ScreenDrawPixel(buffer, pixelPt, &glyphPixelPen);
            }
        }

        // From here all accesses to glyphBuffer should be aligned
        for (; pixelPt.x < hEnd; glyphX += 4) {
            // We read the 4 glyph levels from the glyph
            const BYTE* pLevel = &glyphBufferPtr[glyphBufferLineOffset * glyphBufferRowWidth + glyphX];
            UInt32 glyphLevels = *((const UInt32*)pLevel);

            // NB: Our system is little endian so the 1st level is now on the LSB of the 32bit int
            // So we start with no right shift and we the increment the shift of 8 bits at each iteration
            // First iteration we read 000000XX (1st glyph level in little endian), then 0000XX00, 00XX0000, XX000000 (4th glyph level)
            int i = 0;
            while (i < 32 && pixelPt.x < hEnd) {
                BYTE glyphLevel = (glyphLevels >> i) & 0xFF;

                // If level is zero it' s useless to draw the pixel
                if (glyphLevel != 0) {
                    glyphPixelPen.color = FixPixelColorWithGlyphLevel(glyphLevel, pen->color);
                    ScreenDrawPixel(buffer, pixelPt, &glyphPixelPen);
                }
                // Better than using i = 0 .. 3 and multiplying by 8
                i += 8;
                pixelPt.x++;
            }
        }
    }
}

// ##### Public Function definitions #####

void ScreenClear(const ScreenBuffer* buffer, const Pen* pen) {
    ScreenFillRectangle(buffer, (PointS) { 0 }, buffer->screenSize, pen);
}

void ScreenFillRectangle(const ScreenBuffer* buffer, PointS point, SizeS size, const Pen* pen) {
    // We have to draw within the screen bounds
    Int16 hStart = MAX(point.x, 0);
    Int16 hEnd = (Int16)(MIN(point.x + size.width, buffer->screenSize.width));
    Int16 vStart = MAX(point.y, 0);
    Int16 vEnd = (Int16)(MIN(point.y + size.height, buffer->screenSize.height));

    BYTE packSizePower = buffer->packSizePower;
    DebugAssert(packSizePower <= 2); // We work with 32 bits max
    BYTE packSize = (BYTE)(1 << packSizePower);

    // We do the check here to limit the number of branch instruction in the code
    DebugWriteChar('d');

    if (packSize == 1) {
        // No packed draw available. We draw pixel by pixel
        PointS point;
        for (Int16 line = vStart; line < vEnd; line++) {
            point.y = line;
            for (Int16 pixel = hStart; pixel < hEnd; ++pixel) {
                point.x = pixel;
                ScreenDrawPixel(buffer, point, pen);
            }
        }
    }
    else {
        PointS pixelPoint;
        // We need to calculate the PIXEL alignment mask here to have the correct pack alignment
        // NB: We are assuming that the framebuffer start address is already aligned
        BYTE alignmentMask = 0;

        for (BYTE i = 0; i < packSizePower; i++)
        {
            // Left-shit the alignment mask
            alignmentMask = (BYTE)(alignmentMask << 1);
            // Or with a 1
            alignmentMask |= 1;
        }

        // We are only using 32bit alignment so let's make sure this is correct
        DebugAssert(packSizePower == 2 && packSize == 4 && alignmentMask == 0x3);

        for (Int16 line = vStart; line < vEnd; line++) {
            pixelPoint.y = line;

            // First we draw the unaligned pixels at the beginning
            pixelPoint.x = hStart;
            for (int unPixel = hStart & alignmentMask; unPixel != 0; unPixel = (unPixel + 1) & alignmentMask, pixelPoint.x++) {
                ScreenDrawPixel(buffer, pixelPoint, pen);
            }

            // Once we are aligned to the pack, we can start issuing packed draw calls
            for (; pixelPoint.x < (hEnd & (~alignmentMask)); pixelPoint.x = (Int16)(pixelPoint.x + packSize)) {
                ScreenDrawPixelPack(buffer, pixelPoint, pen);
            }

            // We do the same for the trailing pixels that are not aligned
            for (; pixelPoint.x < hEnd; pixelPoint.x++) {
                ScreenDrawPixel(buffer, pixelPoint, pen);
            }
        }
    }
    DebugWriteChar('D');
}

UInt16 ScreenGetCharMaxHeight() {
    UInt16 size = 0;

    // Super simple loop here
    // For each character in our string we get the metric 
    GlyphMetrics charMetrics;
    PCBYTE glyphBufferPtr;
    for (char i = 0; i < 0x80; i++) {
        GetGlyphOutline(i, &charMetrics, &glyphBufferPtr);

        size = MAX(size, charMetrics.blackBoxY);
    }
    return size;
}

void ScreenMeasureString(const char* str, SizeS* size) {
    if (size == NULL)
        return;

    size->height = 0;
    size->width = 0;
    // This is not safe but our project is just display some stuff on the screen, there is not
    // sensible data in it. It may still be hacked though
    size_t stringLength = strlen(str);
    if (stringLength <= 0) {
        // Nothing to draw
        return;
    }

    // Super simple loop here
    // For each character in our string we get the metric 
    GlyphMetrics charMetrics;
    PCBYTE glyphBufferPtr;
    for (int i = 0; i < stringLength; i++) {
        GetGlyphOutline(str[i], &charMetrics, &glyphBufferPtr);

        size->height = (Int16)MAX(size->height, charMetrics.blackBoxY + charMetrics.glyphOrigin.y);
        size->width = (Int16)(size->width + charMetrics.cellIncX);
    }
}

void ScreenDrawString(const ScreenBuffer* buffer, const char* str, PointS point, const Pen* pen) {
    // This is not safe but our project is just display some stuff on the screen, there is not
    // sensible data in it. It may still be hacked though
    size_t stringLength = strlen(str);

    if (stringLength == 0) {
        // Nothing to draw
        return;
    }

    // Super simple loop here
    // For each character in our string we draw it's glyph on the screen and we move the point forward
    GlyphMetrics charMetrics;
    for (int i = 0; i < stringLength; i++) {
        char character = str[i];
        ScreenDrawCharacter(buffer, character, point, &charMetrics, pen);

        // We move our "drawing cursor" forward using the font specifications
        // The font also has a Y increment but we are not interested
        point.x = (Int16)(point.x + charMetrics.cellIncX);
    }
}

void ScreenDrawPixel(const ScreenBuffer* buffer, PointS point, const Pen* pen) {
    buffer->DrawCallback(point, pen);
}
void ScreenDrawPixelPack(const ScreenBuffer* buffer, PointS point, const Pen* pen) {
    buffer->DrawPackCallback(point, pen);
}

