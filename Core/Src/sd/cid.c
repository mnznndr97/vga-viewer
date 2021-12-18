/*
 * cid.c
 *
 *  Created on: 11 dic 2021
 *      Author: mnznn
 */

#include <sd/cid.h>
#include <crc/crc7.h>
#include <assertion.h>

struct _CIDRegister {
	BYTE Raw[SD_CID_SIZE];
};

SDCIDValidation SDCIDValidate(PCCIDRegister pCSD) {
	static_assert(sizeof(CIDRegister) == SD_CID_SIZE);

	if (pCSD == NULL)
		return SDCIDValidationInvalidPointer;

	PCBYTE pRaw = (PCBYTE) pCSD->Raw;

	// Useless to check the field ranges if checksum is not valid
	// NB: last byte is excluded since it contains the CRC
	BYTE crc7 = Crc7Calculate(pRaw, SD_CID_SIZE - 1);
	if (crc7 != (pRaw[SD_CID_SIZE - 1] >> 1)) {
		return SDCIDValidationCRCFailed;
	}

	return SDCIDValidationOk;
}

