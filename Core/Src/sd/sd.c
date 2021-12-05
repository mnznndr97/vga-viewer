/*
 * sd.c
 *
 *  Created on: Dec 4, 2021
 *      Author: mnznn
 */

#include <sd/sd.h>
#include <stdio.h>


extern void Error_Handler();

static GPIO_TypeDef *_powerGPIO = NULL;
static UInt16 _powerPin = NULL;

static GPIO_TypeDef* _nssGPIO = NULL;
static UInt16 _nssPin = NULL;
static SPI_HandleTypeDef *_spiHandle = NULL;

/// Completly disables the SPI interface
static void ShutdownSDSPIInterface() {
    // We need to complety disable our SPI interface
    // Let's follow the procedure indicate in the SPI chapter of RM0090 [Section 28.3.8]

    // Let's wait to have received the last data 
    while (READ_BIT(_spiHandle->Instance->SR, SPI_SR_RXNE) == 0);
    // Let' s wait for a transfer to complete (we are in full duplex bidirectional mode)
    while (READ_BIT(_spiHandle->Instance->SR, SPI_SR_TXE) == 0);
    // Let' s wait for the channel to be freen
    while (READ_BIT(_spiHandle->Instance->SR, SPI_SR_BSY) == 1);
    __HAL_SPI_DISABLE(_spiHandle);


    // We don't have to do anything with the CS signal for the moment
}

// ##### Public Function definitions #####

SDStatus SDInitialize(GPIO_TypeDef *powerGPIO, UInt16 powerPin, SPI_HandleTypeDef *spiHandle) {
	_powerGPIO = powerGPIO;
	_powerPin = powerPin;

    _nssGPIO = GPIOB;
    _nssPin = GPIO_PIN_12;

	_spiHandle = spiHandle;

	// To initialize the SD layer, let's perform a power cycle in case our system where resetted but the SD card didn' t have time to perform 
	// the power cycle correctly
	return SDPerformPowerCycle();
}

SDStatus SDPerformPowerCycle() {
	// Preamble: we shutdown the SPI interface to later re-enabled it
	HAL_StatusTypeDef abortStatus = HAL_SPI_Abort(_spiHandle);

	/* Reference specification : Physical Layer Simplified Specification Version 8.00[Section 6.4.1.2] */

	// We need to lower the VDD level under the 0.5V threshold for at least 1 ms
	// The other lines should also be low in this phase  
	// Let's do 10ms to be sure
	HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_RESET);
	HAL_Delay(10);

    // CS shall be kept high during initialization as stated in section 6.4.1.1
    HAL_GPIO_WritePin(_nssGPIO, _nssPin, GPIO_PIN_SET);

	// We can now enable the GPIO, and wait another 1 ms (at least) to let the VDD stabilize
	// There are some constraints also on the slope on the VDD change but we can do little about this
	HAL_GPIO_WritePin(_powerGPIO, _powerPin, GPIO_PIN_SET);
	HAL_Delay(10);


    GPIO_PinState lastCardState = GPIO_PIN_RESET;


    while (true) {
        GPIO_PinState currentState = HAL_GPIO_ReadPin(_nssGPIO, _nssPin);
        if (currentState != lastCardState) {

            if (currentState == GPIO_PIN_RESET) {
                printf("Card disconnected\r\n");
            }
            else {
                printf("Card connected\r\n");
            }

            lastCardState = currentState;
            HAL_Delay(1000);
        }
    }

	// After these step, the SD card (if alredy inserted) should be ready to receive the initialization sequence
	return SDStatusOk;
}
