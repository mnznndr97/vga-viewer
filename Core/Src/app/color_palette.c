#include <app/color_palette.h>
#include <stddef.h>

/// Active screen buffer pointer
static ScreenBuffer* _pActiveBuffer = NULL;

/// Number of red levels available (4 or 256)
static int _redLevels = 0;
/// Active red level
static int _redLevel = 0;

/// Increment that needs to be applied to the red level.
/// When screen buffer is in 8bpp, the red must be increment of 64 values
/// for each level
static int _redIncrement = 0;

static void DrawPalette() {
    Pen currentPen = { 0 };
    ScreenBuffer* screenBuffer = _pActiveBuffer;

    // Alpha and red are fixed
    currentPen.color.components.A = 0xFF;
    currentPen.color.components.R = (BYTE)(_redLevel * _redIncrement);

    // Simple loop to draw all the blue levels by line and all green levels by row
    float bluDivisions = screenBuffer->screenSize.height / 256.0f;
    float greenDivisions = screenBuffer->screenSize.width / 256.0f;

    PointS pixelPoint = { 0 };
    for (int line = 0; line < screenBuffer->screenSize.height; line++) {
        // Blue and Y are fixed for the whole line
        // Blue level [0; 255] in the col is simply defined by the line
        pixelPoint.y = (Int16)line;
        currentPen.color.components.B = (BYTE)((float)line / bluDivisions);

        for (int col = 0; col < screenBuffer->screenSize.width; col++) {
            pixelPoint.x = (Int16)col;
            currentPen.color.components.G = (BYTE)((float)col / greenDivisions);

            ScreenDrawPixel(screenBuffer, pixelPoint, &currentPen);
        }
    }
}

// ##### Public Function definitions #####

void AppPaletteInitialize(ScreenBuffer* screenBuffer) {
    // We register the frame buffer
    _pActiveBuffer = screenBuffer;

    // Let's calculate the red level and increment
    if (screenBuffer->bitsPerPixel == Bpp8) {
        _redLevels = 4;
    }
    else {
        _redLevels = 256;
    }
    _redIncrement = 256 / _redLevels;
    _redLevel = 0;

    // Eventually we draw the initial palette
    DrawPalette();
}

void AppPaletteProcessInput(char command) {
    // Super simple here : 
    // '+' char increments the red level and redraw the palette
    // '-' char decrements the red level and redraw the palette
    if (command == '+' && _redLevel < (_redLevels - 1)) {
        ++_redLevel;
        DrawPalette();
    }
    else if (command == '-' && _redLevel > 0) {
        --_redLevel;
        DrawPalette();
    }
}

void AppPaletteClose() {
    // Nothing to do here; let's just remove the screen buffer reference
    _pActiveBuffer = NULL;
}
