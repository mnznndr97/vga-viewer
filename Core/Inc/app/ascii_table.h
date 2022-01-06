/*
 * Header for the color ASCII table application. The application simply display all
 * the ASCII characters
 *
 *  Created on: Jan 5, 2022
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_APP_ASCII_TABLE_H_
#define INC_APP_ASCII_TABLE_H_

#include <screen/screen.h>

 /// Initialize the ASCII table application on the specified screen buffer
void AsciiTableInitialize(ScreenBuffer* screenBuffer);
/// Input process function
void AsciiTableProcessInput(char command);
/// Closes the application
void AsciiTableClose();

#endif /* INC_APP_ASCII_TABLE_H_ */
