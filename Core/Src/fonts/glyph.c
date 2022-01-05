/*
 * glyph.c
 *
 *  Created on: 25 nov 2021
 *      Author: mnznn
 */

#include <fonts/glyph.h>
#include <assertion.h>

// Extern variabled declaration
extern const GlyphMetrics s_glyphs[];
extern const BYTE *s_glyphsData[];

void GetGlyphOutline(char glyph, PGlyphMetrics metric, PCBYTE *data) {
    // Let' s make sure our first glyph buffer is aligned at word boundary
    DebugAssert((((uintptr_t)s_glyphsData) & 0x3) == 0x0);

	*metric = s_glyphs[(int)glyph];
	*data = s_glyphsData[(int)glyph];
}
