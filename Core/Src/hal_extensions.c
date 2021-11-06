/*
 * hal_extensions.c
 *
 *  Created on: Nov 4, 2021
 *      Author: mnznn
 */

#include <hal_extensions.h>

HAL_StatusTypeDef HAL_UART_TransmitAndWait_DMA(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t timeout) {
	/* Check that a Tx process is not already ongoing */
	if (huart->gState != HAL_UART_STATE_READY) {
		return HAL_BUSY;
	}

	if ((pData == NULL) || (Size == 0U)) {
		return HAL_ERROR;
	}

	huart->pTxBuffPtr = pData;
	huart->TxXferSize = Size;
	huart->TxXferCount = Size;

	huart->ErrorCode = HAL_UART_ERROR_NONE;
	huart->gState = HAL_UART_STATE_BUSY_TX;

	/* Enable the UART transmit DMA stream */
	HAL_StatusTypeDef status = HAL_DMA_Start(huart->hdmatx, (uint32_t) pData, (uint32_t) &huart->Instance->DR, Size);
	if (status != HAL_OK) {
		return status;
	}

	/* Clear the TC flag in the SR register by writing 0 to it */
	__HAL_UART_CLEAR_FLAG(huart, UART_FLAG_TC);

	/* Enable the DMA transfer for transmit request by setting the DMAT bit
	 in the UART CR3 register */
	ATOMIC_SET_BIT(huart->Instance->CR3, USART_CR3_DMAT);
	status = HAL_DMA_PollForTransfer(huart->hdmatx, HAL_DMA_FULL_TRANSFER, timeout);
	ATOMIC_CLEAR_BIT(huart->Instance->CR3, USART_CR3_DMAT);

	return status;
}
