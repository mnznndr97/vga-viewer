/*
 * sd.h
 *
 *  Created on: Dec 4, 2021
 *      Author: mnznn
 */

#include "stm32f4xx_hal.h"
#include <hal_extensions.h>
#include <typedefs.h>
#include <sd/csd.h>
#include <sd/cid.h>

#ifndef INC_SD_SD_H_
#define INC_SD_SD_H_

typedef enum _SDStatus {
	SDStatusOk = 0,
    SDStatusErrorUnknown = -1,
    SDStatusInvalidParameter = -2,
    SDStatusCommunicationTimeout = -3,
	/// An unknown device is responding over the SPI interface
	SDStatusNotSDCard = -4,
	/// SD card voltage is not supported
	SDStatusVoltageNotSupported = -5,
	/// Initialization timeout
	SDStatusInitializationTimeout = -6,
    /// CRC for the command is not valid
    SDStatusCRCError = -7,
	/// Command not recognized
	SDStatusIllegalCommand = -8,
    /// Address not aligned at block boundaries provided
    SDStatusMisalignedAddress = -9,
    /// Provided parameter outside the valid range (ex out of bound address)
    SDStatusParameterOutOfRange = -10,
	/// Invalid CSD read from SD card
	SDStatusInvalidCSD = -11,
	/// Invalid CID read from SD card
	SDStatusInvalidCID = -12,
    /// CRC of the read data block is not correct
    SDStatusReadCorrupted = -13,
    /// Read operation reported a Card Controller Error
    SDStatusReadCCError = -14,
    /// Data operation reported a failed ECC correction
    SDStatusECCFailed = -15,

} SDStatus;

typedef enum _SDVersion {
	SDVerUnknown = 0, SDVer1pX, SDVer2p0OrLater
} SDVersion;

typedef enum _SDCapacity {
	SDCapacityUnknown = 0, SDCapacityStandard, SDCapacityExtended
} SDCapacity;

typedef enum _SDAddressingMode {
	SDAddressingModeUnknown = 0, SDAddressingModeByte, SDAddressingModeSector
} SDAddressingMode;

/// Describes the current attached sd card. When no card is attached, the struct will be filled with zeroes
typedef struct _SDDescription {
	SDVersion Version;
	SDCapacity Capacity;

	SDCSDValidation CSDValidationStatus;
	SDCIDValidation CIDValidationStatus;

	UInt32 MaxTransferSpeed;
	SDAddressingMode AddressingMode;
} SDDescription;

typedef const SDDescription *PCSDDescription;

/// Initialize the SD SPI interface using the specified devices
/// \param powerGPIO Pin that will be used as power (needed due to the power cicle timing requirement)
/// \param spiHandle Handle to the SPI interface that will be used
/// \return Operation status. Always should be SDStatusOk, SDStatusInvalidParameter if a parameter is NULL or out of range
SDStatus SDInitialize(GPIO_TypeDef *powerGPIO, UInt16 powerPin, SPI_HandleTypeDef *spiHandle);

/// Performs a SD power cycle
/// \return Operation status
SDStatus SDPerformPowerCycle();

/// @brief Tries to open the SPI interface of an SD card
/// @return Status of the operation
SDStatus SDTryConnect();

/// @brief Closes the SPI communiction but leaves the power active
/// @return Status of the operation
SDStatus SDDisconnect();

/// Reads a data from the specified sector
/// \param destination Destination buffer
/// \param sector Sector to read
/// \return Status of the operation
SDStatus SDReadSector(BYTE* destination, UInt32 sector);

/// Reads multiple sequential sectors into the destination buffer
/// \param destination Destination buffer
/// \param sector Sector where to start the read operation
/// \return Status of the operation
SDStatus SDReadSectors(BYTE* destination, UInt32 sector, UInt32 count);

/// Prints the status code description
/// @param status Error code from the SD
void SDDumpStatusCode(SDStatus status);

/// @brief Completly shudown the SPI interface and the the power
/// @return Status of the operation. Should always be SDStatusOk
SDStatus SDShutdown();

#endif /* INC_SD_SD_H_ */
