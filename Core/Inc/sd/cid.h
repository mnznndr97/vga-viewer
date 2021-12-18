/*
 * cid.h
 *
 *  Created on: 11 dic 2021
 *      Author: mnznn
 */

#ifndef INC_SD_CID_H_
#define INC_SD_CID_H_

/// Size of the CID register in bytes
#define SD_CID_SIZE 16

#include <typedefs.h>

// CID register definition
typedef struct _CIDRegister CIDRegister;
/// CID const pointer definition
typedef const CIDRegister* PCCIDRegister;

typedef enum _SDCIDValidation {
    SDCIDValidationOk,
    /// Null pointer passed to the validation function
    SDCIDValidationInvalidPointer,
    /// CSD data is corrupted
    SDCIDValidationCRCFailed,
    /// Mismatch on the value of some reserved fields
    SDCIDValidationReservedMismatch    
}SDCIDValidation;

/// Validates a pointer to a CID register
/// @param pCID Pointer to CID register
/// @return Status of the CID validation
SDCIDValidation SDCIDValidate(PCCIDRegister pCSD);

#endif /* INC_SD_CID_H_ */
