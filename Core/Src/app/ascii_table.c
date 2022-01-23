#include <app/ascii_table.h>
#include <intmath.h>

/// Active screen buffer pointer
static ScreenBuffer* _pActiveBuffer = NULL;
/// Size of the title box
static int _titleBoxHeight = 0;

/* Forward section */

/// Draws all the available characters on the screen
static void DrawTable();

/// Draws the application title bar
static void DrawApplicationTitle();

/* Private section */

void DrawApplicationTitle() {
    const char* title = "ASCII table";
    const int padding = 3;

    // Title measurement
    SizeS titleStrBox = { 0 };
    ScreenMeasureString(title, &titleStrBox);

    // Title box dimension calc
    _titleBoxHeight = titleStrBox.height + (padding * 2);
    SizeS titleBox = titleStrBox;
    titleBox.height = (Int16)_titleBoxHeight;
    titleBox.width = _pActiveBuffer->screenSize.width;

    Pen pen = { 0 };
    pen.color.argb = SCREEN_RGB(0xFF, 0x6F, 0);

    // Title box filling
    PointS point = { 0 };
    ScreenFillRectangle(_pActiveBuffer, point, titleBox, &pen);

    // Text drawing inside the box
    point.x = (Int16)((_pActiveBuffer->screenSize.width / 2) - (titleStrBox.width / 2));
    point.y = (Int16)padding;

    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);
    ScreenDrawString(_pActiveBuffer, title, point, &pen);
}

void DrawTable() {
    // Screen support only string drawing. No big deal, we create a one char fake string
    char string[2] = { '\0', '\0' };

    // We clear the screen
    Pen pen = { 0 };
    pen.color.argb = SCREEN_RGB(0, 0, 0);
    ScreenClear(_pActiveBuffer, &pen);

    DrawApplicationTitle();

    SizeS charSize = { 0 };
    int lineHeight = 0;
    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);
    // Let' s draw with a little offset to the screen borders
    const int yPadding = 5;
    const int xOffset = 5;

    PointS point;
    point.x = (Int16)xOffset;
    point.y = (Int16)(yPadding + _titleBoxHeight);

    // Let's iterate over the available ASCII chars
    for (char i = 0; i < 128; i++)
    {
        // We setup the string and we measure it
        string[0] = i;
        ScreenMeasureString(string, &charSize);

        // If string bounding box goes out of screen, let' s start a new line
        if ((point.x + charSize.width) > _pActiveBuffer->screenSize.width) {
            point.x = (Int16)xOffset;
            point.y = (Int16)(point.y + lineHeight + yPadding);
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
