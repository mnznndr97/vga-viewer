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

typedef enum _EDIDTiming {
	EDIDTiming720x400At70Hz = 7,
	EDIDTiming720x400At88Hz = 6,
	EDIDTiming640x480At60Hz = 5,
	EDIDTiming640x480At67Hz = 4,
	EDIDTiming640x480At72Hz = 3,
	EDIDTiming640x480At75Hz = 2,
	EDIDTiming800x600At56Hz = 1,
	EDIDTiming800x600At60Hz = 0,
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

typedef struct _EDIDDescriptor {
	union {
		BYTE Raw[18];
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

inline bool EDIDIsTimingSupported(const EDID *edid, EDIDTiming timing) {
	int byteIndex = timing / 8;
	if (byteIndex < 0 || byteIndex >= 3) {
		// Time index is out of range
		return false;
	}

	int bitIndex = timing % 8;
	BYTE bitMask = 1 << bitIndex;
	return (edid->EstablishedTimingBitmap.Data[byteIndex] & bitMask) == bitMask;
}

inline bool EDIDIsTimingInfoFilled(const EDIDTimingInformation *edidTimingInfo) {
	BYTE *bytePtr = (BYTE*) edidTimingInfo;
	return bytePtr[0] != 0x01 && bytePtr[1] != 0x01;
}

static void __staticasserts() {
	static_assert(sizeof(EDIDManufacturerID) == 2, "EDIDManufacturerID is not of size 2 bytes");
	static_assert(sizeof(EDIDDigitalInput) == 1, "EDIDDigitalInput is not of size 1 bytes");
	static_assert(sizeof(EDIDAnalogInput) == 1, "EDIDAnalogInput is not of size 1 bytes");
	static_assert(sizeof(EDIDSupportedFeatured) == 1, "EDIDSupportedFeatured is not of size 1 bytes");
	static_assert(sizeof(EDIDChromaticityCoordinates) == 10, "EDIDChromaticityCoordinates is not of size 10 bytes");
	static_assert(sizeof(EDIDTimingInformation) == 2, "EDIDTimingInformation is not of size 2 bytes");

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
