/*
 * This file contains the headers of the SD specific CRC16 implementation.
 * Before using the CRC16 implementation, the Initialize function must be called.
 * Each CRC calculation must start using the CRC16_ZERO value
 *
 * For more information see SD Physical Layer Simplified Specification - Section 4.5 (CRC)
 *
 * @remarks The polynomial is fixed in the implementation using the SD specification
 * but this can be generalized in a future using a kinda CRC handle initialized using a
 * custom polynomial
 *
 *  Created on: Dic 9, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_CRC_CRC16_H_
#define INC_CRC_CRC16_H_

#define CRC16_ZERO 0

#include <stdint.h>

/// Initialize the CRC16 object
void Crc16Initialize();

/// Accumulate the CRC using a provided data byte
uint16_t Crc16Add(uint16_t crc, uint8_t data);

#endif /* INC_CRC_CRC16_H_ */
