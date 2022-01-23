/*
 * Header for some binary utility functions
 *
 *  Created on: Dec 13, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_BINARY_H_
#define INC_BINARY_H_

#include <typedefs.h>

/// Masks the result of an integer operation to a byte
#define MASKI2BYTE(a) ((a) & 0xFF)

/// Masks the result of an integer operation to a short
#define MASKI2SHORT(a) ((a) & 0xFFFF)

/// Changes the endiannes of a 16 bit value
UInt16 U16ChangeEndiannes(UInt16 data);
/// Changes the endiannes of a 32 bit value
UInt32 U32ChangeEndiannes(UInt32 data);

/// @brief Reads a little endian 32 bit value from a generic memory address (aligned or unaligned)
/// @param pBuffer Byte pointer to the 32 bit value
/// @return 32 bit value
UInt32 ReadUInt32(const BYTE* pBuffer);
/// @brief Reads a little endian 16 bit value from a generic memory address (aligned or unaligned)
/// @param pBuffer Byte pointer to the 16 bit value
/// @return 16 bit value
UInt16 ReadUInt16(const BYTE* pBuffer);
/// @brief Checks if a string ends with a specified suffix
/// @param str String to check
/// @param suffix Suffix to search
bool EndsWith(const char *str, const char *suffix);

#endif /* INC_BINARY_H_ */
