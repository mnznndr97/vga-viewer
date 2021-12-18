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
	SDStatusOk = 0, SDStatusCommunicationTimeout = -1,
	/// An unknown device is responding over the SPI interface
	SDStatusNotSDCard = -2,
	/// SD card voltage is not supported
	SDStatusVoltageNotSupported = -3,
	/// Initialization timeout
	SDStatusInitializationTimeout = -4,
	/// CRC of the read data block is not correct
	SDStatusReadCorrupted = -5,
	/// Invalid CSD read from SD card
	SDStatusInvalidCSD = -6,
	/// Invalid CID read from SD card
	SDStatusInvalidCID = -7
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
/// \return Operation status. Always should be SDStatusOk
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
/// \param sector Sector to read
/// \return Status of the operation
SDStatus SDReadSector(UInt32 sector, BYTE* destination);

/// Prints the status code description
/// @param status Error code from the SD
void SDDumpStatusCode(SDStatus status);

/// @brief Completly shudown the SPI interface and the the power
/// @return Status of the operation
SDStatus SDShutdown();

#endif /* INC_SD_SD_H_ */
