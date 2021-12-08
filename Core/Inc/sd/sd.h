/*
 * sd.h
 *
 *  Created on: Dec 4, 2021
 *      Author: mnznn
 */

#include "stm32f4xx_hal.h"
#include <hal_extensions.h>
#include <typedefs.h>

#ifndef INC_SD_SD_H_
#define INC_SD_SD_H_

typedef enum _SDStatus {
	SDStatusOk = 0
} SDStatus;

/// Initialize the SD SPI interface using the specified devices
/// \param powerGPIO Pin that will be used as power (needed due to the power cicle timing requirement)
/// \param spiHandle Handle to the SPI interface that will be used
/// \return Operation status
SDStatus SDInitialize(GPIO_TypeDef *powerGPIO, UInt16 powerPin, SPI_HandleTypeDef *spiHandle);

/// Performs a SD power cycle
/// \return Operation status
SDStatus SDPerformPowerCycle();

/// @brief Tries to open the SPI interface of an SD card
/// @return Status of the operation
SDStatus SDTryConnect();

#endif /* INC_SD_SD_H_ */
