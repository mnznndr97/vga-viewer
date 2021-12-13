/*
 * binary.c
 *
 *  Created on: 13 dic 2021
 *      Author: mnznn
 */

#include <binary.h>

UInt16 U16ChangeEndiannes(UInt16 little) {
    return (UInt16)(
        ((little << 8) & 0xFF00) | ((little >> 8) & 0x00FF));
}
