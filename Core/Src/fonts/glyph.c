/*
 * glyph.c
 *
 *  Created on: 25 nov 2021
 *      Author: mnznn
 */

#include <fonts/glyph.h>

// Extern variabled declaration
extern const GlyphMetrics s_glyphs[];
extern const BYTE *s_glyphsData[];

void GetGlyphOutline(char glyph, PGlyphMetrics metric, PCBYTE *data) {
	*metric = s_glyphs[glyph];
	*data = s_glyphsData[glyph];
}
