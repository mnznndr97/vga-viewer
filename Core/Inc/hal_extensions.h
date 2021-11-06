/*
 * hal_extensions.h
 *
 *  Created on: Nov 4, 2021
 *      Author: mnznn
 */

#ifndef INC_HAL_EXTENSIONS_H_
#define INC_HAL_EXTENSIONS_H_

#include "stm32f4xx_hal.h"

///
/// @brief  Sends an amount of data in DMA mode as waits for the transfer to complete.
/// @note   When UART parity is not enabled (PCE = 0), and Word Length is configured to 9 bits (M1-M0 = 01),
///         the sent data is handled as a set of u16. In this case, Size must indicate the number
///         of u16 provided through pData.
/// @param  huart  Pointer to a UART_HandleTypeDef structure that contains
///                the configuration information for the specified UART module.
/// @param  pData Pointer to data buffer (u8 or u16 data elements).
/// @param  Size  Amount of data elements (u8 or u16) to be sent
/// @retval HAL status
///
HAL_StatusTypeDef HAL_UART_TransmitAndWait_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t timeout);

#endif /* INC_HAL_EXTENSIONS_H_ */
