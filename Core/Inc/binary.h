/*
 * binary.h
 *
 *  Created on: 13 dic 2021
 *      Author: mnznn
 */

#ifndef INC_BINARY_H_
#define INC_BINARY_H_

#include <typedefs.h>

UInt16 U16ChangeEndiannes(UInt16 little);
UInt32 U32ChangeEndiannes(UInt32 little);

UInt32 ReadUInt32(BYTE* pBuffer);
UInt16 ReadUInt16(BYTE* pBuffer);

int EndsWith(const char *str, const char *suffix);

#endif /* INC_BINARY_H_ */
