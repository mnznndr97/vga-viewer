/*
 * screen.c
 *
 *  Created on: Oct 18, 2021
 *      Author: mnznn
 */

#include <screen\screen.h>
#include <assertion.h>
#include "stm32f4xx_hal.h"

extern UInt32 _currentLineAddr;
extern BYTE _currentLinePrescaler;

void ScreenCheckVideoLineEnded() {
	if (READ_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_EN) != 0) {
		// DMA is still enabled. Something went wrong
		Error_Handler();
	}
	UInt32 dataStilltoRead = DMA_SCREEN_STREAM->NDTR;
	if (dataStilltoRead > 0) {
		// DMA is disabled but not all data have been consumed. It cannot keep
		// up with the screen times
		Error_Handler();
	}
}

void ScreenEnableDMA() {
	// ENABLE Bit for the DMA should already have been set. We just enable the trigger request
	SET_BIT(TIM1->DIER, TIM_DMA_TRIGGER);
	// SET_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_EN);
}

void ScreenDisableDMA() {
	BOOL dmaEnabled = READ_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_EN) != 0;
	UInt32 dataStilltoRead = DMA_SCREEN_STREAM->NDTR;
	if (dmaEnabled && dataStilltoRead > 0) {
		Error_Handler();
	}

	CLEAR_BIT(TIM1->DIER, TIM_DMA_TRIGGER);
	// We prepare the DMA to preload the next line

	// We have to wait that DMA_SxCR_EN is effectively set to 0
	while (READ_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_EN) != 0)
		;

	// Let's check if something has gone wrong
	// We read the register a single time and then we do all the checks
	UInt32 lisr = DMA2->LISR;
	if (READ_BIT(lisr, DMA_LISR_DMEIF0)) {
		// Stream direct mode error
		Error_Handler();
	}
	if (READ_BIT(lisr, DMA_LISR_TEIF0)) {
		// Stream transfer mode error
		Error_Handler();
	}
	if (READ_BIT(lisr, DMA_LISR_FEIF0)) {
		// FIFO transfer mode error
		//Error_Handler();
	}

	SET_BIT(DMA2->LIFCR, DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CFEIF0);
	DMA_SCREEN_STREAM->NDTR = SCREENBUF;
	DMA_SCREEN_STREAM->M0AR = _currentLineAddr;

	SET_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_EN);

	_currentLinePrescaler = (_currentLinePrescaler + 1) % 2;
	if (_currentLinePrescaler == 0) {
		_currentLineAddr += SCREENBUF;
	}

}

void ScreenInitialize(TIM_TypeDef *hSyncTimer, TIM_TypeDef *vSyncTimer) {
	// Assuming a 40Mhz clock for the moment
	const int HFrontPorch = 40;
	const int HVisibleArea = 800;
	const int HSyncPulse = 128;
	const int HBackPorch = 88;
	// Due to the PWM mode of our timers, the line starts with a back porch
	const int WholeLine = HBackPorch + HFrontPorch + HVisibleArea + HSyncPulse;

	const int VFrontPorch = 1;
	const int VVisibleArea = 600;
	const int VSyncPulse = 4;
	const int VBackPorch = 23;
	// Due to the PWM mode of our timers, the frame starts with a back porch
	const int WholeFrame = VBackPorch + VFrontPorch + VVisibleArea + VSyncPulse;

	/* *** Horizontal sync setup *** */

	const int divider = 2;

	// We set the clock division by clearing the bits and setting the new ones
	hSyncTimer->ARR = (WholeLine / divider) - 1;
	hSyncTimer->CNT = 0;

	hSyncTimer->CCR1 = (WholeLine - HSyncPulse) / divider; // Main HSYNC signal
	hSyncTimer->CCR2 = (HBackPorch) / divider; // Black porch VSYNC trigger
	hSyncTimer->CCR3 = ((HBackPorch) / divider) - 15; // DMA start (video line render start)
	hSyncTimer->CCR4 = ((HBackPorch + HVisibleArea) / divider) + 8; // DMA end (video line render end)

	DebugAssert(hSyncTimer->CCR1 >= 0);
	DebugAssert(hSyncTimer->CCR2 >= 0 && hSyncTimer->CCR2 < hSyncTimer->CCR1);
	DebugAssert(hSyncTimer->CCR3 >= 0 && hSyncTimer->CCR3 < hSyncTimer->CCR1);
	DebugAssert(hSyncTimer->CCR4 >= 0 && hSyncTimer->CCR4 >= hSyncTimer->CCR3 && hSyncTimer->CCR4 < hSyncTimer->CCR1);

	/* *** Vertical sync setup *** */

	//vSyncTimer->PSC = 3;
	vSyncTimer->ARR = (WholeFrame) - 1;
	vSyncTimer->CNT = 0;

	vSyncTimer->CCR1 = (WholeFrame - VSyncPulse);
	vSyncTimer->CCR2 = (VBackPorch);				// VideoStart signal
	vSyncTimer->CCR3 = (VBackPorch + VVisibleArea); // VideoEnd signal

	//SET_BIT(TIM1->DIER, TIM_DMA_TRIGGER);

	// DMA Destination is fixed so we can simply set it one time only
	DMA_SCREEN_STREAM->PAR = (uint32_t) (((char*) &GPIOC->ODR) + 0);
	CLEAR_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_DBM);
	DMA_SCREEN_STREAM->NDTR = SCREENBUF;
	DMA_SCREEN_STREAM->M0AR = _currentLineAddr;

	SET_BIT(DMA_SCREEN_STREAM->CR, DMA_SxCR_EN);

	_currentLinePrescaler = (_currentLinePrescaler + 1) % 4;
	if (_currentLinePrescaler == 0) {
		_currentLineAddr += SCREENBUF;
	}
}
