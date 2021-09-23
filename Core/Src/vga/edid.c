/*
 * edid.c
 *
 *  Created on: 20 set 2021
 *      Author: mnznn
 */

#include <vga/edid.h>

void EDIDGetManufacturer(const EDID *edid, char *buffer) {
	// 'A' Is written as 1 in our bit so we simple add the 'A' - 1 = '@' to get back to ASCII

	UInt16 value = *((UInt16*) &edid->Header.Manufacturer.IdData[0]);
	// We are in a LittleEndian system, let's just swap the bytes
	value = (value << 8) | ((value >> 8) & 0xFF);

	buffer[0] = ((value >> 10) & 0x1F) + '@';
	buffer[1] = ((value >> 5) & 0x1F) + '@';
	buffer[2] = (value & 0x1F) + '@';
}

BOOL EDIDIsTimingSupported(const EDID *edid, EDIDTiming timing) {
	int byteIndex = timing / 8;
	if (byteIndex < 0 || byteIndex >= 3) {
		// Time index is out of range
		return false;
	}

	int bitIndex = timing % 8;
	BYTE bitMask = 1 << bitIndex;
	return (edid->EstablishedTimingBitmap.Data[byteIndex] & bitMask) == bitMask;
}

void EDIDDumpStructure(const EDID *edid) {
	printf("Dumping EDID ...\r\n");

	char manufacturer[4];
	EDIDGetManufacturer(edid, manufacturer);
	manufacturer[3] = '\0';

	printf("\tVersion: %d.%d\r\n", edid->Header.EDIDVersion, edid->Header.EDIDRevision);
	printf("\tManufacturer: %s\r\n", manufacturer);
	printf("\tProduct code: %d\r\n", edid->Header.ManufacturerProductCode);
	printf("\tWeek: %d\r\n", edid->Header.ManufactureWeek);
	printf("\tYear: %d\r\n", 1990 + edid->Header.ManufactureYear);

	if (edid->BasicDisplayParameters.DigitalInput.ISDigitalInput)
		printf("\tDigital input\r\n");
	else {
		printf("\tAnalog input\r\n");

		switch ((EDIDAnalogVoltage) edid->BasicDisplayParameters.AnalogInput.VoltageLevel) {
		case EDIDAVoltage0p7Tom0p3:
			printf("\t\tVoltage levels: +0.7/-0.3 V\r\n");
			break;
		case EDIDAVoltage0p714Tom0p286:
			printf("\t\tVoltage levels: +0.714/-0.286 V\r\n");
			break;
		case EDIDAVoltage1p0Tom0p4:
			printf("\t\tVoltage levels: +1.0/-0.4 V\r\n");
			break;
		case EDIDAVoltage0p7To0p0:
			printf("\t\tVoltage levels: +0.7/0 V\r\n");
			break;
		}

		if (edid->BasicDisplayParameters.AnalogInput.SeparateSyncSupported)
			printf("\t\tSeparate sync \033[1;32msupported\033[0m\r\n");
		else
			printf("\t\tSeparate sync \033[1;31mNOT supported\033[0m\r\n");
	}

	printf("\tBasic timings\r\n");
	if (EDIDIsTimingSupported(edid, EDIDTiming640x480At60Hz)) {
		printf("\t\t640x480 @ 60Hz \033[1;32msupported\033[0m\r\n");
	}

}

