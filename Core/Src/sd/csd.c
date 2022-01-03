#include <sd/csd.h>
#include <assertion.h>
#include <crc/crc7.h>
#include <binary.h>
#include <stdio.h>

/// Offset of the CSD structure version (in bytes)
#define CSD_STRUCTURE_U8OFFSET 0
/// Offset of the CSD structure version
#define CSD_STRUCTURE_MASK 0xC0

/// Offset for the Max Transfer Rate (equals for all the versions)
#define TRANSPEED_U8OFFSET 3
/// Mask for the transfer rate unit bits
#define TRAN_SPEED_UNIT_MASK 0x07
/// Position for the transfer rate time bits
#define TRAN_SPEED_TIME_POS 3
/// Mask for the transfer rate time bits
#define TRAN_SPEED_TIME_MASK (0x0f) << (TRAN_SPEED_TIME_POS)

#define CCC_V1_FIXEDBITS 0xDFF
#define CCC_V2_FIXEDBITS 0x5FD

/// CSD minimal version 1.0 mask
 /// \remarks 0b01x1 1011 0101
#define CCC_V1_FIXEDBITS_VALUE ((SDClass10 | SDClass8) | (SDClass7 | SDClass5 | SDClass4) | (SDClass2 | SDClass0))
/// CSD minimal version 2.0 mask
/// \remarks 0bx1x1 1011 01x1
#define CCC_V2_FIXEDBITS_VALUE CCC_V1_FIXEDBITS_VALUE

/// Offset for the read block length field
#define READBLLEN_U8OFFSET 5
/// Mask for the read block length exponent in the field byte
#define READBLLEN_MASK 0x0F;

/// Internal CSD Definition as block of bytes
/// Bit fields are no good since layout is compiler dependent
struct _CSDRegister {
    BYTE Raw[SD_CSD_SIZE];
};

/// Values for time of transfer speed field
static const float s_tranSpeedValues[] = { 0.0f, 1.0f, 1.2f, 1.3f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 7.0f, 8.0f };

/// Read the transfer speed byte from the CSD
/// @param pCSD Pointer to the CSD structure
/// @return Raw byte for the TRAN_SPEED field
BYTE GetTranSpeedByte(PCBYTE pCSD);

/// Reads the READ_BL_LEN parameter from the CSD
/// pCSD Pointer to the CSD register
/// @return Raw bits for the READ_BL_LEN field
BYTE GetReadBlLen(PCBYTE pCSD);

// ##### Private Implementation #####

BYTE GetTranSpeedByte(PCBYTE pCSD) {
    return pCSD[TRANSPEED_U8OFFSET];
}

BYTE GetReadBlLen(PCBYTE pCSD) {
    return (pCSD[READBLLEN_U8OFFSET]) & READBLLEN_MASK;
}

// ##### Public Implementation #####

SdCsdVersion SdCsdGetVersion(PCCsdRegister pCSD) {
    PCBYTE pRaw = (PCBYTE)pCSD->Raw;

    // Version is written in the first byte and the SdCsdVersion maps directly to the byte value
    return (SdCsdVersion)(pRaw[CSD_STRUCTURE_U8OFFSET] & CSD_STRUCTURE_MASK);
}

SdCsdValidation SdCsdValidate(PCCsdRegister pCSD) {
    static_assert(sizeof(CsdRegister) == SD_CSD_SIZE);

    if (pCSD == NULL)
        return SdCsdValidationInvalidPointer;

    PCBYTE pRaw = (PCBYTE)pCSD->Raw;

    // Useless to check the field ranges if checksum is not valid
    // NB: last byte is excluded since it contains the CRC
    BYTE crc7 = Crc7Calculate(pRaw, SD_CSD_SIZE - 1);
    if (crc7 != (pRaw[SD_CSD_SIZE - 1] >> 1)) {
        return SdCsdValidationCRCFailed;
    }

    SdCsdVersion version = SdCsdGetVersion(pCSD);
    if (version != SDCSDV1p0 && version != SDCSDV2p0)
        return SdCsdValidationInvalidVersion;

    // Last bit after CRC should always be 1 in all versions
    if ((pRaw[SD_CSD_SIZE - 1] & 0x1) == 0)
        return SdCsdValidationReservedMismatch;

    // Remaining version bytes should be zero
    if ((pRaw[CSD_STRUCTURE_U8OFFSET] & ((BYTE)(~(CSD_STRUCTURE_MASK)))) != 0)
        return SdCsdValidationReservedMismatch;

    // As stated in CSD description [Section 5.3], the TranSpeed should always be 25 Mhz (mandatory)
    // In high speed the max can be 50 MHz. For CSD v2, the specification allows eve higher data rates but,
    // as stated in the specification, a CMD0 reset the limit to 25Mhz
    BYTE tranSpeed = GetTranSpeedByte(pRaw);
    if (tranSpeed != 0x32 && tranSpeed != 0x5A) {
        return SdCsdValidationTranSpeedNotSupported;
    }

    // As stated in CSD description [Section 5.3], Block length can change in the version 1 but is fixed at 512bytes in the version 2
    BYTE readBlLen = GetReadBlLen(pRaw);
    if ((version == SDCSDV1p0 && (readBlLen < 9 || readBlLen > 11)) || (version == SDCSDV2p0 && (readBlLen != 9))) {
        return SdCsdValidationInvalidReadBlLen;
    }

    return SdCsdValidationOk;
}

UInt32 SdCsdGetMaxTransferRate(PCCsdRegister pCSD) {
    // Function is assuming a valid CSD
    BYTE tranSpeed = GetTranSpeedByte(pCSD->Raw);
    UInt32 baseFreq = 100000; // Base frequency is 100Khz

    BYTE freqScaler = (BYTE)(tranSpeed & TRAN_SPEED_UNIT_MASK);
    DebugAssert(freqScaler >= 0 && freqScaler < 4); // Just asserting we are doing things right

    for (BYTE i = 0; i < freqScaler; i++) {
        baseFreq *= 10;
    }

    BYTE timeValue = (BYTE)((tranSpeed & TRAN_SPEED_TIME_MASK) >> TRAN_SPEED_TIME_POS);
    DebugAssert(timeValue > 0 && timeValue < 16); // Just asserting we are doing things right. 0 is reserved
    return (UInt32)(s_tranSpeedValues[timeValue] * (float)baseFreq);
}

UInt16 SdCsdGetMaxReadDataBlockLength(PCCsdRegister pCSD) {
    // Function is assuming a valid CSD
    BYTE exponent = GetReadBlLen(pCSD->Raw);

    DebugAssert(exponent >= 9 && exponent <= 11);
    return (UInt16)(1 << exponent); // Block size = 2 ^ (READ_BL_LEN)
}

void SdCsdDumpValidationResult(SdCsdValidation result)
{
    switch (result)
    {
    case SdCsdValidationOk:
        printf("OK");
        break;
    case SdCsdValidationInvalidPointer:
        printf("invalid pointer provided");
        break;
    case SdCsdValidationCRCFailed:
        printf("CRC error");
        break;
    case SdCsdValidationInvalidVersion:
        printf("invalid version");
        break;
    case SdCsdValidationReservedMismatch:
        printf("reserved fields mismatch");
        break;
    case SdCsdValidationInvalidCCC:
        printf("invalid CCC");
        break;
    case SdCsdValidationTranSpeedNotSupported:
        printf("transfer speed not supported");
        break;
    case SdCsdValidationInvalidReadBlLen:
        printf("invalid read block length");
        break;
    default:
        break;
    }
}
