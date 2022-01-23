/*
 * Header for the explorer application. The application simply display the list of all
 * BMP files in the attached SD card. One a file is selected, it can be open and rendered on the screen
 *
 *  Created on: Dec 20, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_APP_EXPLORER_H_
#define INC_APP_EXPLORER_H_

#include <screen/screen.h>

/// Opens the explorer app on the specified screen
void ExplorerOpen(ScreenBuffer *screenBuffer);
/// Processes user command 
void ExplorerProcessInput(char command);
/// Closes the application
void ExplorerClose();

#endif /* INC_APP_EXPLORER_H_ */
