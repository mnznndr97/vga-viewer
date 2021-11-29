/*
 * vgascreenbuffer.h
 *
 *  Created on: 1 nov 2021
 *      Author: mnznn
 */

#ifndef INC_VGA_VGASCREENBUFFER_H_
#define INC_VGA_VGASCREENBUFFER_H_

#include <typedefs.h>
#include <screen\screen.h>
#include <stm32f4xx_hal.h>

// ##### Internal forward declarations #####

enum VGAOutputState;
typedef struct _VGAScreenBuffer VGAScreenBuffer;

// ##### Public struct and enums declarations #####

typedef enum _VGAError {
	/**
	 * @brief No error occurred
	 */
	VGAErrorNone = 0,
	/**
	 * @brief Memory space is too low
	 */
	VGAErrorOutOfMemory,
	/**
	 * @brief A parameter passed to a function is not valid
	 */
	VGAErrorInvalidParameter, VGAErrorInvalidState,

	VGAErrorNotSupported,
} VGAError;

/**
 * @brief Generic timing informations of a Scanline or a Frame, expressed in items count
 */
typedef struct _Timing {
	/// Number of visible items (pixels or lines) in the current timing
	UInt16 VisibleArea;
	UInt16 FrontPorch;
	UInt16 SyncPulse;
	UInt16 BackPorch;
} Timing, *PTiming;

/**
 * @brief Complete timing informations of a VGA frame
 */
typedef struct VideoFrameInfo {
	/**
	 * @brief Pixel frequency in MegaHerts
	 */
	float PixelFrequencyMHz;
	/**
	 * @brief Horizontal line timing informations
	 */
	Timing ScanlineTiming;
	/**
	 * @brief Vertical frame timing informations
	 */
	Timing FrameTiming;
} VideoFrameInfo, *PVideoFrameInfo;

typedef struct _VGAVisualizationInfo {
	VideoFrameInfo FrameSignals;
	BYTE Scaling;
	Bpp BitsPerPixel;
	BOOL DoubleBuffered;

	TIM_HandleTypeDef *mainTimer;
	TIM_HandleTypeDef *hSyncTimer;
	TIM_HandleTypeDef *vSyncTimer;
	DMA_HandleTypeDef *lineDMA;
} VGAVisualizationInfo;

// ##### Public fileds declarations #####

extern VideoFrameInfo VideoFrame800x600at60Hz;
// ##### Public functions declarations #####

/// Creates a new ScreenBuffer from VGA initialization parameters and registers it as a working buffer
/// @param visualizationInfo VGA output parameters
/// @param screenBuffer Filled ScreenBuffer struct with the relative data
/// @return VGA operation status
VGAError VGACreateScreenBuffer(const VGAVisualizationInfo *visualizationInfo, ScreenBuffer **screenBuffer);
/// Releases all the resources associated with the ScreenbBuffer instance
/// @return VGA operation status
VGAError VGAReleaseScreenBuffer(ScreenBuffer* screenBuffer);

/// Dumps the active buffer timers frequencies
VGAError VGADumpTimersFrequencies();

VGAError VGAStartOutput();
VGAError VGASuspendOutput();
VGAError VGAResumeOutput();
VGAError VGAStopOutput();

#endif /* INC_VGA_VGASCREENBUFFER_H_ */
