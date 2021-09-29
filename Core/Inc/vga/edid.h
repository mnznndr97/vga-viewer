#ifndef EDID_H_
#define EDID_H_

#include <typedefs.h>
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>

//typedef enum  alignas(1) _EDIDBitDepth {
//	Undefined = (BYTE)0, Bpp6 = (BYTE)1, Bpp8 = (BYTE)2, Bpp10 = (BYTE)3, Bpp12 = (BYTE)4, Bpp14 = (BYTE)5, Bpp16 = (BYTE)6, Reserved = (BYTE)7,
//} EDIDBitDepth;
//
//typedef enum  alignas(1) _EDIDVoltageLevel {
//	Level0, Level1, Level2, Level3
//} EDIDVoltageLevel;
//
//typedef enum  alignas(1)_EDIDVideoInterface {
//	Undefined = 0, HDMIa = 2, HDMIb = 3, MDDI = 4, DisplayPort = 5
//} EDIDVideoInterface;

typedef enum _EDIDAnalogVoltage {
	EDIDAVoltage0p7Tom0p3 = 0, EDIDAVoltage0p714Tom0p286 = 1, EDIDAVoltage1p0Tom0p4 = 2, EDIDAVoltage0p7To0p0 = 3
} EDIDAnalogVoltage;

typedef enum _EDIDTiming {
	EDIDTiming720x400At70Hz = 7,
	EDIDTiming720x400At88Hz = 6,
	EDIDTiming640x480At60Hz = 5,
	EDIDTiming640x480At67Hz = 4,
	EDIDTiming640x480At72Hz = 3,
	EDIDTiming640x480At75Hz = 2,
	EDIDTiming800x600At56Hz = 1,
	EDIDTiming800x600At60Hz = 0,

	EDIDTiming1024x728At60Hz = 11,
} EDIDTiming;

typedef struct _EDIDDigitalInput {
	BYTE VideoInterface :4;
	BYTE BitDepth :3;
	BYTE ISDigitalInput :1;
} EDIDDigitalInput;

typedef struct _EDIDAnalogInput {
	BYTE SerratedVsyncPulse :1;
	BYTE SyncOnGreenSupported :1;
	BYTE CompositeSyncSupported :1;
	BYTE SeparateSyncSupported :1;
	BYTE BlankToBlackExpected :1;
	BYTE VoltageLevel :2;
	BYTE IsDigitalInput :1;
} EDIDAnalogInput;

typedef struct _EDIDSupportedFeatures {
	BYTE ContinuousTimings :1; // Bit 0
	BYTE PreferredTimingMode :1;
	BYTE sRGBColourSpace :1;
	BYTE DisplayType :2;
	BYTE DPMSActiveOff :1;
	BYTE DPMSSuspend :1;
	BYTE DPMSStandby :1; // BIT 7
} EDIDSupportedFeatured;

typedef struct _EDIDTimingInformation {
	BYTE Resolution;
	BYTE VerticalFrequency :6;
	BYTE AspectRatio :2;
} EDIDTimingInformation;

typedef struct _EDIDChromaticityCoordinates {
	BYTE Data[10];
} EDIDChromaticityCoordinates;

typedef struct _EDIDEstablishedTimingBitmap {
	BYTE Data[3];
} EDIDEstablishedTimingBitmap;

/**
 * @summary EDID table manufacturer id bit structure
 */
typedef struct _EDIDManufacturerID {
	BYTE IdData[2];

/*Int16 Reserved : 1;
 Int16 FirstLetter : 5;
 Int16 SecondLetter : 5;
 Int16 ThirdLetter : 5;*/
} EDIDManufacturerID;

typedef struct _EDIDHeader {
	BYTE HeaderPattern[8];
	EDIDManufacturerID Manufacturer;
	UInt16 ManufacturerProductCode;
	UInt32 Serial;
	BYTE ManufactureWeek;
	BYTE ManufactureYear;
	BYTE EDIDVersion;
	BYTE EDIDRevision;
} EDIDHeader, *PEDIDHeader;

typedef struct _EDIDBasicDisplayParameters {
	union {
		EDIDDigitalInput DigitalInput;
		EDIDAnalogInput AnalogInput;
	};
	BYTE HCmScreenSize;
	BYTE VCmScreenSize;
	BYTE DisplayGamma;
	EDIDSupportedFeatured SupportedFeatures;
} EDIDBasicDisplayParameters, *PEDIDBasicDisplayParameters;

typedef struct _EDIDDetailedTimingDescriptor {
	UInt16 PixelClock;

	BYTE HActivePixelsLSBs;
	BYTE HBlanckingPixelsLSBs;
	/* Start Byte 4 */
	BYTE HBlanckingPixelsMSBs :4; // Bit 3-0
	BYTE HActivePixelsMSBs :4; // Bit 7-4
	/* End */

	BYTE VActivePixelsLSBs;
	BYTE VBlanckingPixelsLSBs;
	/* Start Byte 7 */
	BYTE VBlanckingPixelsMSBs :4; // Bit 3-0
	BYTE VActivePixelsMSBs :4; // Bit 7-4
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

} EDIDDetailedTimingDescriptor;

typedef struct _EDIDDescriptor {
	union {
		BYTE Raw[18];
		EDIDDetailedTimingDescriptor DetailedTiming;
	};

} EDIDDescriptor;

typedef struct _EDID {
	EDIDHeader Header;
	EDIDBasicDisplayParameters BasicDisplayParameters;
	EDIDChromaticityCoordinates ChromaticityCoordinates;
	EDIDEstablishedTimingBitmap EstablishedTimingBitmap;
	EDIDTimingInformation TimingInformations[8];

	EDIDDescriptor Descriptor1;
	EDIDDescriptor Descriptor2;
	EDIDDescriptor Descriptor3;
	EDIDDescriptor Descriptor4;

	BYTE Extensions;
	BYTE Checksum;
} EDID, *PEDID;

inline float EDIDGetGamma(const EDID *edid) {
	return 1.0f + (edid->BasicDisplayParameters.DisplayGamma / 100.0f);
}

inline BOOL EDIDIsChecksumValid(const EDID *edid);
inline BOOL EDIDIsTimingSupported(const EDID *edid, EDIDTiming timing);

inline BOOL EDIDIsTimingInfoFilled(const EDIDTimingInformation *edidTimingInfo) {
	BYTE *bytePtr = (BYTE*) edidTimingInfo;
	return bytePtr[0] != 0x01 && bytePtr[1] != 0x01;
}

inline Int32 EDIDDTDMergeBits(BYTE lsb, BYTE msb);

void EDIDGetManufacturer(const EDID *edid, char *buffer);

inline Int32 EDIDDTDGetHorizontalActivePixels(const EDIDDetailedTimingDescriptor *detInfo);

inline Int32 EDIDDTDGetHorizontalBlankingPixels(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->HBlanckingPixelsLSBs, detInfo->HBlanckingPixelsMSBs);
}

inline Int32 EDIDDTDGetVerticalActivePixels(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->VActivePixelsLSBs, detInfo->VActivePixelsMSBs);
}

inline Int32 EDIDDTDGetVerticalBlankingPixels(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->VBlanckingPixelsLSBs, detInfo->VBlanckingPixelsMSBs);
}

inline Int32 EDIDDTDGetHorizontalFrontPorchPixels(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->HFrontPorchLSBs, detInfo->HFrontPorchMSBs);
}

inline Int32 EDIDDTDGetHorizontalSyncPulseWidth(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->HSyncPulseWidthLSBs, detInfo->HSyncPulseWidthMSBs);
}

inline Int32 EDIDDTDGetVerticalFrontPorchPixels(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->VFrontPorchLSBs, detInfo->VFrontPorchMSBs);
}

inline Int32 EDIDDTDGetVerticalSyncPulseWidth(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->VSyncPulseWidthLSBs, detInfo->VSyncPulseWidthMSBs);
}

inline Int32 EDIDDTDGetHorizontalImageSize(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->HImageSizeLSBs, detInfo->HImageSizeMSBs);
}

inline Int32 EDIDDTDGetVerticalImageSize(const EDIDDetailedTimingDescriptor *detInfo) {
	return EDIDDTDMergeBits(detInfo->VImageSizeLSBs, detInfo->VImageSizeMSBs);
}

void EDIDDumpStructure(const EDID *edid);

static void __staticasserts() {
	static_assert(sizeof(EDIDManufacturerID) == 2, "EDIDManufacturerID is not of size 2 bytes");
	static_assert(sizeof(EDIDDigitalInput) == 1, "EDIDDigitalInput is not of size 1 bytes");
	static_assert(sizeof(EDIDAnalogInput) == 1, "EDIDAnalogInput is not of size 1 bytes");
	static_assert(sizeof(EDIDSupportedFeatured) == 1, "EDIDSupportedFeatured is not of size 1 bytes");
	static_assert(sizeof(EDIDChromaticityCoordinates) == 10, "EDIDChromaticityCoordinates is not of size 10 bytes");
	static_assert(sizeof(EDIDTimingInformation) == 2, "EDIDTimingInformation is not of size 2 bytes");
	static_assert(sizeof(EDIDDetailedTimingDescriptor) == 18, "EDIDDetailedTimingDescriptor is not of size 18 bytes");

	static_assert(offsetof(EDIDHeader, HeaderPattern) == 0);
	static_assert(offsetof(EDIDHeader, Manufacturer.IdData) == 8);
	static_assert(offsetof(EDIDHeader, ManufacturerProductCode) == 10);
	static_assert(offsetof(EDIDHeader, Serial) == 12);
	static_assert(offsetof(EDIDHeader, ManufactureWeek) == 16);
	static_assert(offsetof(EDIDHeader, ManufactureYear) == 17);
	static_assert(offsetof(EDIDHeader, EDIDVersion) == 18);
	static_assert(offsetof(EDIDHeader, EDIDRevision) == 19);

	static_assert(offsetof(EDIDBasicDisplayParameters, DigitalInput) == 0);
	static_assert(offsetof(EDIDBasicDisplayParameters, AnalogInput) == 0);
	static_assert(offsetof(EDIDBasicDisplayParameters, HCmScreenSize) == 1);
	static_assert(offsetof(EDIDBasicDisplayParameters, VCmScreenSize) == 2);
	static_assert(offsetof(EDIDBasicDisplayParameters, DisplayGamma) == 3);

	static_assert(offsetof(EDIDDetailedTimingDescriptor, PixelClock) == 0);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, HActivePixelsLSBs) == 2);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, HBlanckingPixelsLSBs) == 3);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, VActivePixelsLSBs) == 5);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, VBlanckingPixelsLSBs) == 6);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, HImageSizeLSBs) == 12);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, VImageSizeLSBs) == 13);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, HBorderPixels) == 15);
	static_assert(offsetof(EDIDDetailedTimingDescriptor, VBorderLines) == 16);

	static_assert(offsetof(EDID, Header) == 0);
	static_assert(offsetof(EDID, BasicDisplayParameters) == 20);
	static_assert(offsetof(EDID, ChromaticityCoordinates) == 25);
	static_assert(offsetof(EDID, EstablishedTimingBitmap) == 35);
	static_assert(offsetof(EDID, TimingInformations) == 38);
	static_assert(offsetof(EDID, Descriptor1) == 54);
	static_assert(offsetof(EDID, Descriptor2) == 72);
	static_assert(offsetof(EDID, Descriptor3) == 90);
	static_assert(offsetof(EDID, Descriptor4) == 108);
	static_assert(offsetof(EDID, Extensions) == 126);
	static_assert(offsetof(EDID, Checksum) == 127);

}

#endif
