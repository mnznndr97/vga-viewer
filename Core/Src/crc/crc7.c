/*
 * crc7.c
 *
 *  Created on: May 18, 2021
 *      Author: mnznn
 */

#include <crc/crc7.h>
#include <assertion.h>

static uint8_t CRCTable[256];

void Crc7Initialize() {
	// SD CRC polinomial is x^7 + x^3 + 1 (in binary 0b10001001)
	const uint8_t Poly = 0x89;

	// Let's pre-generate the 256 bytes -> CRC map
	for (int i = 0; i < 256; ++i) {
		CRCTable[i] = (uint8_t) ((i & 0x80) ? i ^ Poly : i);

		// Data polynomial, per SD standard, must be multiplied by x ^ 7
		// So we just apply the CRC xor shift algorithm 7 times
		for (int j = 1; j < 8; ++j) {
			CRCTable[i] <<= 1;
			if ((CRCTable[i] & 0x80) != 0)
				CRCTable[i] = (uint8_t) (CRCTable[i] ^ Poly);
		}

		// CRC7 MSB should always be 0 per algorithm
		DebugAssert((CRCTable[i] & 0x80) == 0);
	}
}

uint8_t Crc7Add(uint8_t crc, uint8_t data) {
	// To cumulate the CRC, we shift it (MSB should be 0 so we don't need to perform XOR with the Poly)
	// since new data is entering in the cycle and we compute the XOR with the data value

	// We should then perform the * x^7 polynomial multiplication, which eventually result in a simple dereference of our pre-calculated
	// CRC value at the newly calculated value
	uint8_t index = (uint8_t) ((crc << 1) ^ data);
	return CRCTable[index];
}

uint8_t Crc7Calculate(const uint8_t *pData, size_t count) {
	if (count == 0 || pData == NULL)
		return CRC7_ZERO;

	uint8_t crc = CRC7_ZERO;
	// Until pointer is not aligned, we have to access byte per byte
	while ((((uintptr_t) pData) & 0x3)) {
		crc = Crc7Add(crc, *pData);

		// We update pointer and number of item to read
		++pData;
		--count;
	}

	// Until we can read a DWord, we access the 32bit data to gain some performance
	while (count >= 4) {
		uint32_t data = *((const uint32_t*) pData);

		// NB: data are read in little endian order in our U32 so
		// we must crc them in "reverse" order
		crc = Crc7Add(crc, ((data) & 0xFF));
		crc = Crc7Add(crc, ((data >> 8) & 0xFF));
		crc = Crc7Add(crc, ((data >> 16) & 0xFF));
		crc = Crc7Add(crc, ((data >> 24) & 0xFF));

		pData += 4;
		count -= 4;
	}

	// Let's also handle trailing bytes
	while (count > 0) {
		crc = Crc7Add(crc, *pData);

		++pData;
		--count;
	}

	return crc;
}

