/*
 * glyph.h
 *
 *  Created on: 25 nov 2021
 *      Author: mnznn
 * 
 * The file contains the main GlyphMetrics structure definition
 */

#ifndef INC_FONTS_GLYPH_H_
#define INC_FONTS_GLYPH_H_

#include <typedefs.h>
#include <screen/screen.h>

/// @brief Structure that contains information about the placement, orientation, size of a glyph in a character cell
typedef struct _GLYPHMETRICS {
    /// @brief The width of the smallest rectangle that completely encloses the glyph (its black box).
    UInt16  blackBoxX;
    /// @brief The height of the smallest rectangle that completely encloses the glyph (its black box).
    UInt16  blackBoxY;
    /// @brief The x- and y-coordinates of the upper left corner of the smallest rectangle that completely encloses the glyph.
    PointS glyphOrigin;
    /// @brief The horizontal distance from the origin of the current character cell to the origin of the next character cell.
    Int16 cellIncX;
    /// @brief The vertical distance from the origin of the current character cell to the origin of the next character cell
    Int16 cellIncY;
    /// @brief Size of the buffer related to the current glyph
    UInt16 bufferSize;
} GlyphMetrics, * PGlyphMetrics;

/// @brief Retrieves the outline and bitmap for a character in the font that is compiled togheter into the specified device context.
/// @param glyph Character to retrieve
/// @param metric Out pointer where the glyph information will be stored
/// @param data Will contain the pointer to the constant data of the font
void GetGlyphOutline(char glyph, PGlyphMetrics metric, PCBYTE* data);


#endif /* INC_FONTS_GLYPH_H_ */
