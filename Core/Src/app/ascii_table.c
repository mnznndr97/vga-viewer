#include <app/ascii_table.h>
#include <intmath.h>

 /// Active screen buffer pointer
static ScreenBuffer* _pActiveBuffer = NULL;

/* Forward section */

/// Draws all the available characters on the screen
void DrawTable();

/* Private section */

void DrawTable() {
	// Screen support only string drawing. No big deal, we create a one char fake string
    char string[2] = { '\0', '\0' };

    // We clear the screen
    Pen pen = { 0 };
    pen.color.argb = SCREEN_RGB(0, 0, 0);
    ScreenClear(_pActiveBuffer, &pen);

    SizeS charSize = { 0 };
	int lineHeight = 0;
    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);
    // Let' s draw with a little offset to the screen borders
    const int xOffset = 5;
    const int yOffset = 5;

    PointS point;
    point.x = (Int16)xOffset;
    point.y = (Int16)yOffset;

    // Let's iterate over the available ASCII chars
    for (char i = 0; i < 128; i++)
    {
    	// We setup the string and we measure it
        string[0] = i;
        ScreenMeasureString(string, &charSize);

        // If string bounding box goes out of screen, let' s start a new line
        if ((point.x + charSize.width) > _pActiveBuffer->screenSize.width) {
            point.x = (Int16)xOffset;
            point.y = (Int16)(point.y + lineHeight + yOffset);
            // We reset also the max height since characters may change
            lineHeight = 0;
        }

        ScreenDrawString(_pActiveBuffer, string, point, &pen);

        // We update max height and drawing position
        lineHeight = MAX(lineHeight, charSize.height);
        point.x = (Int16)(point.x + charSize.width);
    }
}

/* Public section */

void AsciiTableInitialize(ScreenBuffer* screenBuffer)
{
    _pActiveBuffer = screenBuffer;
    DrawTable();
}

void AsciiTableProcessInput(char command)
{
	// Nothing to handle in this application
}

void AsciiTableClose()
{
    _pActiveBuffer = NULL;
}
