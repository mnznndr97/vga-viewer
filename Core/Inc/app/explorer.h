/*
 * explorer.h
 *
 *  Created on: 20 dic 2021
 *      Author: mnznn
 */

#ifndef INC_APP_EXPLORER_H_
#define INC_APP_EXPLORER_H_

#include <screen/screen.h>

void ExplorerOpen(ScreenBuffer *screenBuffer);
void ExplorerProcessInput(char command);
void ExplorerClose();

#endif /* INC_APP_EXPLORER_H_ */
