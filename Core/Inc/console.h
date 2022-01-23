/*
 * Defines functions to format specific value typed onto the console
 *
 *  Created on: Nov 12, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

/// Formats a time value into a seconds (or submultiples) string
void FormatTime(float seconds);
/// Formats a frequency value into a Hertz (or multiples) string
void FormatFrequency(float hertz);

#endif /* INC_CONSOLE_H_ */
