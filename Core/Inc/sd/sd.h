/*
 * This file contains the headers for the SD power management, connection and data read.
 * Data write is not implemented
 * Each operation status is represented by the SdStatus enumeration
 *
 * For more information see SD Physical Layer Simplified Specification @ https://www.sdcard.org/downloads/pls/
 *
 *  Created on: Dec , 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_SD_SD_H_
#define INC_SD_SD_H_

#include "stm32f4xx_hal.h"
#include <hal_extensions.h>
#include <typedefs.h>
#include <sd/csd.h>

/// SD operation status
typedef enum _SdStatus {
    SdStatusOk = 0, 
    SdStatusErrorUnknown = -1,
    SdStatusInvalidParameter = -2,
    SdStatusCommunicationTimeout = -3,
    /// An unknown device is responding over the SPI interface
    SdStatusNotSDCard = -4,
    /// SD card voltage is not supported
    SdStatusVoltageNotSupported = -5,
    /// Initialization timeout
    SdStatusInitializationTimeout = -6,
    /// CRC for the command is not valid
    SdStatusCRCError = -7,
    /// Command not recognized
    SdStatusIllegalCommand = -8,
    /// Address not aligned at block boundaries provided
    SdStatusMisalignedAddress = -9,
    /// Provided parameter outside the valid range (ex out of bound address)
    SdStatusParameterOutOfRange = -10,
    /// Invalid CSD read from SD card
    SdStatusInvalidCSD = -11,
    /// Invalid CID read from SD card
    SdStatusInvalidCID = -12,
    /// CRC of the read data block is not correct
    SdStatusReadCorrupted = -13,
    /// Read operation reported a Card Controller Error
    SdStatusReadCCError = -14,
    /// Data operation reported a failed ECC correction
    SdStatusECCFailed = -15,
} SdStatus;

/// SD Card version
typedef enum _SdVersion {
    SdVerUnknown = 0, SdVer1pX, SdVer2p0OrLater
} SdVersion;

/// SD Card capacity
typedef enum _SdCapacity {
    SdCapacityUnknown = 0, SdCapacityStandard, SdCapacityExtended
} SdCapacity;

/// Block addressing mode 
typedef enum _SDAddressingMode {
    SDAddressingModeUnknown = 0, SDAddressingModeByte, SDAddressingModeSector
} SDAddressingMode;

/// Describes the current attached sd card. When no card is attached, the struct will be filled with zeroes
typedef struct _SDDescription {
    /// Version of the SD card
    SdVersion Version;
    /// Capacity type of the SD card
    SdCapacity Capacity;
    /// Addressing mode for the SD card
    SDAddressingMode AddressingMode;
    
    /// CSD validation status
    SdCsdValidation CSDValidationStatus;
    /// Max transfer speed calculated from CSD
    UInt32 MaxTransferSpeed;
    /// Max block length for the read transactions calculated from CSD
    UInt16 BlockLen;
} SDDescription;
typedef const SDDescription* PCSDDescription;

/// Initialize the SD SPI interface using the specified devices
/// \param powerGPIO GPIO pin port that will be used as power (needed due to the power cicle timing requirement)
/// \param powerPin Pin number for the GPIO
/// \param spiHandle Handle to the SPI interface that will be used
/// \return Operation status. Always should be SdStatusOk, SDStatusInvalidParameter if a parameter is NULL or out of range
SdStatus SdInitialize(GPIO_TypeDef* powerGPIO, UInt16 powerPin, SPI_HandleTypeDef* spiHandle);

/// Performs a SD power cycle
/// \return Operation status
SdStatus SdPerformPowerCycle();

/// @brief Tries to open the SPI interface of an SD card
/// @return Status of the operation
SdStatus SdTryConnect();

/// @brief Closes the SPI communiction but leaves the power active
/// @return Status of the operation
SdStatus SdDisconnect();

/// Reads a data from the specified sector
/// \param destination Destination buffer
/// \param sector Sector to read
/// \return Status of the operation
SdStatus SdReadSector(BYTE* destination, UInt32 sector);

/// Reads multiple sequential sectors into the destination buffer
/// \param destination Destination buffer
/// \param sector Sector where to start the read operation
/// \return Status of the operation
SdStatus SdReadSectors(BYTE* destination, UInt32 sector, UInt32 count);

/// Prints the status code description
/// @param status Error code from the SD
void SdDumpStatusCode(SdStatus status);

/// @brief Completly shudown the SPI interface and the the power
/// @return Status of the operation. Should always be SdStatusOk
SdStatus SdShutdown();

#endif /* INC_SD_SD_H_ */
