/*
 * ascii_table.c
 *
 *  Created on: 5 gen 2022
 *      Author: mnznn
 */

#include <app/ascii_table.h>
#include <intmath.h>

 /// Active screen buffer pointer
static ScreenBuffer* _pActiveBuffer = NULL;

/* Forward section */
void DrawTable();

/* Private section */

void DrawTable() {
    char string[2] = { '\0', '\0' };

    SizeS charSize = { 0 };
    int lineHeight = 0;

    Pen pen = { 0 };
    pen.color.argb = SCREEN_RGB(0, 0, 0);
    ScreenClear(_pActiveBuffer, &pen);

    pen.color.argb = SCREEN_RGB(0xFF, 0xFF, 0xFF);

    const int xOffset = 0;

    PointS point;
    point.x = xOffset;
    point.y = 5;
    for (char i = 0; i < 128; i++)
    {
        string[0] = i;
        ScreenMeasureString(string, &charSize);
        if ((point.x + charSize.width) > _pActiveBuffer->screenSize.width) {
            point.x = xOffset;
            point.y = point.y + lineHeight;
            lineHeight = 0;
        }

        ScreenDrawString(_pActiveBuffer, string, point, &pen);

        lineHeight = MAX(lineHeight, charSize.height);
        point.x = point.x + charSize.width;
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
}

void AsciiTableClose()
{
    _pActiveBuffer = NULL;
}
