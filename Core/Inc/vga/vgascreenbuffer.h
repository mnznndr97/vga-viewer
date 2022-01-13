/*
 * This file contains the headers for the management of the native VGA framebuffer
 *
 * The entry point is the function VgaCreateScreenBuffer that allocates, initialize a ScreenBuffer object and
 * stores it internally later retrieve the necessary data inside the interrupt handlers.
 * The desired screen setup is specified via the VgaVisualizationInfo structure, passed as parameter to the function
 *
 * Frame timings are described by the VgaVideoFrameInfo structure
 *
 *  Created on: Nov 1, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_VGA_VGASCREENBUFFER_H_
#define INC_VGA_VGASCREENBUFFER_H_

#include <typedefs.h>
#include <screen\screen.h>
#include <stm32f4xx_hal.h>

 // ##### Public struct and enums declarations #####

typedef enum _VGAError {
    /// No error occurred
    VgaErrorNone = 0,
    /// Memory space is too low
    VGAErrorOutOfMemory,
    /// A parameter passed to a function is not valid
    VgaErrorInvalidParameter,
    VGAErrorInvalidState,
    /// provided working mode is not supported
    VgaErrorNotSupported,
} VgaError;

/// Generic timing informations of a Scanline or a Frame, expressed in items count
typedef struct _VgaTiming {
    /// Number of visible items (pixels or lines) in the current timing
    UInt16 VisibleArea;
    UInt16 FrontPorch;
    UInt16 SyncPulse;
    UInt16 BackPorch;
} VgaTiming, * PVgaTiming;

/// Complete timing informations of a VGA frame
typedef struct _VgaVideoFrameInfo {
    /// Pixel frequency in MegaHerts
    float PixelFrequencyMHz;
    /// Horizontal line timing informations
    VgaTiming ScanlineTiming;
    /// Vertical frame timing informations
    VgaTiming FrameTiming;
} VgaVideoFrameInfo, * PVgaVideoFrameInfo;

typedef struct _VgaVisualizationInfo {
    VgaVideoFrameInfo FrameSignals;
    /// Scaling to be applied to the provided resolution
    BYTE Scaling;
    /// BitsPerPixels that must be used
    Bpp BitsPerPixel;

    TIM_HandleTypeDef* mainTimer;
    TIM_HandleTypeDef* hSyncTimer;
    TIM_HandleTypeDef* vSyncTimer;
    DMA_HandleTypeDef* lineDMA;
} VgaVisualizationInfo;

// ##### Public fileds declarations #####

extern VgaVideoFrameInfo VideoFrame800x600at60Hz;
// ##### Public functions declarations #####

/// Creates a new ScreenBuffer from VGA initialization parameters and registers it as a working buffer
/// @param visualizationInfo VGA output parameters
/// @param screenBuffer Filled ScreenBuffer struct with the relative data
/// @return VGA operation status
VgaError VgaCreateScreenBuffer(const VgaVisualizationInfo* visualizationInfo, ScreenBuffer** screenBuffer);
/// Releases all the resources associated with the ScreenbBuffer instance
/// @return VGA operation status
VgaError VgaReleaseScreenBuffer(ScreenBuffer* screenBuffer);
/// Dumps the active buffer timers frequencies
VgaError VgaDumpTimersFrequencies();
/// Enable the display output of the VGA driver
/// @return Status of the operation
VgaError VgaStartOutput();
/// Suspend visible area output. Sync signals remain active
/// @return Status of the operation
VgaError VgaSuspendOutput();
/// Resume visible area output
/// @return Status of the operation
VgaError VgaResumeOutput();
/// Completly disable VGA output (sync signals are no more generated)
/// @return Status of the operation
VgaError VgaStopOutput();

#endif /* INC_VGA_VGASCREENBUFFER_H_ */
