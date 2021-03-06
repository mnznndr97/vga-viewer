#include <vga/edid.h>
#include <stdio.h>
#include <stdint.h>

BOOL EdidIsChecksumValid(const Edid *edid) {
	// https://en.wikipedia.org/wiki/Extended_Display_Identification_Data#Structure,_version_1.4
	// Sum of all 128 bytes should equal 0 (mod 256)

	const BYTE *bytePtr = (const BYTE*) edid;
	int sum = 0;

	for (size_t i = 0; i < sizeof(Edid); i++) {
		sum += bytePtr[i];
	}

	return (sum % 256) == 0;
}

void EdidGetManufacturer(const Edid *edid, char *buffer) {
	// 'A' Is written as 1 in our bit so we simple add the 'A' - 1 = '@' to get back to ASCII

	UInt16 value = *((UInt16*) &edid->Header.Manufacturer.IdData[0]);
	// We are in a LittleEndian system, let's just swap the bytes
	value = U16ChangeEndiannes(value);

	buffer[0] = (char)(((value >> 10) & 0x1F) + '@');
	buffer[1] = (char)(((value >> 5) & 0x1F) + '@');
	buffer[2] = (char)((value & 0x1F) + '@');
}

Int32 EdidDtdGetHorizontalActivePixels(const EdidDetailedTimingDescriptor *detInfo) {
	return EdidDtdMergeBits(detInfo->HActivePixelsLSBs, detInfo->HActivePixelsMSBs);
}

BOOL EdidIsTimingSupported(const Edid *edid, EdidTiming timing) {
	int byteIndex = timing / 8;
	if (byteIndex < 0 || byteIndex >= 3) {
		// Time index is out of range
		return false;
	}

	int bitIndex = timing % 8;
	int bitMask = 1 << bitIndex;
	return (edid->EstablishedTimingBitmap.Data[byteIndex] & bitMask) == bitMask;
}

Int32 EdidDtdMergeBits(BYTE lsb, BYTE msb) {
	return lsb | (msb << 8);
}

void EdidDumpStructure(const Edid *edid) {
	printf("Dumping Edid ...\r\n");

	char manufacturer[4];
	EdidGetManufacturer(edid, manufacturer);
	manufacturer[3] = '\0';

	printf("\tVersion: %d.%d\r\n", edid->Header.EdidVersion, edid->Header.EdidRevision);
	printf("\tManufacturer: %s\r\n", manufacturer);
	printf("\tProduct code: %d\r\n", edid->Header.ManufacturerProductCode);
	printf("\tWeek: %d\r\n", edid->Header.ManufactureWeek);
	printf("\tYear: %d\r\n", 1990 + edid->Header.ManufactureYear);

	if (edid->BasicDisplayParameters.DigitalInput.IsDigitalInput)
		printf("\tDigital input\r\n");
	else {
		printf("\tAnalog input\r\n");

		switch ((EdidAnalogVoltage) edid->BasicDisplayParameters.AnalogInput.VoltageLevel) {
		case EdidAVoltage0p7Tom0p3:
			printf("\t\tVoltage levels: +0.7/-0.3 V\r\n");
			break;
		case EdidAVoltage0p714Tom0p286:
			printf("\t\tVoltage levels: +0.714/-0.286 V\r\n");
			break;
		case EdidAVoltage1p0Tom0p4:
			printf("\t\tVoltage levels: +1.0/-0.4 V\r\n");
			break;
		case EdidAVoltage0p7To0p0:
			printf("\t\tVoltage levels: +0.7/0 V\r\n");
			break;
		}

		if (!edid->BasicDisplayParameters.AnalogInput.BlankToBlackExpected)
			printf("\t\tBlank to black \033[1;32m not expected\033[0m\r\n");
		else
			printf("\t\tBlank to black \033[1;31mexpected\033[0m\r\n");

		if (edid->BasicDisplayParameters.AnalogInput.SeparateSyncSupported)
			printf("\t\tSeparate sync \033[1;32msupported\033[0m\r\n");
		else
			printf("\t\tSeparate sync \033[1;31mNOT supported\033[0m\r\n");

		if (edid->BasicDisplayParameters.AnalogInput.CompositeSyncSupported)
			printf("\t\tComposite sync (on HSync) \033[1;32msupported\033[0m\r\n");
		else
			printf("\t\tComposite sync (on HSync) \033[1;31mNOT supported\033[0m\r\n");

		if (edid->BasicDisplayParameters.AnalogInput.SyncOnGreenSupported)
			printf("\t\tSync on green \033[1;32msupported\033[0m\r\n");
		else
			printf("\t\tSync on green \033[1;31mNOT supported\033[0m\r\n");

		if (!edid->BasicDisplayParameters.AnalogInput.SerratedVsyncPulse)
			printf("\t\tSerrated VSync pulse (on Composite or SOG) \033[1;32mNOT necessary\033[0m\r\n");
		else
			printf("\t\tSerrated VSync pulse (on Composite or SOG) \033[1;31mnecessary\033[0m\r\n");
	}

	printf("\tBasic timings\r\n");
	if (EdidIsTimingSupported(edid, EdidTiming640x480At60Hz)) {
		printf("\t\t640x480 @ 60Hz \033[1;32msupported\033[0m\r\n");
	} else {
		printf("\t\t640x480 @ 60Hz \033[1;31mNOT supported\033[0m\r\n");
	}

	if (EdidIsTimingSupported(edid, EdidTiming800x600At56Hz)) {
		printf("\t\t800x600 @ 56Hz \033[1;32msupported\033[0m\r\n");
	} else {
		printf("\t\t800x600 @ 56Hz \033[1;31mNOT supported\033[0m\r\n");
	}

	if (EdidIsTimingSupported(edid, EdidTiming800x600At60Hz)) {
		printf("\t\t800x600 @ 60Hz \033[1;32msupported\033[0m\r\n");
	} else {
		printf("\t\t800x600 @ 60Hz \033[1;31mNOT supported\033[0m\r\n");
	}

	if (EdidIsTimingSupported(edid, EdidTiming1024x728At60Hz)) {
		printf("\t\t1024x728 @ 60Hz \033[1;32msupported\033[0m\r\n");
	} else {
		printf("\t\t1024x728 @ 60Hz \033[1;31mNOT supported\033[0m\r\n");
	}

	printf("\tDescriptor 2\r\n");
	Int32 hActivePixel = EdidDtdGetHorizontalActivePixels(&edid->Descriptor2.DetailedTiming);
	printf("\t\tActive H: \033[1;31m%" PRId32 "\033[0m pixels\r\n", hActivePixel);

	printf("\tDescriptor 3\r\n");
	hActivePixel = EdidDtdGetHorizontalActivePixels(&edid->Descriptor3.DetailedTiming);
	printf("\t\tActive H: \033[1;31m%" PRId32 "\033[0m pixels\r\n", hActivePixel);

	printf("\tDescriptor 4\r\n");
	hActivePixel = EdidDtdGetHorizontalActivePixels(&edid->Descriptor4.DetailedTiming);
	printf("\t\tActive H: \033[1;31m%" PRId32 "\033[0m pixels\r\n", hActivePixel);

	if (edid->Extensions > 0) {
		printf("\t%d extension to follow\r\n", edid->Extensions);
	}

	if (EdidIsChecksumValid(edid)) {
		printf("\tChecksum is \033[1;32mvalid\033[0m\r\n");
	} else {
		printf("\tChecksum is \033[1;31mNOT valid\033[0m\r\n");
	}
}

