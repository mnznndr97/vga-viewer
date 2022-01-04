/*
 * Header for the color palette application. The application simply display all the 256 blue values by
 * row and all the 256 green values by column.
 * The red level can be changed by using the UART '+' and '-' inputs
 * For the 8bpp mode, the red has only 4 "levels"
 *
 *  Created on: Nov 29, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_APP_COLOR_PALETTE_H_
#define INC_APP_COLOR_PALETTE_H_

#include <screen/screen.h>

/// Initialize the color palette application on the specified screen buffer
void AppPaletteInitialize(ScreenBuffer* screenBuffer);
/// Input process function
void AppPaletteProcessInput(char command);
/// Closes the application
void AppPaletteClose();


#endif /* INC_APP_COLOR_PALETTE_H_ */
