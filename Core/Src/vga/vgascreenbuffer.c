/*
 * vgascreenbuffer.c
 *
 *  Created on: 1 nov 2021
 *      Author: mnznn
 */

#include <vga/vgascreenbuffer.h>
#include <assertion.h>
#include <stm32f4xx_hal.h>

extern void Error_Handler();

// ##### Private forward declarations #####

static VGAError CorrectVideoFrameTimings(const VGAVisualizationInfo* info, VideoFrameInfo* finalTimes);
static VGAError SetupMainClockTree(float pixelMHzFreq);
static VGAError ValidateTiming(const Timing* timing);
static void ScaleTiming(const Timing* timing, BYTE scale, Timing* dest);

// ##### Private types declarations #####
typedef enum _VGAState {
    /**
     * @brief VGA is in the porch/sync state
     *
     * @remarks In this stage, the VGA monitor is using the RBG channels as black level calibration so our
     * signals should be blanked out
     * Reference: https://electronics.stackexchange.com/a/209494
     */
    VGAStateVSync,
    /**
     * @brief VGA is displaying some active line
     *
     * @remarks This state should be maintained during the whole vertical visible area.
     * The horizontal blanking of the single line is addressed separately
     */
     VGAStateActive

} VGAState;

typedef struct _VGAScreenBuffer {
    ScreenBuffer base;

    volatile UInt32 currentLineAddress;
    VideoFrameInfo VideoFrameTiming;
    VGAState state;
} VGAScreenBuffer;

VideoFrameInfo VideoFrame800x600at60Hz = { 40, { 800, 40, 128, 88 }, { 600, 1, 4, 23 } };

// ##### Private Function definitions #####

VGAError CorrectVideoFrameTimings(const VGAVisualizationInfo* info, VideoFrameInfo* newTimings) {
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

void ScaleTiming(const Timing* timing, BYTE scale, Timing* dest) {
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
    Int32 newWholeLineScaled = VGATimingGetSum(dest);

    Int32 scalingLoss = realWholeLineScaled - VGATimingGetSum(dest);
    // In theory, we should never have a negative loss (scaled pixels are more that the original sum scaled)
    // The code will handle the situation correctly, but it can indicate some logic/programmin error 
    DebugAssert(scalingLoss >= 0);

    // Assumption: Visible area should always scale correctly. let's just correct the back/front porch
    if (scalingLoss < 0) {
        // TODO Handle negative scaling
        Error_Handler();
    }
    else if (scalingLoss > 0) {
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

    DebugAssert(clkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    DebugAssert(clkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK);
    DebugAssert(clkInit.AHBCLKDivider = RCC_SYSCLK_DIV1);
    clkInit.APB1CLKDivider = RCC_HCLK_DIV4;
    clkInit.APB2CLKDivider = RCC_HCLK_DIV4;

    return VGAErrorNone;
}

static VGAError ValidateTiming(const Timing* timing) {
    DebugAssert(timing);

    if (timing->VisibleArea <= 0 || timing->FrontPorch <= 0 || timing->SyncPulse <= 0 || timing->BackPorch <= 0) {
        // Base VGA parameters should all be > 0
        return VGAErrorInvalidParameter;
    }

    if (timing->VisibleArea <= timing->FrontPorch
        || timing->VisibleArea <= timing->SyncPulse
        || timing->VisibleArea <= timing->BackPorch)
    {
        // VGA visible area should be the larger value
        return VGAErrorInvalidParameter;
    }

    return VGAErrorNone;
};

// ##### Public Function definitions #####

VGAError VGACreateScreenBuffer(const VGAVisualizationInfo* visualizationInfo, ScreenBuffer* screenBuffer) {
    if (!visualizationInfo || !screenBuffer) {
        return VGAErrorInvalidParameter;
    }

    if (!visualizationInfo->FrameSignals.PixelFrequencyMHz <= 0.0f) {
        return VGAErrorInvalidParameter;
    }

    // For now we support the exact 40MHz frequency
    if (!visualizationInfo->FrameSignals.PixelFrequencyMHz != 40.0f) {
        return VGAErrorNotSupported;
    }
    if (!visualizationInfo->Scaling != 2) {
        return VGAErrorNotSupported;
    }

    VGAError result;
    VideoFrameInfo scaledVideoTimings;
    if ((result = CorrectVideoFrameTimings(visualizationInfo, &scaledVideoTimings)) != VGAErrorNone) {
        return result;
    }

    SetupMainClockTree(scaledVideoTimings.PixelFrequencyMHz);


}

Int32 VGATimingGetSum(const Timing* timing) {
    if (!timing) return -1;
    return timing->VisibleArea + timing->FrontPorch + timing->SyncPulse + timing->BackPorch;
}
