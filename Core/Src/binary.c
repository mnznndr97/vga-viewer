/*
 * binary.c
 *
 *  Created on: 13 dic 2021
 *      Author: mnznn
 */

#include <binary.h>
#include <string.h>

UInt16 U16ChangeEndiannes(UInt16 little) {
    return (UInt16)(
        ((little << 8) & 0xFF00) | ((little >> 8) & 0x00FF));
}

UInt32 U32ChangeEndiannes(UInt32 little) {
    return (UInt32)(
        ((little << 24) & 0xFF000000) |
        ((little << 16) & 0x00FF0000) |
        ((little << 8) & 0x0000FF00) |
        ((little) & 0x000000FF));
}


int EndsWith(const char* str, const char* suffix)
{
    // From https://stackoverflow.com/a/744822/13184908

    // We don't do anything if the 
    if (str == NULL || suffix == NULL) return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    // Suffix is bigget, do nothing
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}