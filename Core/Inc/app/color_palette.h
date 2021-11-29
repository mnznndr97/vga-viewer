/*
 * color_palette.h
 *
 *  Created on: Nov 29, 2021
 *      Author: mnznn
 */

#include <screen/screen.h>

#ifndef INC_APP_COLOR_PALETTE_H_
#define INC_APP_COLOR_PALETTE_H_


void AppPaletteInitialize(ScreenBuffer* screenBuffer);
void AppPaletteProcessInput(char command);
void AppPaletteDeinitialize();


#endif /* INC_APP_COLOR_PALETTE_H_ */
