/*
 * csd.c
 *
 *  Created on: 11 dic 2021
 *      Author: mnznn
 */

#include <sd/csd.h>
#include <assertion.h>
#include <crc/crc7.h>
#include <binary.h>
#include <stdio.h>

#define CSD_STRUCTURE_U8OFFSET 0
#define CSD_STRUCTURE_MASK 0x3

#define TRANSPEED_U8OFFSET 3
#define TRAN_SPEED_RATE_MASK 0x07

#define TRAN_SPEED_TIME_POS 3
#define TRAN_SPEED_TIME_MASK (0x0f) << (TRAN_SPEED_TIME_POS)

#define CCC_U16OFFSET 2
#define CCC_SHIFT 4

#define CCC_V1_FIXEDBITS 0xDFF
#define CCC_V2_FIXEDBITS 0x5FD
 /// CSD minimal version 1.0 mask
 /// \remarks 0b01x1 1011 0101
#define CCC_V1_FIXEDBITS_VALUE ((SDClass10 | SDClass8) | (SDClass7 | SDClass5 | SDClass4) | (SDClass2 | SDClass0))
/// CSD minimal version 2.0 mask
/// \remarks 0bx1x1 1011 01x1
#define CCC_V2_FIXEDBITS_VALUE CCC_V1_FIXEDBITS_VALUE

#define READBLLEN_U8OFFSET 5
#define READBLLEN_MASK 0x0F;

struct _CSDRegister {
    BYTE Raw[SD_CSD_SIZE];
};

static const float s_tranSpeedValues[] = { 0.0f, 1.0f, 1.2f, 1.3f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 7.0f, 8.0f };

//struct _CSDRegisterV1 {
//    BYTE RsvAlways1 : 1; // Bit 0
//    BYTE Crc : 7; // Bits [7:1]
//    BYTE Reserved4 : 2; // Bits [9:8]
//    BYTE FileFormat : 2; // Bits [11:10]
//    BYTE TmpWriteProtect : 1; // Bits [12:12]
//    BYTE PermWriteProtect : 1; // Bits [13:13]
//    BYTE Copy : 1; // Bits [14:14]
//    BYTE FileFormatGrp : 1; // Bits [15:15]
//    BYTE Reserved3 : 5; // Bits [20:16]
//    BYTE WriteBlPartial : 1; // Bits [21:21]
//    BYTE WriteBlLen : 4; // Bits [25:22]
//    BYTE R2WFactor : 3; // Bits [28:26]
//    BYTE Reserved2 : 2; // Bits [30:29]
//    BYTE WpGrpEn : 1; // Bits [31:31]
//    BYTE WpGrpSize : 7; // Bits [38:32]
//    BYTE SectorSize : 7; // Bits [45:39]
//    BYTE EraseBlkEn : 1; // Bits [46:46]
//    BYTE CSizeMult : 3; // Bits [49:47]
//    BYTE VddWCurrMax : 3; // Bits [52:50]
//    BYTE VddWCurrMin : 3; // Bits [55:53]
//    BYTE VddRCurrMax : 3; // Bits [58:56]
//    BYTE VddRCurrMin : 3; // Bits [61:59]
//    UInt16 CSize : 12; // Bits [73:62]
//    BYTE Reserverd1 : 2; // Bits [75: 74]
//    BYTE ReadBlkMisalign : 1; // Bits [77: 77]
//    BYTE WriteBlkMisalign : 1; // Bits [78: 78]
//    BYTE ReadBlPartial : 1; // Bits [79: 79]
//    BYTE ReadBlLen : 4; // Bits [83: 80]
//    UInt16 CCC : 12; // Bits [95: 84]
//    BYTE TranSpeed; // Bits [103: 96]
//    BYTE NSAC; // Bits [111 : 104]
//    BYTE TAAC; // Bits [119 : 112]
//    BYTE Reserved : 6; // Bits [125 : 120]
//    BYTE Version : 2; // Bits [127 - 126]
//} __attribute__((aligned(1), packed));

UInt16 GetCCC(PCUInt16 pCSDu16) {
    UInt16 littleEndianCCC = pCSDu16[CCC_U16OFFSET];
    return U16ChangeEndiannes(littleEndianCCC) >> CCC_SHIFT;
}

BYTE GetTranSpeedByte(PCBYTE pCSD) {
    return pCSD[TRANSPEED_U8OFFSET];
}

/// Reads the READ_BL_LEN parameter from the CSD
/// pCSD Pointer to the CSD register
/// @return Maximum read data block length parameter
BYTE GetReadBlLen(PCBYTE pCSD) {
    return (pCSD[READBLLEN_U8OFFSET]) & READBLLEN_MASK;
}

// ##### Private Implementation #####

// ##### Public Implementation #####

SDCSDVersion SDCSDGetVersion(PCCSDRegister pCSD) {
    PCBYTE pRaw = (PCBYTE)pCSD->Raw;

    // Version is written in the first byte
    return (SDCSDVersion)(pRaw[CSD_STRUCTURE_U8OFFSET] & CSD_STRUCTURE_MASK);

}

SDCSDValidation SDCSDValidate(PCCSDRegister pCSD) {
    static_assert(sizeof(CSDRegister) == SD_CSD_SIZE);

    if (pCSD == NULL)
        return SDCSDValidationInvalidPointer;

    PCBYTE pRaw = (PCBYTE)pCSD->Raw;

    // Useless to check the field ranges if checksum is not valid
    // NB: last byte is excluded since it contains the CRC
    BYTE crc7 = Crc7Calculate(pRaw, SD_CSD_SIZE - 1);
    if (crc7 != (pRaw[SD_CSD_SIZE - 1] >> 1)) {
        return SDCSDValidationCRCFailed;
    }

    SDCSDVersion version = SDCSDGetVersion(pCSD);
    if (version != SDCSDV1p0 && version != SDCSDV2p0)
        return SDCSDValidationInvalidVersion;

    // First bit (last bit in our memory) should always be 1 in all versions
    if ((pRaw[SD_CSD_SIZE - 1] & 0x1) == 0)
        return SDCSDValidationReservedMismatch;

    // Remaining version bytes should be zero
    if ((pRaw[CSD_STRUCTURE_U8OFFSET] & ((BYTE)(~(CSD_STRUCTURE_MASK)))) != 0)
        return SDCSDValidationReservedMismatch;

    /*UInt16 ccc = GetCCC(pRawU16);
     if ((version == SDCSDV1p0 && ((ccc & CCC_V1_FIXEDBITS) != CCC_V1_FIXEDBITS_VALUE)) || (version == SDCSDV2p0 && ((ccc & CCC_V2_FIXEDBITS) != CCC_V2_FIXEDBITS_VALUE)))
     return SDCSDValidationInvalidCCC;*/

     // As stated in CSD description [Section 5.3], the TranSpeed should always be 25 Mhz (mandatory)
     // In high speed the max can be 50 MHz. For CSD v2, the specification allows eve higher data rates but,
     // as stated in the specification, a CMD0 reset the limit to 25Mhz
    BYTE tranSpeed = GetTranSpeedByte(pRaw);
    if (tranSpeed != 0x32 && tranSpeed != 0x5) {
        return SDCSDValidationTranSpeedNotSupported;
    }

    BYTE readBlLen = GetReadBlLen(pRaw);
    if ((version == SDCSDV1p0 && (readBlLen < 9 || readBlLen > 11)) || (version == SDCSDV2p0 && (readBlLen != 9))) {
        return SDCSDValidationInvalidReadBlLen;
    }

    return SDCSDValidationOk;
}

UInt32 SDCSDGetMaxTransferRate(PCCSDRegister pCSD) {
    // Function is assuming a valid CSD
    BYTE tranSpeed = GetTranSpeedByte(pCSD->Raw);
    UInt32 baseFreq = 100000; // Base frequency is 100Khz

    BYTE freqScaler = (BYTE)(tranSpeed & TRAN_SPEED_RATE_MASK);
    DebugAssert(freqScaler >= 0 && freqScaler < 4); // Just asserting we are doing things right

    for (size_t i = 0; i < freqScaler; i++) {
        baseFreq *= 10;
    }

    BYTE timeValue = (BYTE)((tranSpeed & TRAN_SPEED_TIME_MASK) >> TRAN_SPEED_TIME_POS);
    DebugAssert(timeValue >= 0 && timeValue < 16); // Just asserting we are doing things right
    return s_tranSpeedValues[timeValue] * baseFreq;
}

UInt16 SDCSDGetMaxReadDataBlockLength(PCCSDRegister pCSD) {
    // Function is assuming a valid CSD
    BYTE exponent = GetReadBlLen(pCSD->Raw);
    return 1 << exponent; // Block size = 2 ^ (READ_BL_LEN)
}

void SDCSDDumpValidationResult(SDCSDValidation result)
{
    switch (result)
    {
    case SDCSDValidationOk:
        printf("OK");
        break;
    case SDCSDValidationInvalidPointer:
        printf("invalid pointer provided");
        break;
    case SDCSDValidationCRCFailed:
        printf("CRC error");
        break;
    case SDCSDValidationInvalidVersion:
        printf("invalid version");
        break;
    case SDCSDValidationReservedMismatch:
        printf("reserved fields mismatch");
        break;
    case SDCSDValidationInvalidCCC:
        printf("invalid CCC");
        break;
    case SDCSDValidationTranSpeedNotSupported:
        printf("transfer speed not supported");
        break;
    case SDCSDValidationInvalidReadBlLen:
        printf("invalid read block length");
        break;
    default:
        break;
    }
}
