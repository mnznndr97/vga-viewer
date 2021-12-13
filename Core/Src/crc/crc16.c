/*
 * crc16.c
 *
 *  Created on: 9 dic 2021
 *      Author: mnznn
 */

#include <crc/crc16.h>
#include <assert.h>

static uint16_t CRCTable[256];

void Crc16Initialize() {
    // SD CRC polinomial is x^16 + x^12 + x^5 + 1 (MSB can be ignored)
    const uint16_t Poly = 0x1021;

    // Let's pre-generate the 256 bytes -> CRC map
    for (int i = 0; i < 256; ++i) {
        // Move divident byte into MSB of 16Bit CRC */
        uint16_t crc = (uint16_t)(i << 8);

        for (int j = 0; j < 8; ++j) {
            if ((crc & 0x8000) != 0) {
                crc <<= 1;
                crc ^= Poly;
            }
            else
            {
                crc <<= 1;
            }
        }

        CRCTable[i] = crc;
    }
}

uint16_t Crc16Add(uint16_t crc, uint8_t data) {
    uint8_t pos = (uint8_t)((crc >> 8) ^ data);
    return (uint16_t)((crc << 8) ^ ((uint16_t)CRCTable[pos]));
}


