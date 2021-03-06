#include <binary.h>
#include <string.h>

UInt16 U16ChangeEndiannes(UInt16 data) {
    return (UInt16)(
        ((data << 8) & 0xFF00) | ((data >> 8) & 0x00FF));
}

UInt32 U32ChangeEndiannes(UInt32 little) {
    return (UInt32)(
        ((little >> 24) & 0x000000FF) | // MSB becomes LSB
        ((little >> 8) & 0x0000FF00) |
        ((little << 8) & 0x00FF0000) |
        ((little << 24) & 0xFF000000)); // LSB becomes MSB
}

UInt32 ReadUInt32(const BYTE* pBuffer)
{
    uintptr_t address = (uintptr_t)pBuffer;
    if ((address & 0x3) == 0) {
        // Buffer is aligned at word boundary. We can load the int32 directly
        return *((const UInt32*)pBuffer);
    }
    else
    {
        // If we are NOT word aliged, our CPU will issue a trap due to the option _UNALIGN_TRP enable, so we read byte per byte
        UInt32 result = (UInt32)pBuffer[0]; // LSB
        result |= ((UInt32)pBuffer[1]) << 8;
        result |= ((UInt32)pBuffer[2]) << 16;
        result |= ((UInt32)pBuffer[3]) << 24;
        return result;
    }
}

UInt16 ReadUInt16(const BYTE* pBuffer)
{
    uintptr_t address = (uintptr_t)pBuffer;
    if ((address & 0x1) == 0) {
        // Buffer is aligned at half word boundary. We cano load the int32 directly
        return *((const UInt16*)pBuffer);
    }
    else
    {
        // If we are NOT half-word aliged, our CPU will issue a trap due to the option _UNALIGN_TRP enable, so we read byte per byte
        return (UInt16)(
            ((((UInt16)pBuffer[1]) << 8) & 0xFF00) |
            ((((UInt16)pBuffer[0]))));
    }
}


bool EndsWith(const char* str, const char* suffix)
{
    // From https://stackoverflow.com/a/744822/13184908

    // We don't do anything if the strings are empty
    if (str == NULL || suffix == NULL) return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    // Suffix is bigget, do nothing
    if (lensuffix > lenstr)
        return false;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}
