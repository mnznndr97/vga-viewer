/*
 * This file contains the headers of the SD specific CSD register manipulation.
 * The implementation is focused only in a base validation and read of the patrameters we need 
 * in the project (TranSpeed and BlockSize). No other operation are permitted on the CSD
 * Validation Status is represented by the SdCsdValidation enumeration
 * 
 * For more information see SD Physical Layer Simplified Specification - Section 5.3 (CSD register)
 *
 *  Created on: Dec 11, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_SD_CSD_H_
#define INC_SD_CSD_H_

/// Size of the CSD register in bytes
#define SD_CSD_SIZE 16
/// Macro for generating a class based on its offset
#define SD_CLASS_OFFSET(x) (1 << (x))

#include <typedefs.h>

/// CSD register definition
typedef struct _CSDRegister CsdRegister;
/// CSD const pointer definition
typedef const CsdRegister* PCCsdRegister;

typedef enum _SdCsdValidation {
    SdCsdValidationOk,
    /// Null pointer passed to the validation function
    SdCsdValidationInvalidPointer,
    /// CSD data is corrupted
    SdCsdValidationCRCFailed,
    /// Invalid or unknown version
    SdCsdValidationInvalidVersion,
    /// Mismatch on the value of some reserved fields
    SdCsdValidationReservedMismatch,
    /// Invalid CCC configuration found
    SdCsdValidationInvalidCCC,
    /// TranSpeed parameter is not supported
    SdCsdValidationTranSpeedNotSupported,
    /// Invalid READ_BL_LEN configuration found
    SdCsdValidationInvalidReadBlLen,
}SdCsdValidation;

/// Version of the CSD register
/// \remarks: enum values match the values in the CSD version byte
typedef enum _SdCsdVersion {
    // Standard capacity SD card CSD
    SDCSDV1p0 = 0,
    // High capacity and Extended Capacity SD card CSD
    SDCSDV2p0 = 0x40,
    // SUDC
    SDCSDV3p0 = 0x80,
    // Reserved
    SDCSDReserved = 0xC0,
} SdCsdVersion;

typedef enum _SdClasses {
    SDClass0 = SD_CLASS_OFFSET(0),
    SDClass1 = SD_CLASS_OFFSET(1),
    SDClass2 = SD_CLASS_OFFSET(2),
    SDClass3 = SD_CLASS_OFFSET(3),
    SDClass4 = SD_CLASS_OFFSET(4),
    SDClass5 = SD_CLASS_OFFSET(5),
    SDClass6 = SD_CLASS_OFFSET(6),
    SDClass7 = SD_CLASS_OFFSET(7),
    SDClass8 = SD_CLASS_OFFSET(8),
    SDClass9 = SD_CLASS_OFFSET(9),
    SDClass10 = SD_CLASS_OFFSET(10),
    SDClass11 = SD_CLASS_OFFSET(11),
} SdClasses;

/// Get the version of the current CSD
/// @param pCSD Valid pointer to the CSD register
/// @return Version of the CSD
SdCsdVersion SdCsdGetVersion(PCCsdRegister pCSD);
/// Get the max transfer speed written in the CSD
/// @param pCSD Pointer to a CSD register structure
/// @return Valid pointer assumed
UInt32 SdCsdGetMaxTransferRate(PCCsdRegister pCSD);
/// Get the block transfer size SD is expecting
/// @param pCSD Pointer to a CSD register structure
/// @return Valid pointer assumed
UInt16 SdCsdGetMaxReadDataBlockLength(PCCsdRegister pCSD);
/// @brief Dumps a validation status on the console
/// @param result Validation status
void SdCsdDumpValidationResult(SdCsdValidation result);
/// Validates a pointer to a CSD register
/// @param pCSD 
/// @return Status of the CSD validation
SdCsdValidation SdCsdValidate(PCCsdRegister pCSD);

#endif /* INC_SD_CSD_H_ */
