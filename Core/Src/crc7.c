/*
 * crc7.c
 *
 *  Created on: May 18, 2021
 *      Author: mnznn
 */

#include <crc7.h>
#include <assert.h>

static uint8_t CRCTable[256];

void Crc7Initialize() {
	// SD CRC polinomial is x^7 + x^3 + 1 (in binary 0b10001001)
	const uint8_t Poly = 0x89;

	// Let's pre-generate the 256 bytes -> CRC map
	for (int i = 0; i < 256; ++i) {
		CRCTable[i] = (i & 0x80) ? i ^ Poly : i;

		// Data polynomial, per SD standard, must be multiplied by x ^ 7
		// So we just apply the CRC xor shift algorithm 7 times
		for (int j = 1; j < 8; ++j) {
			CRCTable[i] <<= 1;
			if (CRCTable[i] & 0x80)
				CRCTable[i] ^= Poly;
		}

		// CRC7 MSB should always be 0 per algorithm
		assert(CRCTable[i] & 0x80 == 0);
	}
}

uint8_t Crc7Add(uint8_t crc, uint8_t data) {
	// To cumulate the CRC, we shift it (MSB should be 0 so we don't need to perform XOR with the Poly)
	// since new data is entering in the cycle and we compute the XOR with the data value

	// We should then perform the * x^7 polynomial multiplication, which eventually result in a simple dereference of our pre-calculated
	// CRC value at the newly calculated value
	return CRCTable[(crc << 1) ^ data];
}

