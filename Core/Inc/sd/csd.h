/*
 * csd.h
 *
 *  Created on: 11 dic 2021
 *      Author: mnznn
 */

#ifndef INC_SD_CSD_H_
#define INC_SD_CSD_H_

/// Size of the CSD register in bytes
#define SD_CSD_SIZE 16

#define SD_CLASS_OFFSET(x) (1 << (x))

#include <typedefs.h>

/// CSD register definition
typedef struct _CSDRegister CSDRegister;
/// CSD const pointer definition
typedef const CSDRegister *PCCSDRegister;

typedef enum _SDCSDValidation {
    SDCSDValidationOk,
    /// Null pointer passed to the validation function
    SDCSDValidationInvalidPointer,
    /// CSD data is corrupted
    SDCSDValidationCRCFailed,
    /// Invalid or unknown version
    SDCSDValidationInvalidVersion,
    /// Mismatch on the value of some reserved fields
    SDCSDValidationReservedMismatch,
    /// Invalid CCC configuration found
    SDCSDValidationInvalidCCC,
    /// TranSpeed parameter is not supported
    SDCSDValidationTranSpeedNotSupported,
    /// Invalid READ_BL_LEN configuration found
    SDCSDValidationInvalidReadBlLen,
}SDCSDValidation;

/// Version of the CSD register
typedef enum _SDCSDVersion {
	// Standard capacity SD card CSD
	SDCSDV1p0 = 0,
	// High capacity and Extended Capacity SD card CSD
	SDCSDV2p0 = 1,
	// SUDC
	SDCSDV3p0 = 2,
	// Reserved
	SDCSDReserved = 3,
} SDCSDVersion;

typedef enum _SDClasses {
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
} SDClasses;

/// Get the version of the current CSD
/// @param pCSD Valid pointer to the CSD register
/// @return Version of the CSD
SDCSDVersion SDCSDGetVersion(PCCSDRegister pCSD);

/// Validates a pointer to a CSD register
/// @param pCSD 
/// @return Status of the CSD validation
SDCSDValidation SDCSDValidate(PCCSDRegister pCSD);

UInt32 SDCSDGetMaxTransferRate(PCCSDRegister pCSD);
UInt16 SDCSDGetMaxReadDataBlockLength(PCCSDRegister pCSD);

#endif /* INC_SD_CSD_H_ */
