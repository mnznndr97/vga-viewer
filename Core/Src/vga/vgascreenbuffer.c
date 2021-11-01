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

static void SetupMainClockTree();


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
    BYTE linePrescaler;
    VGAState state;
} VGAScreenBuffer;

VideoFrameInfo VideoFrame800x600at60Hz = { 40, { 800, 40, 128, 88 }, { 600, 1, 4, 23 } };

// ##### Function definitions #####

void SetupMainClockTree() {
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

    if (HAL_RCC_OscConfig(&oscInit) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitTypeDef clkInit = { 0 };
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clkInit, &latency);

    DebugAssert(clkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    DebugAssert(clkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK);
    DebugAssert(clkInit.AHBCLKDivider = RCC_SYSCLK_DIV1);
    clkInit.APB1CLKDivider = RCC_HCLK_DIV4;
    clkInit.APB2CLKDivider = RCC_HCLK_DIV4;
}

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

    SetupMainClockTree();

}
