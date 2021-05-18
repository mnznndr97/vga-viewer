/*
 * SD specific CRC7 implementation.
 *
 * For more information see SD Physical Layer Simplified Specification - Section 4.5 (CRC)
 *
 *  Created on: May 18, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef CRC7_H_
#define CRC7_H_

#define CRC7_ZERO 0

#include <stdint.h>

void Crc7Initialize();

uint8_t Crc7Add(uint8_t crc, uint8_t data);

#endif /* CRC7_H_ */
