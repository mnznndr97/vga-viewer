/*
 * color_palette.h
 *
 *  Created on: Nov 29, 2021
 *      Author: mnznn
 */


#ifndef INC_APP_COLOR_PALETTE_H_
#define INC_APP_COLOR_PALETTE_H_

#include <screen/screen.h>

void AppPaletteInitialize(ScreenBuffer* screenBuffer);
void AppPaletteProcessInput(char command);
void AppPaletteClose();


#endif /* INC_APP_COLOR_PALETTE_H_ */
