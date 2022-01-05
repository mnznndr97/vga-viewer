/*
 * binary.h
 *
 *  Created on: 13 dic 2021
 *      Author: mnznn
 */

#ifndef INC_BINARY_H_
#define INC_BINARY_H_

#include <typedefs.h>

/// Masks the result of an integer operation to a byte
#define MASKI2BYTE(a) ((a) & 0xFF)

/// Masks the result of an integer operation to a short
#define MASKI2SHORT(a) ((a) & 0xFFFF)

UInt16 U16ChangeEndiannes(UInt16 little);
UInt32 U32ChangeEndiannes(UInt32 little);

UInt32 ReadUInt32(const BYTE* pBuffer);
UInt16 ReadUInt16(const BYTE* pBuffer);

int EndsWith(const char *str, const char *suffix);

#endif /* INC_BINARY_H_ */
