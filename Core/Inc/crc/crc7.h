/*
 * This file contains the headers of the SD specific CRC7 implementation.
 * Before using the CRC7 implementation, the Initialize function must be called.
 * Each CRC calculation must start usinf the CRC7_ZERO value
 * 
 * For more information see SD Physical Layer Simplified Specification - Section 4.5 (CRC)
 *
 * @remarks The polynomial is fixed in the implementation using the SD specification
 * but this can be generalized in a future using a kinda CRC handle initialized using a
 * custom polynomial
 * 
 *  Created on: May 18, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef CRC7_H_
#define CRC7_H_

/// Initial CRC value
#define CRC7_ZERO 0

#include <stdint.h>
#include <stddef.h>

/// Initialize the CRC calculation componet by calculating the CRC table for each byte
void Crc7Initialize();
/// Add the CRC for a byte to a current existing CRC value (incremental calculation)
/// @param crc Current CRC value
/// @param data New byte to add to the CRC
/// @return 
uint8_t Crc7Add(uint8_t crc, uint8_t data);
/// @brief Calculates the CRC of a stream of data
/// @param pData Data pointer
/// @param count Number of bytes to read from the pointer
/// @return Data CRC
uint8_t Crc7Calculate(const uint8_t *pData, size_t count);

#endif /* CRC7_H_ */
