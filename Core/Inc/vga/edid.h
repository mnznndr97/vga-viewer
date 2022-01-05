/*
 * edid.h
 * 
 * This files contains the Edid sub structures and descriptors, and the declaration
 * for the parsing and reading function
 * 
 * Bits with different meaning the same one-byte structure are modeled with C bit field
 * for simplicity
 * 
 * All the functions assume that a valid edid pointer is provided
 *
 *  Created on: 11 set 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef EDID_H_
#define EDID_H_

#include <typedefs.h>
#include <assertion.h>
#include <stddef.h>
#include <stdbool.h>

/// I2C device address for the Edid structure
/// \link https://en.wikipedia.org/wiki/Display_Data_Channel#DDC2 Source Reference \endlink
#define EDID_DDC2_I2C_DEVICE_ADDRESS 0x50

/// Video white and sync levels, relative to blank
/// \remarks The enumeration value maps directly to the value that can be found in the Edid
typedef enum _EdidAnalogVoltage {
	/// Analog voltage level = +0.7/-0.3 V 
	EdidAVoltage0p7Tom0p3 = 0,
	/// Analog voltage level = +0.714/-0.286 V 
	EdidAVoltage0p714Tom0p286 = 1,
	/// Analog voltage level = +1.0/-0.4 V 
	EdidAVoltage1p0Tom0p4 = 2,
	/// Analog voltage level = +0.7/0 V (EVC)
	EdidAVoltage0p7To0p0 = 3
} EdidAnalogVoltage;

typedef enum _EDIDTiming {
	EDIDTiming720x400At70Hz = 7, EDIDTiming720x400At88Hz = 6, EDIDTiming640x480At60Hz = 5, EDIDTiming640x480At67Hz = 4, EDIDTiming640x480At72Hz = 3, EDIDTiming640x480At75Hz = 2, EDIDTiming800x600At56Hz = 1, EDIDTiming800x600At60Hz = 0,

	EDIDTiming1024x728At60Hz = 11,
} EDIDTiming;

typedef struct _EdidDigitalInput {
	BYTE VideoInterface :4;
	BYTE BitDepth :3;
	BYTE ISDigitalInput :1;
} EdidDigitalInput;

/// Analog input definition
typedef struct _EdidAnalogInput {
	/// VSync pulse must be serrated when composite or sync-on-green is used.
	BYTE SerratedVsyncPulse :1;
	BYTE SyncOnGreenSupported :1;
	BYTE CompositeSyncSupported :1;
	BYTE SeparateSyncSupported :1;
	/// Blank-to-black setup (pedestal) expected 
	BYTE BlankToBlackExpected :1;
	/// Video white and sync levels, relative to blank. Maps to EdidAnalogVoltage
	BYTE VoltageLevel :2;
	BYTE IsDigitalInput :1;
} EdidAnalogInput;

/// Supported features bitmap
typedef struct _EdidSupportedFeatures {
	BYTE ContinuousTimings :1; // Bit 0
	BYTE PreferredTimingMode :1;
	BYTE sRGBColourSpace :1;
	BYTE DisplayType :2;
	BYTE DPMSActiveOff :1;
	BYTE DPMSSuspend :1;
	BYTE DPMSStandby :1; // BIT 7
} EdidSupportedFeatured;

typedef struct _EdidTimingInformation {
	BYTE Resolution;
	BYTE VerticalFrequency :6;
	BYTE AspectRatio :2;
} EdidTimingInformation;

/// @brief 10-bit 2� CIE 1931 xy coordinates for red, green, blue, and white point 
/// \remarks Not implemented
typedef struct _EdidChromaticityCoordinates {
	BYTE Data[10];
} EdidChromaticityCoordinates;

/// Supported bitmap for (formerly) very common timing modes. 
typedef struct _EdidEstablishedTimingBitmap {
	BYTE Data[3];
} EdidEstablishedTimingBitmap;

/// Edid table manufacturer id bit structure
typedef struct _EdidManufacturerID {
	BYTE IdData[2];
} EdidManufacturerID;

typedef struct _EDIDHeader {
	/// Fixed header pattern: 00 FF FF FF FF FF FF 00 
	BYTE HeaderPattern[8];
	/// Manufacturer ID. This is a legacy Plug and Play ID
	EdidManufacturerID Manufacturer;
	UInt16 ManufacturerProductCode;
	UInt32 Serial;
	BYTE ManufactureWeek;
	BYTE ManufactureYear;
	BYTE EdidVersion;
	BYTE EdidRevision;
} EdidHeader, *PEdidHeader;

/// Basic display parameters container
typedef struct _EDIDBasicDisplayParameters {
	/// Video input parameters bitmap 
	union {
		EdidDigitalInput DigitalInput;
		EdidAnalogInput AnalogInput;
	};
	/// Horizontal screen size, in centimeters
	BYTE HCmScreenSize;
	/// Vertical screen size, in centimeters
	BYTE VCmScreenSize;
	BYTE DisplayGamma;
	/// Supported features bitmap 
	EdidSupportedFeatured SupportedFeatures;
} EdidBasicDisplayParameters, *PEdidBasicDisplayParameters;

typedef struct _EdidDetailedTimingDescriptor {
	UInt16 PixelClock;

	BYTE HActivePixelsLSBs;
	BYTE HBlanckingPixelsLSBs;
	/* Start Byte 4 */
	BYTE HBlanckingPixelsMSBs :4; // Bit 3-0
	BYTE HActivePixelsMSBs :4;	   // Bit 7-4
	/* End */

	BYTE VActivePixelsLSBs;
	BYTE VBlanckingPixelsLSBs;
	/* Start Byte 7 */
	BYTE VBlanckingPixelsMSBs :4; // Bit 3-0
	BYTE VActivePixelsMSBs :4;	   // Bit 7-4
	/* Start Byte 4 */

	BYTE HFrontPorchLSBs;
	BYTE HSyncPulseWidthLSBs;
	BYTE VFrontPorchLSBs :4;
	BYTE VSyncPulseWidthLSBs :4;
	BYTE HFrontPorchMSBs :2;
	BYTE HSyncPulseWidthMSBs :2;
	BYTE VFrontPorchMSBs :2;
	BYTE VSyncPulseWidthMSBs :2;

	BYTE HImageSizeLSBs;
	BYTE VImageSizeLSBs;
	/* Start Byte 14 */
	BYTE VImageSizeMSBs :4; // Bit 3-0
	BYTE HImageSizeMSBs :4; // Bit 7-4
	/* End */

	BYTE HBorderPixels;
	BYTE VBorderLines;
	BYTE Raw;

} EdidDetailedTimingDescriptor;

/// 18 byte descriptor struct
typedef struct _EdidDescriptor {
	union {
		BYTE Raw[18];
		EdidDetailedTimingDescriptor DetailedTiming;
	};

} EdidDescriptor;

typedef struct _edid {
	EdidHeader Header;
	EdidBasicDisplayParameters BasicDisplayParameters;
	EdidChromaticityCoordinates ChromaticityCoordinates;
	EdidEstablishedTimingBitmap EstablishedTimingBitmap;
	EdidTimingInformation TimingInformations[8];

	EdidDescriptor Descriptor1;
	EdidDescriptor Descriptor2;
	EdidDescriptor Descriptor3;
	EdidDescriptor Descriptor4;

	BYTE Extensions;
	BYTE Checksum;
} Edid, *PEdid;

/// Returns the gamma value stored in the provided EDID
inline float EdidGetGamma(const Edid *edid) {
	return 1.0f + (edid->BasicDisplayParameters.DisplayGamma / 100.0f);
}

/// Validate the EDID with the checksum
BOOL EdidIsChecksumValid(const Edid *edid);
/// Checks if the timing is supported by the provided EDID
/// @param edid EDID struct pointer
/// @param timing VgaTiming
BOOL EdidIsTimingSupported(const Edid *edid, EDIDTiming timing);

/// Checks if the Standard timing information is filled with data
/// @param edidTimingInfo 
inline BOOL EdidIsTimingInfoFilled(const EdidTimingInformation *edidTimingInfo) {
	BYTE *bytePtr = (BYTE*) edidTimingInfo;
	return bytePtr[0] != 0x01 && bytePtr[1] != 0x01;
}

/// Merges the bytes of a Detailed VgaTiming Descriptor
/// @param lsb LSB of the value
/// @param msb LSB of the value
Int32 EdidDtdMergeBits(BYTE lsb, BYTE msb);

/// @brief Writes the 3 letters manufacturer in the provided buffer
/// @param edid EDID pointer
/// @param buffer Destination buffer
void EdidGetManufacturer(const Edid *edid, char *buffer);

Int32 EdidDtdGetHorizontalActivePixels(const EdidDetailedTimingDescriptor *detInfo);

inline Int32 EdidDtdGetHorizontalBlankingPixels(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->HBlanckingPixelsLSBs, detInfo->HBlanckingPixelsMSBs);
}

inline Int32 EdidDtdGetVerticalActivePixels(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->VActivePixelsLSBs, detInfo->VActivePixelsMSBs);
}

inline Int32 EdidDtdGetVerticalBlankingPixels(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->VBlanckingPixelsLSBs, detInfo->VBlanckingPixelsMSBs);
}

inline Int32 EdidDtdGetHorizontalFrontPorchPixels(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->HFrontPorchLSBs, detInfo->HFrontPorchMSBs);
}

inline Int32 EdidDtdGetHorizontalSyncPulseWidth(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->HSyncPulseWidthLSBs, detInfo->HSyncPulseWidthMSBs);
}

inline Int32 EdidDtdGetVerticalFrontPorchPixels(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->VFrontPorchLSBs, detInfo->VFrontPorchMSBs);
}

inline Int32 EdidDtdGetVerticalSyncPulseWidth(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->VSyncPulseWidthLSBs, detInfo->VSyncPulseWidthMSBs);
}

inline Int32 EdidDtdGetHorizontalImageSize(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->HImageSizeLSBs, detInfo->HImageSizeMSBs);
}

inline Int32 EdidDtdGetVerticalImageSize(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->VImageSizeLSBs, detInfo->VImageSizeMSBs);
}

/// Dump the EDID structure to the console
/// @param edid Structure pointer
void EdidDumpStructure(const Edid *edid);

static void __staticasserts() {
	SUPPRESS_WARNING(__staticasserts);
	// Internal static assert to check out EDID structure layout matches the specification
	static_assert(sizeof(EdidManufacturerID) == 2, "EdidManufacturerID is not of size 2 bytes");
	static_assert(sizeof(EdidDigitalInput) == 1, "EdidDigitalInput is not of size 1 bytes");
	static_assert(sizeof(EdidAnalogInput) == 1, "EdidAnalogInput is not of size 1 bytes");
	static_assert(sizeof(EdidSupportedFeatured) == 1, "EdidSupportedFeatured is not of size 1 bytes");
	static_assert(sizeof(EdidChromaticityCoordinates) == 10, "EdidChromaticityCoordinates is not of size 10 bytes");
	static_assert(sizeof(EdidTimingInformation) == 2, "EdidTimingInformation is not of size 2 bytes");
	static_assert(sizeof(EdidDetailedTimingDescriptor) == 18, "EdidDetailedTimingDescriptor is not of size 18 bytes");

	static_assert(offsetof(EdidHeader, HeaderPattern) == 0);
	static_assert(offsetof(EdidHeader, Manufacturer.IdData) == 8);
	static_assert(offsetof(EdidHeader, ManufacturerProductCode) == 10);
	static_assert(offsetof(EdidHeader, Serial) == 12);
	static_assert(offsetof(EdidHeader, ManufactureWeek) == 16);
	static_assert(offsetof(EdidHeader, ManufactureYear) == 17);
	static_assert(offsetof(EdidHeader, EdidVersion) == 18);
	static_assert(offsetof(EdidHeader, EdidRevision) == 19);

	static_assert(offsetof(EdidBasicDisplayParameters, DigitalInput) == 0);
	static_assert(offsetof(EdidBasicDisplayParameters, AnalogInput) == 0);
	static_assert(offsetof(EdidBasicDisplayParameters, HCmScreenSize) == 1);
	static_assert(offsetof(EdidBasicDisplayParameters, VCmScreenSize) == 2);
	static_assert(offsetof(EdidBasicDisplayParameters, DisplayGamma) == 3);

	static_assert(offsetof(EdidDetailedTimingDescriptor, PixelClock) == 0);
	static_assert(offsetof(EdidDetailedTimingDescriptor, HActivePixelsLSBs) == 2);
	static_assert(offsetof(EdidDetailedTimingDescriptor, HBlanckingPixelsLSBs) == 3);
	static_assert(offsetof(EdidDetailedTimingDescriptor, VActivePixelsLSBs) == 5);
	static_assert(offsetof(EdidDetailedTimingDescriptor, VBlanckingPixelsLSBs) == 6);
	static_assert(offsetof(EdidDetailedTimingDescriptor, HImageSizeLSBs) == 12);
	static_assert(offsetof(EdidDetailedTimingDescriptor, VImageSizeLSBs) == 13);
	static_assert(offsetof(EdidDetailedTimingDescriptor, HBorderPixels) == 15);
	static_assert(offsetof(EdidDetailedTimingDescriptor, VBorderLines) == 16);

	static_assert(sizeof(Edid) == 128);
	static_assert(offsetof(Edid, Header) == 0);
	static_assert(offsetof(Edid, BasicDisplayParameters) == 20);
	static_assert(offsetof(Edid, ChromaticityCoordinates) == 25);
	static_assert(offsetof(Edid, EstablishedTimingBitmap) == 35);
	static_assert(offsetof(Edid, TimingInformations) == 38);
	static_assert(offsetof(Edid, Descriptor1) == 54);
	static_assert(offsetof(Edid, Descriptor2) == 72);
	static_assert(offsetof(Edid, Descriptor3) == 90);
	static_assert(offsetof(Edid, Descriptor4) == 108);
	static_assert(offsetof(Edid, Extensions) == 126);
	static_assert(offsetof(Edid, Checksum) == 127);
}

#endif
