/*
 * vgascreenbuffer.h
 *
 *  Created on: 1 nov 2021
 *      Author: mnznn
 */

#ifndef INC_VGA_VGASCREENBUFFER_H_
#define INC_VGA_VGASCREENBUFFER_H_

#include <screen/screenbuffer.h>

 // ##### Internal forward declarations #####

enum VGAState;

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
    VGAErrorInvalidParameter,

    VGAErrorNotSupported,
} VGAError;

/**
 * @brief Generic timing informations of a Scanline or a Frame (expressed in pixel counts)
 */
typedef struct _Timing {
    UInt16 VisibleArea;
    UInt16 FrontPorch;
    UInt16 SyncPulse;
    UInt16 BackPorch;
} Timing, * PTiming;

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
} VideoFrameInfo, * PVideoFrameInfo;

typedef struct _VGAVisualizationInfo {
    VideoFrameInfo FrameSignals;
    BYTE Scaling;
    Bpp BitsPerPixel;
    BOOL DoubleBuffered;
} VGAVisualizationInfo;

// ##### Public fileds declarations #####


extern VideoFrameInfo VideoFrame800x600at60Hz;
// ##### Public functions declarations #####

/**
 * @brief Creates a new ScreenBuffer from VGA initialization parameters
 * @param visualizationInfo VGA output parameters
 * @param screenBuffer Filled ScreenBuffer struct with the relative data
 * @return VGA status
*/
VGAError VGACreateScreenBuffer(const VGAVisualizationInfo* visualizationInfo, ScreenBuffer* screenBuffer);


/**
 * @brief Get the sum of all the pixels count in a Timing instance
 * 
 * @return -1 if pointer is invalid, pixel sum otherwhise
*/
Int32 VGATimingGetSum(const Timing* timing);


#endif /* INC_VGA_VGASCREENBUFFER_H_ */
