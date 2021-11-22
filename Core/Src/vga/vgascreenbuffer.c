/*
 * vgascreenbuffer.c
 *
 *  Created on: 1 nov 2021
 *      Author: mnznn
 */

#include <vga/vgascreenbuffer.h>
#include <assertion.h>
#include <ram.h>
#include <stdlib.h>
#include <stdio.h>
#include <console.h>

#ifdef _DEBUG
#define DRAWPIXELASSERT
#endif // _DEBUG

extern void Error_Handler();

// ##### Private forward declarations #####

static VGAError AllocateFrameBuffer(const VGAVisualizationInfo *info, VGAScreenBuffer *buffer);
static VGAError CorrectVideoFrameTimings(const VGAVisualizationInfo *info, VideoFrameInfo *finalTimes);

static void DrawPixel(PointS pixel, const Pen *pen);
static void DrawPixelPack(PointS pixel, const Pen *pen);

static void DisableLineDMA(DMA_Stream_TypeDef *dmaStream);
void HandleDMALineEnd(VGAScreenBuffer *screenBuffer);
static VGAError ValidateTiming(const Timing *timing);
static void ScaleTiming(const Timing *timing, BYTE scale, Timing *dest);
static VGAError SetupMainClockTree(float pixelMHzFreq);
static VGAError SetupTimers(BYTE resScaling, VGAScreenBuffer *screenBuffer);

// ##### Private types declarations #####
typedef enum _VGAState {
	VGAStateStopped, VGAStateSuspended,

	//VGAStateVSync,

	/// VGA is displaying some active line
	///
	/// \remarks This state should be maintained during the whole vertical visible area.
	/// The horizontal blanking of the single line is addressed separately
	VGAStateActive
} VGAState;

struct _VGAScreenBuffer {
	ScreenBuffer base;

	BYTE *BufferPtr;
	UInt32 currentLineOffset;
	UInt16 linePixels;
	UInt32 bufferSize;
	VideoFrameInfo VideoFrameTiming;
	VGAState state;

	SBYTE linePrescaler;
	UInt16 lines;

	/// @brief VGA is in the vertical porch/sync state
	///
	/// @remarks In this stage, the VGA monitor is using the RBG channels as black level calibration so our
	/// signals should be blanked out
	/// Reference: https://electronics.stackexchange.com/a/209494
	BYTE vSyncing;

	TIM_HandleTypeDef *mainPixelClockTimer;
	TIM_HandleTypeDef *hSyncClockTimer;
	TIM_HandleTypeDef *vSyncClockTimer;

	DMA_TypeDef *screenLineDMAController;
	DMA_Stream_TypeDef *screenLineDMAStream;

	/// Cache for clear flags that need to be set to the DMA
	/// \remarks The flags are always the same depending on the stream so we can cache
	/// the value to avoid a calculation each time
	UInt32 dmaClearFlags;
};

VideoFrameInfo VideoFrame800x600at60Hz = { 40, { 800, 40, 128, 88 }, { 600, 1, 4, 23 } };

static VGAScreenBuffer *volatile _activeScreenBuffer = NULL;

// ##### Private Function definitions #####

void TIM1_CC_IRQHandler(void) {
	// IRQ normal bit stuff handling
	// Not much to optimize here
	UInt32 timStatus = TIM1->SR;
	UInt32 isLineStartIRQ = READ_BIT(timStatus, TIM_FLAG_CC3);
	UInt32 isLineEndIRQ = READ_BIT(timStatus, TIM_FLAG_CC4);
	CLEAR_BIT(TIM1->SR, isLineStartIRQ | isLineEndIRQ);

	++_activeScreenBuffer->lines;
	VGAScreenBuffer *screenBuffer = _activeScreenBuffer;
	if (screenBuffer == NULL) {
		// Main timer may have been stopped just when the IRQ was pending
		// We simply ignore the IRQ
		return;
	}

	VGAState currentState = screenBuffer->state;
	if (currentState != VGAStateActive) {
		// Video is not active. Exiting
		return;
	}

	if (screenBuffer->vSyncing) {
		DebugWriteChar('V');

		DMA_Stream_TypeDef *dmaStream = screenBuffer->screenLineDMAStream;
		if (dmaStream->M0AR != (UInt32) screenBuffer->BufferPtr) {
			DebugWriteChar('v');
			// DMA should not be running in our ideal world. But as we already mentioned, the BusMatrix contentions can
			// introduce some latency
			// So what to do in the vSyncing portion of the frame?

			// We have to prepare the DMA for the next line, that means enabling the DMA on the buffer start address
			// Before we ensure that the DMA is disable toghether with the timer trigger

			CLEAR_BIT(screenBuffer->hSyncClockTimer->Instance->DIER, TIM_DMA_TRIGGER);
			DisableLineDMA(dmaStream);

			screenBuffer->linePrescaler = -1;
			screenBuffer->currentLineOffset = 0;
			dmaStream->M0AR = ((UInt32) screenBuffer->BufferPtr);
			dmaStream->NDTR = (UInt32) screenBuffer->linePixels;
			SET_BIT(dmaStream->CR, DMA_SxCR_EN);
		}

	} else if (isLineStartIRQ != 0) {
		// The line start IRQ HAVE TO be the fastest one -> When we are in the porch section (line or frame) we have more
		// "relaxed" time restriction
		// So we must do only one thing. Start the DMA. Everything else must be done in another time
		SET_BIT(screenBuffer->hSyncClockTimer->Instance->DIER, TIM_DMA_TRIGGER);

		// After the DMA start, we can send a little char to the ITM to let the programmer know what is happening
		DebugWriteChar('S');
	} else {
		HandleDMALineEnd(screenBuffer);
	}
}

void TIM3_IRQHandler() {
	uint32_t timStatus = TIM3->SR;
	uint32_t isVisibleFrameStartIRQ = READ_BIT(timStatus, TIM_FLAG_CC2);
	uint32_t isVisibleFrameEndIRQ = READ_BIT(timStatus, TIM_FLAG_CC3);

	if (isVisibleFrameStartIRQ == 0 && isVisibleFrameEndIRQ == 0) {
		// We are handling an interrupt that should not be enabled
		Error_Handler();
	}

	CLEAR_BIT(TIM3->SR, isVisibleFrameStartIRQ | isVisibleFrameEndIRQ);

	// We copy the volatile pointer locally. This may be useless in our "single threaded" event but it can be
	// cached or optimized in a register
	VGAScreenBuffer *screenBuffer = _activeScreenBuffer;
	if (screenBuffer == NULL) {
		// Main timer may have been stopped just when the IRQ was pending
		// We simply ignore the IRQ
		return;
	}

	// Independently of the state, we set the vSyncing flag
	BYTE isVSyncing = isVisibleFrameEndIRQ != 0;

	if (!isVSyncing) {
		screenBuffer->lines = 0;
	}
	screenBuffer->vSyncing = isVSyncing;
	DebugWriteChar('v' + isVSyncing);
}

void HandleDMALineEnd(VGAScreenBuffer *screenBuffer) {
	// We are in the porch section, we have a little more time to do all of our stuff
	/* First thing that we have to do: check that the DMA has completed the transfer */
	// The ideal should check if the DMA is completed, otherwhise we have done something wrong with the timing and "raise" and error
	// There is a little problem though. Even if the DMA stream has the highest priority, it still needs to cross the bus matrix
	// (in case of GPIO output) since the GPIO are directly connected to the AHB1 (we cannot take advantage of the AHBx-APBx direct path).
	// Even if the CortexM is accessing only CCMRAM memory and there are no other "AHB/SRAM" interactions with the bus matrix, as stated in a line
	// of the "BusMatrix arbitration ..." paragraph of AN4031 [Section 2.1.3]:
	/*      "The CPU locks the AHB bus to keep ownership and reduces latency during multiple
	 load / store operations and interrupts entry. This enhances firmware responsiveness but it
	 can result in delays on the DMA transaction"
	 */

	// This means that all our "MUST HAVE" interrupts (HSYNC/VSYNC timers, SysTick and Clock) introduce little delay in the DMA
	// (maybe just one sample delay)
	// Moreover, when calling some scheduling-related function of free-rtos, a PendSV interrupt may be raised, resulting in even more
	// bus matrix contention
	// So there is only one thing for it: even if there are more data to be transferred, we stop the DMA and (if necessary) force the output to low
	// so that the black calibration of the monitor can still do its work
	DMA_Stream_TypeDef *dmaStream = screenBuffer->screenLineDMAStream;
	UInt32 dmaEnabled = READ_BIT(dmaStream->CR, DMA_SxCR_EN);
	if (dmaEnabled != 0) {
		DisableLineDMA(dmaStream);
	}

	DebugWriteChar('E');

	// We can stop the trigger DMA request of the timer. It will be resumed
	// at the start of the visible portion of the area
	__HAL_TIM_DISABLE_DMA(screenBuffer->hSyncClockTimer, TIM_DMA_TRIGGER);

	/* Check for error condition */
	UInt32 lisr = screenBuffer->screenLineDMAController->LISR;

	// TODO Calculate this based on stream number
	UInt32 streamDirectModeErrorFlag = DMA_LISR_DMEIF0;
	UInt32 streamTransferErrorFlag = DMA_LISR_TEIF0;

	if (READ_BIT(lisr, streamDirectModeErrorFlag)) {
		// Stream direct mode error
		Error_Handler();
	}
	if (READ_BIT(lisr, streamTransferErrorFlag)) {
		// Stream transfer mode error
		Error_Handler();
	}

	// As stated in this forum answer (https://community.st.com/s/question/0D50X00009XkaG8/stm32f2xx-spi-dma-fifo-error), it seems that
	// "What the reference manual doesn't make clear is the FIFO error is also triggered by falling below the FIFO threshold."
	// So we have to ignore this error

	/*if (READ_BIT(lisr, DMA_LISR_FEIF0)) {
	 // FIFO transfer mode error
	 Error_Handler();
	 }*/

	/* Preparation of a new DMA request */
	// We clear all the Complete|Half Trasfer completed flags
	SET_BIT(screenBuffer->screenLineDMAController->LIFCR, screenBuffer->dmaClearFlags);

	// Line pixels freq scaling
	// TODO Remove constant
	if ((++screenBuffer->linePrescaler) == 2) {
		screenBuffer->currentLineOffset += screenBuffer->linePixels;
		screenBuffer->linePrescaler = 0;
	}

	// Data items to transfer are always the lines pixels
	// In Mem2Per mode the "items" width are relative to the width of the "peripheral" bus (RM0090 - Section 10.3.10)
	dmaStream->NDTR = screenBuffer->linePixels;
	// Source memory address is simply buffer start + current line offset
	dmaStream->M0AR = ((UInt32) screenBuffer->BufferPtr) + ((UInt32) screenBuffer->currentLineOffset);

	if (screenBuffer->currentLineOffset < screenBuffer->bufferSize) {
		// If the buffer is within the limits, we enable the dma stream
		// This will only preload the data in the FIFO (at least in Mem2Per mode) [AN4031- Section 2.2.2]
		SET_BIT(dmaStream->CR, DMA_SxCR_EN);
	} else {
		// We are at the end of the buffer
		// Soon we will see the beginning of the porch area of the frame
		// We DON'T enable the DMA either so we avoid locking the BusMatrix
		dmaStream->NDTR = 0;
	}

	if (READ_BIT(TIM1->SR, TIM_FLAG_CC3) != 0) {
		Error_Handler();
	}
}

VGAError AllocateFrameBuffer(const VGAVisualizationInfo *info, VGAScreenBuffer *screenBuffer) {
	const VideoFrameInfo *finalTimings = &screenBuffer->VideoFrameTiming;
	// Let's cache our info data in local variables
	Bpp localBpp = info->BitsPerPixel;
	// TODO: If this remains a WORD, we can optimize the blanking writing directly a 4 bytes 0x0
	UInt16 lineVisiblePixels = finalTimings->ScanlineTiming.VisibleArea;
	UInt16 lineBorderPixels = 4;

	// 1) We initialize the framebuffer using the line visible portion + some "safety border" that will remain blanked
	size_t totalLinePixels = lineVisiblePixels + lineBorderPixels;
	size_t framebufferSize = totalLinePixels;

	// Here we setup the screen dimension in the VGAScreenBuffer
	screenBuffer->base.screenSize.width = lineVisiblePixels;
	screenBuffer->base.screenSize.height = totalLinePixels;
	screenBuffer->linePixels = totalLinePixels;

	size_t frameLines = finalTimings->FrameTiming.VisibleArea;
	if (info->DoubleBuffered)
		frameLines *= 2;

	// 2) We multiply by the number of lines (visible area of the frame)
	framebufferSize *= frameLines;

	if (localBpp == Bpp8) {
		// If we have one byte per pixels, let's reflect it on the buffer
		framebufferSize *= 3;
	}

	screenBuffer->bufferSize = framebufferSize;
	screenBuffer->linePrescaler = 0;
	screenBuffer->currentLineOffset = 0;
	screenBuffer->vSyncing = true;

	BYTE *buffer = (BYTE*) ralloc(framebufferSize);
	if (buffer == NULL) {
		return VGAErrorOutOfMemory;
	}

	screenBuffer->BufferPtr = buffer;
	screenBuffer->base.DrawCallback = &DrawPixel;
    screenBuffer->base.DrawPackCallback = &DrawPixelPack;
    screenBuffer->base.packSize = 4;

	// Let's initialize the border pixels -> these will remain untouched for the rest of the application lifetime
	for (size_t line = 0; line < frameLines; line++) {
		for (size_t pixel = lineVisiblePixels; pixel < totalLinePixels; pixel++) {
			if (localBpp == Bpp8) {
				// RGB write in 3 succesive bytes
				size_t currentPixel = (line * totalLinePixels * 3) + (pixel * 3);
				buffer[currentPixel] = 0x00;
				buffer[currentPixel + 1] = 0x00;
				buffer[currentPixel + 2] = 0x00;
			} else {
				// RGB write in 1 single byte
				buffer[line * totalLinePixels + pixel] = 0x00;
			}
		}
	}
	return VGAErrorNone;
}

VGAError CorrectVideoFrameTimings(const VGAVisualizationInfo *info, VideoFrameInfo *newTimings) {
	// All basic parameters (except for timings) should have been checked here
	DebugAssert(info && newTimings);
	DebugAssert(info->Scaling >= 0);

	VGAError result;
	if ((result = ValidateTiming(&info->FrameSignals.FrameTiming)) != VGAErrorNone) {
		return result;
	}
	if ((result = ValidateTiming(&info->FrameSignals.ScanlineTiming)) != VGAErrorNone) {
		return result;
	}

	// Here basic timings are all ok
	// If no scaling is necessary, we simply copy the timings
	// and return
	if (info->Scaling == 1) {
		*newTimings = info->FrameSignals;
		return VGAErrorNone;
	}

	// The PixelFrequencyMHz is simply scaled. Nothing fancy here, it will be up to the clock setup routine to settle any borderline cases
	newTimings->PixelFrequencyMHz = info->FrameSignals.PixelFrequencyMHz / info->Scaling;

	// Same for the pixel values (which have already been checked, so everyting should be ok even aftyer the scaling)
	ScaleTiming(&info->FrameSignals.FrameTiming, info->Scaling, &newTimings->FrameTiming);
	ScaleTiming(&info->FrameSignals.ScanlineTiming, info->Scaling, &newTimings->ScanlineTiming);

	// TODO Check again parameter scaling ??
	return VGAErrorNone;
}

void DrawPixel(PointS pixel, const Pen *pen) {
	VGAScreenBuffer *buffer = _activeScreenBuffer;

#ifdef DRAWPIXELASSERT
    DebugAssert(buffer != NULL);
    DebugAssert(pixel.x >= 0 && pixel.x < buffer->base.screenSize.width);
    DebugAssert(pixel.y >= 0 && pixel.y < buffer->base.screenSize.height);
    DebugAssert(pen != NULL);
#endif // DRAWPIXELASSERT

	ARGB8Color color = pen->color;
	BYTE r = color.components.R;
	BYTE g = color.components.G;
	BYTE b = color.components.B;

	// In case of a Bpp3, R is only 2 bits (the r byte MSB)
	BYTE pixelColor = r >> 6;
	pixelColor |= ((g >> 5) << 2);
	pixelColor |= ((b >> 5) << 5);

	buffer->BufferPtr[pixel.y * buffer->linePixels + pixel.x] = pixelColor;
}

void DrawPixelPack(PointS pixel, const Pen *pen) {
	VGAScreenBuffer *buffer = _activeScreenBuffer;

#ifdef DRAWPIXELASSERT
    DebugAssert(buffer != NULL);
    DebugAssert(pixel.x >= 0 && pixel.x < buffer->base.screenSize.width);
    DebugAssert(pixel.y >= 0 && pixel.y < buffer->base.screenSize.height);
#endif // DRAWPIXELASSERT

	ARGB8Color color = pen->color;
	BYTE r = color.components.R;
	BYTE g = color.components.G;
	BYTE b = color.components.B;

	// In case of a Bpp3, R is only 2 bits (the r byte MSB)
	BYTE pixelColor = r >> 6;
	pixelColor |= ((g >> 5) << 2);
	pixelColor |= ((b >> 5) << 5);

	// TODO: Can this be optimized?
	UInt32 wordPixelColor = pixelColor | pixelColor << 8 | pixelColor << 16 | pixelColor << 24;

	// We need to calculate the pack address. In our case, the pack address must be 32 bit aligned since we are using a 32bit
    // memory access. The processor will throw an exception if the access is not aligned. 
	BYTE *linePtr = &buffer->BufferPtr[pixel.y * buffer->linePixels + pixel.x];
    DebugAssert(((UInt32)linePtr & 0x03) == 0x0);

	UInt32 *wordPtr = ((UInt32*) linePtr);

    // Finally we store our pack
	*wordPtr = wordPixelColor;
}

void DisableLineDMA(DMA_Stream_TypeDef *dmaStream) {
	// If DMA is still enabled, we disable it to interrupt the transfer
	// This is the only thing that has to be done [RM0090 - Section 10.3.14 ]
	CLEAR_BIT(dmaStream->CR, DMA_SxCR_EN);

	// We have to wait that DMA_SxCR_EN is effectively set to 0
	// so we can setup our next transfer
	while (READ_BIT(dmaStream->CR, DMA_SxCR_EN) != 0)
		;

	// We clear the output on the DMA peripheral since we are in the blanking portion and we don't know
	// the exact output value where the transfer was interrupted
	// TODO Assuming 8 bit peripheral for the moment
	*((BYTE*) dmaStream->PAR) = 0x0;
}

void ScaleTiming(const Timing *timing, BYTE scale, Timing *dest) {
	Int32 wholeLine = VGATimingGetSum(timing);

	// NB: We are scaling pixel count values, so an integer division should do
	dest->VisibleArea = timing->VisibleArea / scale;
	dest->FrontPorch = timing->FrontPorch / scale;
	dest->SyncPulse = timing->SyncPulse / scale;
	dest->BackPorch = timing->BackPorch / scale;

	if (wholeLine % scale != 0) {
		// Edge case: VGA timing is not a multiple of the scaler, not much else to do
		return;
	}

	// Common case: VGA timing is a multiple of the scaler, so we can check that all the original pixels have not been lost
	// in the int division
	Int32 realWholeLineScaled = wholeLine / scale;

	Int32 scalingLoss = realWholeLineScaled - VGATimingGetSum(dest);
	// In theory, we should never have a negative loss (scaled pixels are more that the original sum scaled)
	// The code will handle the situation correctly, but it can indicate some logic/programming error
	DebugAssert(scalingLoss >= 0);

	// Assumption: Visible area should always scale correctly. let's just correct the back/front porch
	if (scalingLoss < 0) {
		// TODO Handle negative scaling
		Error_Handler();
	} else if (scalingLoss > 0) {
		// TODO Better logic
		dest->FrontPorch += scalingLoss;
	}
}

VGAError SetupMainClockTree(float pixelMHzFreq) {
	RCC_OscInitTypeDef oscInit = { 0 };
	HAL_RCC_GetOscConfig(&oscInit);

	// We need to configure only the hse oscillator
	DebugAssert(oscInit.OscillatorType == RCC_OSCILLATORTYPE_HSE);
	DebugAssert(oscInit.HSEState == RCC_HSE_BYPASS);
	// Our PLL is On and it takes the clock from our super stable HSE
	DebugAssert(oscInit.PLL.PLLState == RCC_PLL_ON);
	DebugAssert(oscInit.PLL.PLLSource == RCC_PLLSOURCE_HSE);

	// We set the output freq of the PLL to 120MHx = 6 * 20Mhz needed
	// To do this we set the PLLP to 2 (minimum) and PLLM to 4. In this way, our 8 Mhz HSE source clock
	// is bring to 120MHz
	oscInit.PLL.PLLM = 4;
	oscInit.PLL.PLLP = RCC_PLLP_DIV2;
	oscInit.PLL.PLLN = 120;

	/*if (HAL_RCC_OscConfig(&oscInit) != HAL_OK)
	 {
	 Error_Handler();
	 }*/

	RCC_ClkInitTypeDef clkInit = { 0 };
	uint32_t latency;
	HAL_RCC_GetClockConfig(&clkInit, &latency);

	DebugAssert(clkInit.ClockType == (RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2));
	DebugAssert(clkInit.SYSCLKSource == RCC_SYSCLKSOURCE_PLLCLK);
	DebugAssert(clkInit.AHBCLKDivider == RCC_SYSCLK_DIV1);
	clkInit.APB1CLKDivider = RCC_HCLK_DIV4;
	clkInit.APB2CLKDivider = RCC_HCLK_DIV4;

	return VGAErrorNone;
}

VGAError ValidateTiming(const Timing *timing) {
	DebugAssert(timing);

	if (timing->VisibleArea <= 0 || timing->FrontPorch <= 0 || timing->SyncPulse <= 0 || timing->BackPorch <= 0) {
		// Base VGA parameters should all be > 0
		return VGAErrorInvalidParameter;
	}

	if (timing->VisibleArea <= timing->FrontPorch || timing->VisibleArea <= timing->SyncPulse || timing->VisibleArea <= timing->BackPorch) {
		// VGA visible area should be the larger value
		return VGAErrorInvalidParameter;
	}

	return VGAErrorNone;
}

VGAError SetupTimers(BYTE resScaling, VGAScreenBuffer *screenBuffer) {
	UInt32 timersOnAPB1Freq = HAL_RCC_GetPCLK1Freq() * 2;
	UInt32 pixelFreq = screenBuffer->VideoFrameTiming.PixelFrequencyMHz * 1000000;
	UInt32 prescaler = timersOnAPB1Freq / pixelFreq;
	if (prescaler == 0 || prescaler > UINT8_MAX) {
		return VGAErrorInvalidParameter;
	}

	TIM_TypeDef *mainTimHandle = screenBuffer->mainPixelClockTimer->Instance;
	mainTimHandle->ARR = prescaler - 1;
	mainTimHandle->CNT = 0;

	const Timing *hTiming = &screenBuffer->VideoFrameTiming.ScanlineTiming;
	const Timing *vTiming = &screenBuffer->VideoFrameTiming.FrameTiming;
	// Due to the PWM mode of our timers, the line starts with a back porch

	// Due to the PWM mode of our timers, the line/frame starts with a back porch
	// The timers will be high for BPorch + Visible + FPorch time and go low for the Syn time
	int wholeLine = VGATimingGetSum(hTiming);
	int wholeFrame = VGATimingGetSum(vTiming);

	/* *** Horizontal sync setup *** */

// We set the clock division by clearing the bits and setting the new ones
	TIM_TypeDef *hSyncTimer = screenBuffer->hSyncClockTimer->Instance;
	hSyncTimer->ARR = wholeLine - 1;
	hSyncTimer->CNT = 0;

	hSyncTimer->CCR1 = wholeLine - hTiming->SyncPulse; // Main HSYNC signal
	hSyncTimer->CCR2 = hTiming->BackPorch; // Black porch VSYNC trigger
	hSyncTimer->CCR3 = hTiming->BackPorch - 15; // DMA start (video line render start)
	hSyncTimer->CCR4 = hTiming->BackPorch + hTiming->VisibleArea + 8; // DMA end (video line render end)

	DebugAssert(hSyncTimer->CCR1 >= 0);
	DebugAssert(hSyncTimer->CCR2 >= 0 && hSyncTimer->CCR2 < hSyncTimer->CCR1);
	DebugAssert(hSyncTimer->CCR3 >= 0 && hSyncTimer->CCR3 < hSyncTimer->CCR1);
	DebugAssert(hSyncTimer->CCR4 >= 0 && hSyncTimer->CCR4 >= hSyncTimer->CCR3 && hSyncTimer->CCR4 < hSyncTimer->CCR1);

	/* *** Vertical sync setup *** */
// HSync will trigger the VSync at each line using the correct vga timing, so we have to prescale the
// timer at the input
	TIM_TypeDef *vSyncTimer = screenBuffer->vSyncClockTimer->Instance;
	vSyncTimer->ARR = wholeFrame - 1;
	vSyncTimer->CNT = 0;
	vSyncTimer->PSC = resScaling - 1;

	vSyncTimer->CCR1 = wholeFrame - vTiming->SyncPulse;
	vSyncTimer->CCR2 = vTiming->BackPorch;				// VideoStart signal
	vSyncTimer->CCR3 = vTiming->BackPorch + vTiming->VisibleArea; // VideoEnd signal

	DebugAssert(vSyncTimer->CCR1 >= 0);
	DebugAssert(vSyncTimer->CCR2 >= 0 && vSyncTimer->CCR2 < vSyncTimer->CCR1);
	DebugAssert(vSyncTimer->CCR3 >= 0 && vSyncTimer->CCR3 < vSyncTimer->CCR1 && vSyncTimer->CCR3 > vSyncTimer->CCR2);

	vSyncTimer->EGR = TIM_EGR_UG;

	return VGAErrorNone;
}

// ##### Public Function definitions #####

VGAError VGACreateScreenBuffer(const VGAVisualizationInfo *visualizationInfo, ScreenBuffer **screenBuffer) {
	if (!visualizationInfo || !screenBuffer) {
		return VGAErrorInvalidParameter;
	}

	if (!visualizationInfo->mainTimer || !visualizationInfo->hSyncTimer || !visualizationInfo->vSyncTimer) {
		return VGAErrorInvalidParameter;
	}

	VGAScreenBuffer *vgaScreenBuffer = (VGAScreenBuffer*) malloc(sizeof(VGAScreenBuffer));
	*screenBuffer = (ScreenBuffer*) vgaScreenBuffer;
	if (*screenBuffer == NULL) {
		// Cannot allocate memory for ScreenBuffer
		return VGAErrorOutOfMemory;
	}

	if (visualizationInfo->FrameSignals.PixelFrequencyMHz <= 0.0f) {
		return VGAErrorInvalidParameter;
	}

	// For now we support the exact 40MHz frequency
	if (visualizationInfo->FrameSignals.PixelFrequencyMHz != 40.0f) {
		return VGAErrorNotSupported;
	}
	if (visualizationInfo->Scaling != 2) {
		return VGAErrorNotSupported;
	}

	VGAError result;
	VideoFrameInfo scaledVideoTimings;
	if ((result = CorrectVideoFrameTimings(visualizationInfo, &scaledVideoTimings)) != VGAErrorNone) {
		return result;
	}

	vgaScreenBuffer->state = VGAStateStopped;
	vgaScreenBuffer->vSyncing = 0;
	vgaScreenBuffer->base.bitsPerPixel = visualizationInfo->BitsPerPixel;
	vgaScreenBuffer->VideoFrameTiming = scaledVideoTimings;

	// Before configuring clock, we try to allocate our buffer to see if there is enougth memory
	if ((result = AllocateFrameBuffer(visualizationInfo, vgaScreenBuffer)) != VGAErrorNone) {
		return result;
	}

	vgaScreenBuffer->mainPixelClockTimer = visualizationInfo->mainTimer;
	vgaScreenBuffer->hSyncClockTimer = visualizationInfo->hSyncTimer;
	vgaScreenBuffer->vSyncClockTimer = visualizationInfo->vSyncTimer;

	vgaScreenBuffer->screenLineDMAController = DMA2;
	vgaScreenBuffer->screenLineDMAStream = visualizationInfo->lineDMA->Instance;
	// TODO Calculate this based on stream number
	vgaScreenBuffer->dmaClearFlags = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CFEIF0;

	/*if ((result = SetupMainClockTree(scaledVideoTimings.PixelFrequencyMHz)) != VGAErrorNone) {
	 return result;
	 }*/

	if ((result = SetupTimers(visualizationInfo->Scaling, vgaScreenBuffer)) != VGAErrorNone) {
		return result;
	}

	if (visualizationInfo->BitsPerPixel == Bpp3) {
		// When using the 3 bpp visualization, we use the low 8 GPIOE pins to output our colors
		// (3 bits for blue, 3 bits for green, 2 bits for red, in "little endian" order (Red -> [0, 1], Green -> [2, 4], Blu -> [5-7])
		vgaScreenBuffer->screenLineDMAStream->PAR = (uint32_t) &GPIOE->ODR;
		vgaScreenBuffer->screenLineDMAStream->M0AR = (uint32_t) vgaScreenBuffer->BufferPtr;
		vgaScreenBuffer->screenLineDMAStream->NDTR = (uint32_t) vgaScreenBuffer->linePixels;

	}
	_activeScreenBuffer = vgaScreenBuffer;
	return VGAErrorNone;
}

Int32 VGATimingGetSum(const Timing *timing) {
	if (!timing)
		return -1;
	return timing->VisibleArea + timing->FrontPorch + timing->SyncPulse + timing->BackPorch;
}

VGAError VGADumpTimersFrequencies() {
	VGAScreenBuffer *screenBuf = _activeScreenBuffer;
	if (screenBuf == NULL) {
		// VGA screen buffer not allocated and registered
		return VGAErrorInvalidState;
	}

	UInt32 timersOnAPB1Freq = HAL_RCC_GetPCLK1Freq() * 2;
	UInt32 timersOnAPB2Freq = HAL_RCC_GetPCLK2Freq() * 2;

	// Let's print first the main timer frequency

	printf("Main timer:\r\n");
	printf("\tInput frequency (from APB1): ");
	FormatFrequency(timersOnAPB1Freq);
	printf("\r\n");

	TIM_TypeDef *mainTim = screenBuf->mainPixelClockTimer->Instance;
	float mainTimerFreq = timersOnAPB2Freq / (mainTim->PSC + 1.0f);

	printf("\tPrescaled frequency: ");
	FormatFrequency(mainTimerFreq);
	printf("\r\n");

	mainTimerFreq = timersOnAPB2Freq / (mainTim->ARR + 1.0f);
	printf("\tEffetive frequency: ");
	FormatFrequency(mainTimerFreq);
	printf("\r\n");

	// HSync timer info print
	TIM_TypeDef *hSyncTim = screenBuf->hSyncClockTimer->Instance;
	printf("HSync timer:\r\n");
	printf("\tInput frequency (from trigger timer): ");
	FormatFrequency(mainTimerFreq);
	printf("\r\n");

	float hSyncFreq = mainTimerFreq / (hSyncTim->ARR + 1.0f);
	printf("\tSignal frequency: ");
	FormatFrequency(hSyncFreq);
	printf("\r\n");

	// VSync timer info print
	TIM_TypeDef *vSyncTim = screenBuf->vSyncClockTimer->Instance;
	printf("VSync timer:\r\n");
	printf("\tInput frequency (from trigger hsync): ");
	FormatFrequency(hSyncFreq);
	printf("\r\n");

	float vSyncFreq = hSyncFreq / (vSyncTim->PSC + 1.0f);
	printf("\tPrescaled frequency: ");
	FormatFrequency(vSyncFreq);
	printf("\r\n");

	vSyncFreq = vSyncFreq / (vSyncTim->ARR + 1.0f);
	printf("\tSignal frequency: ");
	FormatFrequency(vSyncFreq);
	printf("\r\n");
}

VGAError VGAStartOutput() {
	VGAScreenBuffer *screenBuf = _activeScreenBuffer;
	if (screenBuf == NULL) {
		// VGA screen buffer not allocated and registered
		return VGAErrorInvalidState;
	}

	if (screenBuf->state != VGAStateStopped) {
		// VGA screen buffer not allocated and registered
		return VGAErrorInvalidState;
	}

	// Everything should be ok here. Buffer is allocated and timers are hopefully setted correctly
	// We can start our timers

	// First we start the Hsync timer. The timer will not run until the main timer is started
	// Main HSync signal. Does not require interrupt handling since it is feeded directly into the monitor
	HAL_TIM_PWM_Start(screenBuf->hSyncClockTimer, TIM_CHANNEL_1);
	// VSync trigger signal. Does not require interrupt handling since it is feeded directly into the slave timer
	HAL_TIM_PWM_Start(screenBuf->hSyncClockTimer, TIM_CHANNEL_2);
	// Visible line start + Visible line end interrupt must be enabled (in PWM in order to have a "loop" ov events each cycle)
	HAL_TIM_PWM_Start_IT(screenBuf->hSyncClockTimer, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start_IT(screenBuf->hSyncClockTimer, TIM_CHANNEL_4);

	// Then we start the Vsync timer. The timer will not run until the HSync timer is started (VSync is slave of HSYNC)
	// Main VSync signal. Does not require interrupt handling since it is feeded directly into the monitor
	HAL_TIM_PWM_Start(screenBuf->vSyncClockTimer, TIM_CHANNEL_1);
	// Visible area start + Visible area end interrupt must be enabled (in PWM in order to have a "loop" ov events each cycle)
	HAL_TIM_PWM_Start_IT(screenBuf->vSyncClockTimer, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start_IT(screenBuf->vSyncClockTimer, TIM_CHANNEL_3);

	/*for (size_t line = 0; line < 75; line++) {
	 for (size_t pixel = 0; pixel < 100; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 0;
	 }
	 for (size_t pixel = 100; pixel < 200; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 1;
	 }
	 for (size_t pixel = 200; pixel < 300; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 2;
	 }
	 for (size_t pixel = 300; pixel < 400; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 3;
	 }
	 }
	 for (size_t line = 75; line < 150; line++) {
	 for (size_t pixel = 0; pixel < 100; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 3;
	 }
	 for (size_t pixel = 100; pixel < 200; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 0;
	 }
	 for (size_t pixel = 200; pixel < 300; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 1;
	 }
	 for (size_t pixel = 300; pixel < 400; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 2;
	 }
	 }

	 for (size_t line = 150; line < 225; line++) {
	 for (size_t pixel = 0; pixel < 100; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 2;
	 }
	 for (size_t pixel = 100; pixel < 200; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 3;
	 }
	 for (size_t pixel = 200; pixel < 300; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 0;
	 }
	 for (size_t pixel = 300; pixel < 400; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 1;
	 }
	 }

	 for (size_t line = 225; line < 300; line++) {
	 for (size_t pixel = 0; pixel < 100; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 1;
	 }
	 for (size_t pixel = 100; pixel < 200; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 2;
	 }
	 for (size_t pixel = 200; pixel < 300; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 3;
	 }
	 for (size_t pixel = 300; pixel < 400; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 0;
	 }
	 }*/

	/*for (size_t line = 100; line < 200; line++) {
	 for (size_t pixel = 150; pixel < 250; pixel++) {
	 screenBuf->BufferPtr[line * 404 + pixel] = 0x21;
	 }
	 }*/

	SET_BIT(screenBuf->screenLineDMAStream->CR, DMA_SxCR_EN);

	UInt32 thresholdSelected = READ_BIT(screenBuf->screenLineDMAStream->FCR, DMA_SxFCR_FTH);
	UInt32 fifoStatusToReach;

	switch (thresholdSelected) {
	case DMA_FIFO_THRESHOLD_1QUARTERFULL:
		fifoStatusToReach = DMA_SxFCR_FS_0; // 1/4 <= Level < 1/2
		break;
	case DMA_FIFO_THRESHOLD_HALFFULL:
		fifoStatusToReach = DMA_SxFCR_FS_1; // 1/2 <= Level < 3/4
		break;
	case DMA_FIFO_THRESHOLD_3QUARTERSFULL:
		fifoStatusToReach = DMA_SxFCR_FS_1 | DMA_SxFCR_FS_0; // 3/4 <= Level < Full
		break;
	case DMA_FIFO_THRESHOLD_FULL:
		fifoStatusToReach = DMA_SxFCR_FS_2 | DMA_SxFCR_FS_0; // Full
		break;
	default:
		Error_Handler();
		break;
	}

	while (READ_BIT(screenBuf->screenLineDMAStream->FCR, DMA_SxFCR_FS) != fifoStatusToReach) {

	}
	screenBuf->state = VGAStateActive;

	// END STEP: we start the main timer. Ther main timer direclty feeds the Hsync so we don't need any
	// output
	// We also set activate
	HAL_TIM_Base_Start(screenBuf->mainPixelClockTimer);
	return VGAErrorNone;
}

VGAError VGASuspendOutput() {

}

VGAError VGAResumeOutput() {

}

VGAError VGAStopOutput() {

}
