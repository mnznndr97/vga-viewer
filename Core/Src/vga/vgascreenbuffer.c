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
#include <binary.h>

#ifdef _DEBUG
#define DRAWPIXELASSERT

 // #define DEBUGWRITE
#endif // _DEBUG

extern void Error_Handler();

// ##### Private forward declarations #####

typedef struct _VgaScreenBuffer VgaScreenBuffer;

/// \brief  Creates a new frame buffer using the specified informations
/// \param info Information about screen resolution, timings, ecc
/// \param buffer [Out] Buffer informations
/// \return VgaErrorNone if everything is ok
static VgaError AllocateFrameBuffer(const VgaVisualizationInfo* info, VgaScreenBuffer* buffer);
static VgaError CorrectVideoFrameTimings(const VgaVisualizationInfo* info, VgaVideoFrameInfo* finalTimes);

/// \brief Draw a single pixel in the buffer using the speficied color in 3 bits per pixels mode
/// \param pixelPtr Screen buffer pointer
/// \param color Pixel color
static void Draw3bppPixelImpl(BYTE* pixelPtr, ARGB8Color color);
/// \brief Draw a single pixel in the specified point using the speficied pen
/// \param pixel Pixel location
/// \param pen Pixel pen
static void DrawPixel(PointS pixel, const Pen* pen);
/// \brief Draw a pack of pixel starting in the specified point using the speficied pen
/// \param pixel Pixel location
/// \param pen Pixel pen
/// \remarks This function allows some optimizations when drawing the same color on a large part of the screen
/// (clearing the entire screen for example)
static void DrawPixelPack(PointS pixel, const Pen* pen);

static void DisableLineDMA(DMA_Stream_TypeDef* dmaStream);
///\brief Get the sum of all the pixels count in a VgaTiming instance
///\return -1 if pointer is invalid, pixel sum otherwhise
UInt32 GetTimingSum(const VgaTiming* timing);
static void HandleHSyncInterruptFor3bpp(VgaScreenBuffer* screenBuffer, UInt32 isLineStart);
static void HandleDMALineEndFor3Bpp(VgaScreenBuffer* screenBuffer);
static VgaError ValidateTiming(const VgaTiming* timing);
static void ScaleTiming(const VgaTiming* timing, BYTE scale, VgaTiming* dest);
static VgaError SetupMainClockTree(float pixelMHzFreq);
static VgaError SetupTimers(BYTE resScaling, VgaScreenBuffer* screenBuffer);

static void ShutdownDMAFor3BppBuffer(VgaScreenBuffer* screenBuffer);

// ##### Private types declarations #####

/// Compacts the 24 color bits into a single byte
/// \remarks Red is only 2 bits (the r byte MSB), Green and Blue are 3 bits
#define RGB_TO_8BPP(r,g,b) ((((r) >> 6) | (((g) >> 5) << 2) | (((b) >> 5) << 5)) & 0xFF)
typedef enum _VGAOutputState {
    /// VGA is ready but is not outputting any signal
    VGAOutputStopped,
    /// VGA is ready and outputting timing signals but the DMA is not active and the
    /// output is forced to low for the entire frame
    VGAOutputSuspended,
    /// VGA is displaying the data in the active buffer
    ///
    /// \remarks This state should be maintained during the whole vertical visible area.
    /// The horizontal blanking of the single line is addressed separately
    VGAOutputActive
} VGAOutputState;

typedef struct _Bpp3State {
    /// \brief Word-aligned number of pixel per lines
    /// \remarks By enforcing a word-aliged line, we can use the DMA 32 bit memory access
    UInt16 linePixels;
    UInt32 currentLineOffset;

    DMA_TypeDef* screenLineDMAController;
    DMA_Stream_TypeDef* screenLineDMAStream;
} Bpp3State;

struct _VgaScreenBuffer {
    ScreenBuffer base;

    BYTE* BufferPtr;
    /// \brief Size of the buffer allocated (in bytes)
    UInt32 bufferSize;

    /// @brief State of the buffer depending on the selected color mode
    union {
        Bpp3State Bpp8;
    } bufferState;

    VgaVideoFrameInfo VideoFrameTiming;
    VGAOutputState outputState;

    /// \brief Lines prescaling constant
    /// If the resolution is "software" scaled, we may have to display the same lime for a given number of real "lines"
    BYTE linePrescaler;
    /// \brief Lines prescaler counter. When the counter reaches the prescaling constant, a new line will displayed
    SBYTE linePrescalerCnt;

    /// \brief VGA is in the vertical porch/sync state
    ///
    /// \remarks In this stage, the VGA monitor is using the RBG channels as black level calibration so our
    /// signals should be blanked out
    /// Reference: https://electronics.stackexchange.com/a/209494
    BYTE vSyncing;

    TIM_HandleTypeDef* mainPixelClockTimer;
    TIM_HandleTypeDef* hSyncClockTimer;
    TIM_HandleTypeDef* vSyncClockTimer;

    /// Cache for clear flags that need to be set to the DMA
    /// \remarks The flags are always the same depending on the stream so we can cache
    /// the value to avoid a calculation each time
    UInt32 dmaClearFlags;
};

VgaVideoFrameInfo VideoFrame800x600at60Hz = { 40, { 800, 40, 128, 88 }, { 600, 1, 4, 23 } };

static VgaScreenBuffer* volatile _activeScreenBuffer = NULL;

// ##### Private Function definitions #####

void TIM1_CC_IRQHandler(void) {
    // IRQ normal bit stuff handling
    // Not much to optimize here
    UInt32 timStatus = TIM1->SR;
    UInt32 isLineStartIRQ = READ_BIT(timStatus, TIM_FLAG_CC3);
    UInt32 isLineEndIRQ = READ_BIT(timStatus, TIM_FLAG_CC4);
    CLEAR_BIT(TIM1->SR, isLineStartIRQ | isLineEndIRQ);

    DebugAssert(isLineStartIRQ != isLineEndIRQ);

    VgaScreenBuffer* screenBuffer = _activeScreenBuffer;
    if (screenBuffer == NULL) {
        // Main timer may have been stopped just when the IRQ was pending
        // We simply ignore the IRQ
        return;
    }

    if (screenBuffer->outputState == VGAOutputStopped) {
        // Video is going to be stopped. Exiting
        return;
    }

    if (screenBuffer->base.bitsPerPixel == Bpp8) {
        HandleHSyncInterruptFor3bpp(screenBuffer, isLineStartIRQ);
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
    VgaScreenBuffer* screenBuffer = _activeScreenBuffer;
    if (screenBuffer == NULL) {
        // Main timer may have been stopped just when the IRQ was pending
        // We simply ignore the IRQ
        return;
    }

    // Independently of the state, we set the vSyncing flag
    // The vSyncing flag is indipended from the output buffer color mode
    BYTE isVSyncing = isVisibleFrameEndIRQ != 0;
    screenBuffer->vSyncing = isVSyncing;
    //DebugWriteChar('v' + isVSyncing);
}

void HandleHSyncInterruptFor3bpp(VgaScreenBuffer* screenBuffer, UInt32 isLineStart) {
    Bpp3State* bpp3State = &screenBuffer->bufferState.Bpp8;
    if (screenBuffer->vSyncing) {
        //DebugWriteChar('V');

        DMA_Stream_TypeDef* dmaStream = bpp3State->screenLineDMAStream;
        if (dmaStream->M0AR != (UInt32)screenBuffer->BufferPtr) {
            //DebugWriteChar('v');
            // DMA should not be running in our ideal world. But as we already mentioned, the BusMatrix contentions can
            // introduce some latency
            // So what to do in the vSyncing portion of the frame?

            // We have to prepare the DMA for the next line, that means enabling the DMA on the buffer start address
            // Before we ensure that the DMA is disable toghether with the timer trigger

            CLEAR_BIT(screenBuffer->hSyncClockTimer->Instance->DIER, TIM_DMA_TRIGGER);
            DisableLineDMA(dmaStream);

            // VSYNC is raised at the start event one line before the actual start of the frame so we have to make sure that the
            // end line interrupt will set a prescaler counter at zero to correctly start drawing at the next line
            screenBuffer->linePrescalerCnt = -1;
            bpp3State->currentLineOffset = 0;

            // We set back the dma to read data from the beginning of the buffer
            dmaStream->M0AR = ((UInt32)screenBuffer->BufferPtr);
            dmaStream->NDTR = (UInt32)bpp3State->linePixels;
            // We enable ONLY the DMA to start the fifo preloading of the data
            SET_BIT(dmaStream->CR, DMA_SxCR_EN);
        }

    }
    else if (isLineStart != 0) {
        // The line start IRQ HAVE TO be the fastest one -> When we are in the porch section (line or frame) we have more
        // "relaxed" time restriction
        // So we must do only one thing. Start the DMA. Everything else must be done in another time
        SET_BIT(screenBuffer->hSyncClockTimer->Instance->DIER, TIM_DMA_TRIGGER);

        // After the DMA start, we can send a little char to the ITM to let the programmer know what is happening
        //DebugWriteChar('S');
    }
    else {
        HandleDMALineEndFor3Bpp(screenBuffer);
    }
}

void HandleDMALineEndFor3Bpp(VgaScreenBuffer* screenBuffer) {
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
    Bpp3State* bpp3State = &screenBuffer->bufferState.Bpp8;
    DMA_Stream_TypeDef* dmaStream = bpp3State->screenLineDMAStream;
    DisableLineDMA(dmaStream);

    //DebugWriteChar('E');

    // We can stop the trigger DMA request of the timer. It will be resumed
    // at the start of the visible portion of the area
    __HAL_TIM_DISABLE_DMA(screenBuffer->hSyncClockTimer, TIM_DMA_TRIGGER);

    /* Check for error condition */
    UInt32 lisr = bpp3State->screenLineDMAController->LISR;

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
    SET_BIT(bpp3State->screenLineDMAController->LIFCR, screenBuffer->dmaClearFlags);

    // Line pixels freq scaling
    if ((++screenBuffer->linePrescalerCnt) == screenBuffer->linePrescaler) {
        bpp3State->currentLineOffset += bpp3State->linePixels;
        screenBuffer->linePrescalerCnt = 0;
    }

    // Data items to transfer are always the lines pixels
    // In Mem2Per mode the "items" width are relative to the width of the "peripheral" bus (RM0090 - Section 10.3.10)
    dmaStream->NDTR = bpp3State->linePixels;
    // Source memory address is simply buffer start + current line offset
    dmaStream->M0AR = ((UInt32)screenBuffer->BufferPtr) + ((UInt32)bpp3State->currentLineOffset);

    if (bpp3State->currentLineOffset < screenBuffer->bufferSize) {
        // If the buffer is within the limits, we enable the dma stream
        // This will only preload the data in the FIFO (at least in Mem2Per mode) [AN4031- Section 2.2.2]
        SET_BIT(dmaStream->CR, DMA_SxCR_EN);
    }
    else {
        // We are at the end of the buffer
        // Soon we will see the beginning of the porch area of the frame
        // We DON'T enable the DMA either so we avoid locking the BusMatrix
        dmaStream->NDTR = 0;
    }

    if (READ_BIT(TIM1->SR, TIM_FLAG_CC3) != 0) {
        Error_Handler();
    }
}

VgaError AllocateFrameBuffer(const VgaVisualizationInfo* info, VgaScreenBuffer* vgaScreenBuffer) {
    const VgaVideoFrameInfo* finalTimings = &vgaScreenBuffer->VideoFrameTiming;
    // Let's cache our info data in local variables
    Bpp localBpp = info->BitsPerPixel;

    ScreenBuffer screenBufferInfos = { 0 };
    screenBufferInfos.bitsPerPixel = localBpp;
    // We write as the screen width out visible line pixels
    screenBufferInfos.screenSize.width = (Int16)finalTimings->ScanlineTiming.VisibleArea;
    // We write as the screen height out visible frame lines
    screenBufferInfos.screenSize.height = (Int16)finalTimings->FrameTiming.VisibleArea;

    // Let' s check that here we are ok with our math
    DebugAssert(screenBufferInfos.screenSize.width > 0);
    DebugAssert(screenBufferInfos.screenSize.height > 0);

    size_t framebufferSize = 0;
    if (localBpp == Bpp8) {
        Bpp3State* bpp3State = &vgaScreenBuffer->bufferState.Bpp8;

        // We calculate the border pixels to have an word-aligned buffer width
        BYTE borderPixels = (BYTE)((4 - (screenBufferInfos.screenSize.width & 0x3)) & 0x3);

        bpp3State->linePixels = (UInt16)(screenBufferInfos.screenSize.width + borderPixels);
        framebufferSize = bpp3State->linePixels;
        DebugAssert((bpp3State->linePixels & 0x3) == 0); // make sure we have done everything right

        // 8bpp supports optimized 32bit pixel writes
        screenBufferInfos.packSizePower = 2;
    }
    else {
        // localBpp == Bpp24
        // If we have one byte per pixels, let's reflect it on the buffer
        //framebufferSize *= 3;
        screenBufferInfos.packSizePower = 0;
    }
    screenBufferInfos.DrawCallback = &DrawPixel;
    screenBufferInfos.DrawPackCallback = &DrawPixelPack;

    // framebufferSize here contains the number of bytes requires for a single line depending of the mode
    // We simply now multiply the lines and double everything if double buffered
    framebufferSize = (size_t)(framebufferSize * (size_t)screenBufferInfos.screenSize.height);

    // We store the new buffer size
    vgaScreenBuffer->bufferSize = framebufferSize;
    // We setup the lines scaling
    vgaScreenBuffer->linePrescaler = info->Scaling;
    vgaScreenBuffer->linePrescalerCnt = 0;
    // VGA will be by default in the vSyncing section
    vgaScreenBuffer->vSyncing = true;

    // We last step: we try to allocate
    BYTE* buffer = (BYTE*)ralloc(framebufferSize);
    if (buffer == NULL) {
        // We clear the out parameter to emphasize that something has gone wrong
        *vgaScreenBuffer = (const VgaScreenBuffer){ 0 };
        return VGAErrorOutOfMemory;
    }

    // Allocation is ok. Let' s write the few remaining things
    vgaScreenBuffer->BufferPtr = buffer;
    vgaScreenBuffer->base = screenBufferInfos;

    // Let's initialize the border pixels -> these will remain untouched for the rest of the application lifetime
    for (int line = 0; line < screenBufferInfos.screenSize.height; line++) {
        if (localBpp == Bpp8) {
            UInt16 totalLinePixels = vgaScreenBuffer->bufferState.Bpp8.linePixels;
            for (int pixel = screenBufferInfos.screenSize.width; pixel < totalLinePixels; pixel++) {
                // RGB write in 1 single byte
                buffer[line * totalLinePixels + pixel] = 0x00;
            }
        }
    }
    return VgaErrorNone;
}

/// Checks and calculates the new video timings using the timings provided in the initialization info 
/// @param info Initialization information
/// @param newTimings Scaled pTiming
/// @return Status of the operation
VgaError CorrectVideoFrameTimings(const VgaVisualizationInfo* info, VgaVideoFrameInfo* newTimings) {
    // All basic parameters (except for timings) should have been checked here
    DebugAssert(info && newTimings);
    DebugAssert(info->Scaling >= 0);

    VgaError result;
    if ((result = ValidateTiming(&info->FrameSignals.FrameTiming)) != VgaErrorNone) {
        return result;
    }
    if ((result = ValidateTiming(&info->FrameSignals.ScanlineTiming)) != VgaErrorNone) {
        return result;
    }

    // Here basic timings are all ok
    // If no scaling is necessary, we simply copy the timings
    // and return
    if (info->Scaling == 1) {
        *newTimings = info->FrameSignals;
        return VgaErrorNone;
    }

    // The PixelFrequencyMHz is simply scaled. Nothing fancy here, it will be up to the clock setup routine to settle any borderline cases
    newTimings->PixelFrequencyMHz = info->FrameSignals.PixelFrequencyMHz / info->Scaling;

    // Same for the pixel values (which have already been checked, so everything should be ok even after the scaling)
    ScaleTiming(&info->FrameSignals.FrameTiming, info->Scaling, &newTimings->FrameTiming);
    ScaleTiming(&info->FrameSignals.ScanlineTiming, info->Scaling, &newTimings->ScanlineTiming);

    return VgaErrorNone;
}

void Draw3bppPixelImpl(BYTE* pixelPtr, ARGB8Color color) {
    // Super simple implementations: we read our color components, correct them
    // with the alpha if necessary and then we map to the 8bit pointer

    BYTE r = color.components.R;
    BYTE g = color.components.G;
    BYTE b = color.components.B;

    // We apply alpha correction only if necessary
    // In this way we can avoid floating point operations when
    // possible
    if (color.components.A != 0xFF) {
        int currentPixelColor = ((int)(*pixelPtr)) & 0xFF;

        float normalizedAlpha = color.components.A / 255.0f;
        float normalizedBgAlpha = 1.0f - normalizedAlpha;

        float newRed = ((float)r) * normalizedAlpha;
        float oldRed = ((float)MASKI2BYTE(currentPixelColor << 6)) * normalizedBgAlpha;

        float newGreen = ((float)g) * normalizedAlpha;
        float oldGreen = ((float)MASKI2BYTE((currentPixelColor & 0x1c) << 3)) * normalizedBgAlpha;

        float newBlue = ((float)b) * normalizedAlpha;
        float oldBlue = ((float)MASKI2BYTE(currentPixelColor & 0xE0)) * normalizedBgAlpha;

        DebugAssert(newRed + oldRed <= 255.0f);
        DebugAssert(newGreen + oldGreen <= 255.0f);
        DebugAssert(newBlue + oldBlue <= 255.0f);

        r = (BYTE)(newRed + oldRed);
        g = (BYTE)(newGreen + oldGreen);
        b = (BYTE)(newBlue + oldBlue);
    }

    *pixelPtr = (BYTE)RGB_TO_8BPP(r, g, b);
}

void DrawPixel(PointS pixel, const Pen* pen) {
    VgaScreenBuffer* buffer = _activeScreenBuffer;

#ifdef DRAWPIXELASSERT
    DebugAssert(buffer != NULL);
    DebugAssert(pixel.x >= 0 && pixel.x < buffer->base.screenSize.width);
    DebugAssert(pixel.y >= 0 && pixel.y < buffer->base.screenSize.height);
    DebugAssert(pen != NULL);
#endif // DRAWPIXELASSERT

    if (buffer->base.bitsPerPixel == Bpp8) {
        BYTE* vgaBufferPtr = &buffer->BufferPtr[pixel.y * buffer->bufferState.Bpp8.linePixels + pixel.x];
        Draw3bppPixelImpl(vgaBufferPtr, pen->color);
    }
}

void DrawPixelPack(PointS pixel, const Pen* pen) {
    VgaScreenBuffer* buffer = _activeScreenBuffer;

#ifdef DRAWPIXELASSERT
    DebugAssert(buffer != NULL);
    DebugAssert(pixel.x >= 0 && pixel.x < buffer->base.screenSize.width);
    DebugAssert(pixel.y >= 0 && pixel.y < buffer->base.screenSize.height);
#endif // DRAWPIXELASSERT

    // We need to calculate the pack address. In our case, the pack address must be 32 bit aligned since we are using a 32bit
    // memory access. The processor will throw an exception if the access is not aligned.
    BYTE* pixelPtr = &buffer->BufferPtr[pixel.y * buffer->bufferState.Bpp8.linePixels + pixel.x];
    DebugAssert(((UInt32)pixelPtr & 0x03) == 0x0);

    ARGB8Color color = pen->color;
    if (color.components.A != 0xFF) {
        // If we are using alpha, the underling 4 background pixels can be different so we must fall back to a single
        // draw call per pixel

        Draw3bppPixelImpl(pixelPtr, color);
        Draw3bppPixelImpl(pixelPtr + 1, color);
        Draw3bppPixelImpl(pixelPtr + 2, color);
        Draw3bppPixelImpl(pixelPtr + 3, color);
    }
    else {
        UInt32 pixelColor = (UInt32)RGB_TO_8BPP(color.components.R, color.components.G, color.components.B);

        // Can this be optimized with some strange arm instruction? I din't find anything in the MCU Programming manual [PM0214]
        // but from the tests we still have some good performance boost
        UInt32 wordPixelColor = pixelColor | pixelColor << 8 | pixelColor << 16 | pixelColor << 24;

        // Super simple word access and pixel pack store
        UInt32* wordPtr = ((UInt32*)pixelPtr);
        *wordPtr = wordPixelColor;
    }
}

void DisableLineDMA(DMA_Stream_TypeDef* dmaStream) {
    // If DMA is still enabled, we disable it to interrupt the transfer
    // This is the only thing that has to be done [RM0090 - Section 10.3.14]
    CLEAR_BIT(dmaStream->CR, DMA_SxCR_EN);

    // We have to wait that DMA_SxCR_EN is effectively set to 0
    // so we can setup our next transfer
    while (READ_BIT(dmaStream->CR, DMA_SxCR_EN) != 0)
        ;

    // We clear the output on the DMA peripheral since we are in the blanking portion and we don't know
    // the exact output value where the transfer was interrupted
    // TODO Assuming 8 bit peripheral for the moment
    *((BYTE*)dmaStream->PAR) = 0x0;
}

UInt32 GetTimingSum(const VgaTiming* timing) {
    UInt32 result = 0;
    result += (UInt32)timing->VisibleArea;
    result += (UInt32)timing->FrontPorch;
    result += (UInt32)timing->SyncPulse;
    result += (UInt32)timing->BackPorch;
    return result;
}

void ScaleTiming(const VgaTiming* timing, BYTE scale, VgaTiming* dest) {
    UInt32 wholeLine = GetTimingSum(timing);

    // NB: We are scaling pixel count values, so an integer division should do
    dest->VisibleArea = (UInt16)(timing->VisibleArea / scale);
    dest->FrontPorch = (UInt16)(timing->FrontPorch / scale);
    dest->SyncPulse = (UInt16)(timing->SyncPulse / scale);
    dest->BackPorch = (UInt16)(timing->BackPorch / scale);

    if (wholeLine % scale != 0) {
        // Edge case: VGA timing is not a multiple of the scaler, not much else to do
        return;
    }

    // Common case: VGA timing is a multiple of the scaler, so we can check that all the original pixels have not been lost
    // in the int division
    UInt32 realWholeLineScaled = wholeLine / scale;

    Int32 scalingLoss = (Int32)realWholeLineScaled - (Int32)GetTimingSum(dest);
    // In theory, we should never have a negative loss (scaled pixels are less that the original sum scaled)
    // The code will handle the situation correctly, but it can indicate some logic/programming error
    // Loss should also have an upper bound (max rest of division by the scale mult the number of time this is used)
    DebugAssert(scalingLoss >= 0 && scalingLoss < ((scale - 1) * 4));

    // Assumption: Visible area should always scale correctly. let's just correct the back/front porch
    if (scalingLoss > 0) {
        // Front porch is the smaller in our 800x600 frame. 
        // This is a workaround since we must scale our buffer, so let's not
        // do any fancy algorithm here
        dest->FrontPorch = (UInt16)(dest->FrontPorch + scalingLoss);
    }
}

static VgaError SetupMainClockTree(float pixelMHzFreq) {
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

    return VgaErrorNone;
}

/// @brief Validate a VgaTiming structure
/// @param timing Valid pointer to a VgaTiming structure
/// @return Status of the validation
VgaError ValidateTiming(const VgaTiming* pTiming) {
    DebugAssert(pTiming);

    if (pTiming->VisibleArea <= 0 || pTiming->FrontPorch <= 0 || pTiming->SyncPulse <= 0 || pTiming->BackPorch <= 0) {
        // Base VGA parameters should all be > 0
        return VgaErrorInvalidParameter;
    }

    if (pTiming->VisibleArea <= pTiming->FrontPorch || pTiming->VisibleArea <= pTiming->SyncPulse || pTiming->VisibleArea <= pTiming->BackPorch) {
        // VGA visible area should be the larger value
        return VgaErrorInvalidParameter;
    }

    return VgaErrorNone;
}

VgaError SetupTimers(BYTE resScaling, VgaScreenBuffer* screenBuffer) {
    UInt32 timersOnAPB1Freq = HAL_RCC_GetPCLK1Freq() * 2;
    UInt32 pixelFreq = (UInt32)(screenBuffer->VideoFrameTiming.PixelFrequencyMHz * 1000000.0f);
    UInt32 prescaler = timersOnAPB1Freq / pixelFreq;

    if (prescaler == 0 || prescaler > UINT8_MAX) {
        return VgaErrorInvalidParameter;
    }

    // We prepare the main timer with the prescaling. The main timer will clock the HSync
    // that will clock the DMA
    TIM_TypeDef* mainTimHandle = screenBuffer->mainPixelClockTimer->Instance;
    mainTimHandle->ARR = prescaler - 1;
    mainTimHandle->CNT = 0;

    const VgaTiming* hTiming = &screenBuffer->VideoFrameTiming.ScanlineTiming;
    const VgaTiming* vTiming = &screenBuffer->VideoFrameTiming.FrameTiming;

    // Due to the PWM mode of our timers, the line/frame starts with a back porch
    // The timers will be high for BPorch + Visible + FPorch time and go low for the Sync time
    UInt32 wholeLine = GetTimingSum(hTiming);
    UInt32 wholeFrame = GetTimingSum(vTiming);

    /* *** Horizontal sync setup *** */

    // We set the clock division by clearing the bits and setting the new ones
    TIM_TypeDef* hSyncTimer = screenBuffer->hSyncClockTimer->Instance;
    hSyncTimer->ARR = wholeLine - 1;
    hSyncTimer->CNT = 0;

    hSyncTimer->CCR1 = wholeLine - hTiming->SyncPulse; // Main HSYNC signal
    hSyncTimer->CCR2 = hTiming->BackPorch; // Black porch VSYNC trigger
    hSyncTimer->CCR3 = hTiming->BackPorch - 24U; // DMA start (video line render start)
    hSyncTimer->CCR4 = (UInt32)(hTiming->BackPorch + hTiming->VisibleArea + 8); // DMA end (video line render end)

    DebugAssert(hSyncTimer->CCR1 >= 0);
    DebugAssert(hSyncTimer->CCR2 >= 0 && hSyncTimer->CCR2 < hSyncTimer->CCR1);
    DebugAssert(hSyncTimer->CCR3 >= 0 && hSyncTimer->CCR3 < hSyncTimer->CCR1);
    DebugAssert(hSyncTimer->CCR4 >= 0 && hSyncTimer->CCR4 >= hSyncTimer->CCR3 && hSyncTimer->CCR4 < hSyncTimer->CCR1);

    /* *** Vertical sync setup *** */
    // HSync will trigger the VSync at each line using the correct VGA timing, so we have to prescale the
    // timer at the input
    TIM_TypeDef* vSyncTimer = screenBuffer->vSyncClockTimer->Instance;
    vSyncTimer->ARR = wholeFrame - 1;
    vSyncTimer->CNT = 0;
    vSyncTimer->PSC = (UInt32)resScaling - 1U;

    vSyncTimer->CCR1 = wholeFrame - vTiming->SyncPulse;
    vSyncTimer->CCR2 = vTiming->BackPorch;				// VideoStart signal
    vSyncTimer->CCR3 = (UInt32)(vTiming->BackPorch + vTiming->VisibleArea); // VideoEnd signal

    DebugAssert(vSyncTimer->CCR1 >= 0);
    DebugAssert(vSyncTimer->CCR2 >= 0 && vSyncTimer->CCR2 < vSyncTimer->CCR1);
    DebugAssert(vSyncTimer->CCR3 >= 0 && vSyncTimer->CCR3 < vSyncTimer->CCR1&& vSyncTimer->CCR3 > vSyncTimer->CCR2);

    vSyncTimer->EGR = TIM_EGR_UG;

    return VgaErrorNone;
}

void ShutdownDMAFor3BppBuffer(VgaScreenBuffer* screenBuffer) {
    Bpp3State* bpp3BufState = &screenBuffer->bufferState.Bpp8;
    // 1) We disable the line DMA
    DisableLineDMA(bpp3BufState->screenLineDMAStream);

    // 2) We can disable the timer DMA trigger
    __HAL_TIM_DISABLE_DMA(screenBuffer->hSyncClockTimer, TIM_DMA_TRIGGER);
}

// ##### Public Function definitions #####

VgaError VgaCreateScreenBuffer(const VgaVisualizationInfo* visualizationInfo, ScreenBuffer** screenBuffer) {
    // In and out pointers must me valid
    if (!visualizationInfo || !screenBuffer) {
        return VgaErrorInvalidParameter;
    }
    if (!visualizationInfo->mainTimer || !visualizationInfo->hSyncTimer || !visualizationInfo->vSyncTimer) {
        return VgaErrorInvalidParameter;
    }

    // Let's allocate in our generic memory the VgaScreenBuffer struct
    VgaScreenBuffer* vgaScreenBuffer = (VgaScreenBuffer*)malloc(sizeof(VgaScreenBuffer));
    *screenBuffer = (ScreenBuffer*)vgaScreenBuffer;
    if (*screenBuffer == NULL) {
        // Cannot allocate memory for ScreenBuffer
        return VGAErrorOutOfMemory;
    }

    if (visualizationInfo->FrameSignals.PixelFrequencyMHz <= 0.0f
        || visualizationInfo->Scaling <= 0) {
        return VgaErrorInvalidParameter;
    }
    // For now we support the exact 40MHz frequency
    if (visualizationInfo->FrameSignals.PixelFrequencyMHz != 40.0f) {
        return VgaErrorNotSupported;
    }
    if (visualizationInfo->Scaling != 2) {
        return VgaErrorNotSupported;
    }

    VgaError result;
    VgaVideoFrameInfo scaledVideoTimings;
    if ((result = CorrectVideoFrameTimings(visualizationInfo, &scaledVideoTimings)) != VgaErrorNone) {
        return result;
    }

    vgaScreenBuffer->outputState = VGAOutputStopped;
    vgaScreenBuffer->vSyncing = 0;
    vgaScreenBuffer->base.bitsPerPixel = visualizationInfo->BitsPerPixel;
    vgaScreenBuffer->VideoFrameTiming = scaledVideoTimings;

    // Before configuring clock, we try to allocate our buffer to see if there is enough memory
    if ((result = AllocateFrameBuffer(visualizationInfo, vgaScreenBuffer)) != VgaErrorNone) {
        return result;
    }

    vgaScreenBuffer->mainPixelClockTimer = visualizationInfo->mainTimer;
    vgaScreenBuffer->hSyncClockTimer = visualizationInfo->hSyncTimer;
    vgaScreenBuffer->vSyncClockTimer = visualizationInfo->vSyncTimer;

    vgaScreenBuffer->bufferState.Bpp8.screenLineDMAController = DMA2;
    vgaScreenBuffer->bufferState.Bpp8.screenLineDMAStream = visualizationInfo->lineDMA->Instance;
    // TODO Calculate this based on stream number
    vgaScreenBuffer->dmaClearFlags = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CFEIF0;

    /*if ((result = SetupMainClockTree(scaledVideoTimings.PixelFrequencyMHz)) != VgaErrorNone) {
     return result;
     }*/

    if ((result = SetupTimers(visualizationInfo->Scaling, vgaScreenBuffer)) != VgaErrorNone) {
        return result;
    }

    if (visualizationInfo->BitsPerPixel == Bpp8) {
        // When using the 8bpp visualization, we use the low 8 GPIOE pins to output our colors:
        // 3 bits for blue, 3 bits for green, 2 bits for red, in "little-endian" order (Red -> [0, 1], Green -> [2, 4], Blu -> [5-7])

        DMA_Stream_TypeDef* dmaStream = vgaScreenBuffer->bufferState.Bpp8.screenLineDMAStream;
        dmaStream->PAR = (uint32_t)&GPIOE->ODR;
        dmaStream->M0AR = (uint32_t)vgaScreenBuffer->BufferPtr;
        dmaStream->NDTR = (uint32_t)vgaScreenBuffer->bufferState.Bpp8.linePixels;

    }

    // We eventually register the screen buffer instance
    _activeScreenBuffer = vgaScreenBuffer;
    return VgaErrorNone;
}

VgaError VGAReleaseScreenBuffer(ScreenBuffer* screenBuffer) {
    if (!screenBuffer) {
        return VgaErrorInvalidParameter;
    }
    VgaScreenBuffer* vgaBuffer = (VgaScreenBuffer*)screenBuffer;

    rfree(vgaBuffer->BufferPtr, vgaBuffer->bufferSize);
    // Zeroing everything to make sure the buffer will be not reused
    *vgaBuffer = (VgaScreenBuffer){ 0 };
    // TODO: check consistency of this with parameter?
    _activeScreenBuffer = NULL;

    free(vgaBuffer);
    return VgaErrorNone;
}

VgaError VGADumpTimersFrequencies() {
    VgaScreenBuffer* screenBuf = _activeScreenBuffer;
    if (screenBuf == NULL) {
        // VGA screen buffer not allocated and registered
        return VGAErrorInvalidState;
    }

    float timersOnAPB1Freq = (float)HAL_RCC_GetPCLK1Freq() * 2.0f;
    float timersOnAPB2Freq = (float)HAL_RCC_GetPCLK2Freq() * 2.0f;

    // Let's print first the main timer frequency

    printf("Main timer:\r\n");
    printf("\tInput frequency (from APB1): ");
    FormatFrequency(timersOnAPB1Freq);
    printf("\r\n");

    TIM_TypeDef* mainTim = screenBuf->mainPixelClockTimer->Instance;
    float mainTimerFreq = timersOnAPB2Freq / ((float)(mainTim->PSC) + 1.0f);

    printf("\tPrescaled frequency: ");
    FormatFrequency(mainTimerFreq);
    printf("\r\n");

    mainTimerFreq = timersOnAPB2Freq / (((float)mainTim->ARR) + 1.0f);
    printf("\tEffetive frequency: ");
    FormatFrequency(mainTimerFreq);
    printf("\r\n");

    // HSync timer info print
    TIM_TypeDef* hSyncTim = screenBuf->hSyncClockTimer->Instance;
    printf("HSync timer:\r\n");
    printf("\tInput frequency (from trigger timer): ");
    FormatFrequency(mainTimerFreq);
    printf("\r\n");

    float hSyncFreq = mainTimerFreq / (((float)hSyncTim->ARR) + 1.0f);
    printf("\tSignal frequency: ");
    FormatFrequency(hSyncFreq);
    printf("\r\n");

    // VSync timer info print
    TIM_TypeDef* vSyncTim = screenBuf->vSyncClockTimer->Instance;
    printf("VSync timer:\r\n");
    printf("\tInput frequency (from trigger hsync): ");
    FormatFrequency(hSyncFreq);
    printf("\r\n");

    float vSyncFreq = hSyncFreq / (((float)vSyncTim->PSC) + 1.0f);
    printf("\tPrescaled frequency: ");
    FormatFrequency(vSyncFreq);
    printf("\r\n");

    vSyncFreq = vSyncFreq / (((float)vSyncTim->ARR) + 1.0f);
    printf("\tSignal frequency: ");
    FormatFrequency(vSyncFreq);
    printf("\r\n");

    return VgaErrorNone;
}

VgaError VGAStartOutput() {
    VgaScreenBuffer* screenBuf = _activeScreenBuffer;
    if (screenBuf == NULL) {
        // VGA screen buffer not allocated and registered
        return VGAErrorInvalidState;
    }

    if (screenBuf->outputState != VGAOutputStopped) {
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

    Bpp3State* bpp3State = &screenBuf->bufferState.Bpp8;
    // Before starting we clear all the flags in case a previous transfer was completed/cancelled
    SET_BIT(bpp3State->screenLineDMAController->LIFCR, screenBuf->dmaClearFlags);
    SET_BIT(bpp3State->screenLineDMAStream->CR, DMA_SxCR_EN);

    //DebugAssert((READ_BIT(bpp3State->screenLineDMAStream->CR, DMA_SxCR_EN)) != 0);

    UInt32 thresholdSelected = READ_BIT(bpp3State->screenLineDMAStream->FCR, DMA_SxFCR_FTH);
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

    // Let's just wait the FIFO is effectively filled
    // NB: Didn't find anything in the documentation that states that this operation is necessary  but let's keep it anyway
    // If there is some problem with the FIFO we may end up being blocked in this infinite loop and track down the problem
    while (READ_BIT(bpp3State->screenLineDMAStream->FCR, DMA_SxFCR_FS) != fifoStatusToReach) {

    }
    screenBuf->outputState = VGAOutputActive;

    // END STEP: we start the main timer. The main timer directly feeds the Hsync so we don't need any
    // output
    // We also set activate
    HAL_TIM_Base_Start(screenBuf->mainPixelClockTimer);
    return VgaErrorNone;
}

VgaError VGASuspendOutput() {

}

VgaError VGAResumeOutput() {

}

VgaError VGAStopOutput() {
    VgaScreenBuffer* screenBuf = _activeScreenBuffer;
    if (screenBuf == NULL) {
        // VGA screen buffer not allocated and registered
        return VGAErrorInvalidState;
    }

    // Before doing anything else, we set the output state as stopped
    // In this way our timers IRQ_Handlers will do nothing
    // This is necessary since, as stated in the "Tips and Warning" section of DMA controller [Section 4.1; AN4031]
    // we first have to disable the DMA, wait for its EN bit to become 0 and then disable the peripheral
    screenBuf->outputState = VGAOutputStopped;

    if (screenBuf->base.bitsPerPixel == Bpp8) {
        ShutdownDMAFor3BppBuffer(screenBuf);
    }

    // At the end we stop our timers. First we stop our main timer to avoid any other interrupt to be issued
    HAL_TIM_Base_Stop(screenBuf->mainPixelClockTimer);
    HAL_TIM_PWM_Stop_IT(screenBuf->vSyncClockTimer, TIM_CHANNEL_3);
    HAL_TIM_PWM_Stop_IT(screenBuf->vSyncClockTimer, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(screenBuf->vSyncClockTimer, TIM_CHANNEL_1);

    HAL_TIM_PWM_Stop_IT(screenBuf->hSyncClockTimer, TIM_CHANNEL_4);
    HAL_TIM_PWM_Stop_IT(screenBuf->hSyncClockTimer, TIM_CHANNEL_3);
    HAL_TIM_PWM_Stop(screenBuf->hSyncClockTimer, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(screenBuf->hSyncClockTimer, TIM_CHANNEL_1);

    // let's reset the counters just in case
    screenBuf->mainPixelClockTimer->Instance->CNT = 0;
    screenBuf->vSyncClockTimer->Instance->CNT = 0;
    screenBuf->hSyncClockTimer->Instance->CNT = 0;
    return VgaErrorNone;
}
