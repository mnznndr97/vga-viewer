#include <crc/crc16.h>
#include <assertion.h>
#include <binary.h>

static uint16_t CRCTable[256];

#if _DEBUG
/// Super simple debug initialization flag. If we forgot to call the Initialize function, the assertion in the Add method will fail
static uint8_t _initialized = 0;
#endif

void Crc16Initialize() {
    // SD CRC polynomial is x^16 + x^12 + x^5 + 1 (MSB can be ignored)
    const uint16_t Poly = 0x1021;

    // Let's pre-generate the 256 bytes -> CRC map
    for (int i = 0; i < 256; ++i) {
        // Move dividend byte into MSB of 16Bit CRC */
        uint16_t crc = (uint16_t)(i << 8);

        // We loop over the 8 data bits
        for (int j = 0; j < 8; ++j) {

            if ((crc & 0x8000) != 0) {
                // Exiting MSB is 1, we shift to remove the bit then XOR
                crc = (uint16_t)MASKI2SHORT(crc << 1);
                crc ^= Poly;
            }
            else {
                crc = (uint16_t)MASKI2SHORT(crc << 1);
            }
        }

        CRCTable[i] = crc;
    }

#if _DEBUG
    _initialized = 1;
#endif
}

uint16_t Crc16Add(uint16_t crc, uint8_t data) {
#if _DEBUG
    DebugAssert(_initialized != 0);
#endif

    // We need to XOR the current CRC with the data as usual (by discarding the 8 LSB of the CRC)
    uint8_t pos = (uint8_t)((crc >> 8) ^ data);
    // Once we have the crc of our combined data + intermediate crc, we need to combined it with the right part of the old CRC
    // This comes from the fact that the CRC calculation is done only on 1 byte at a time, so when two bytes are queued, only the LSB of the 
    // first byte CRC is XORer with the input dividend.
    // http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html#ch5 explains the operation using the column division
    return (uint16_t)((crc << 8) ^ ((uint16_t)CRCTable[pos]));
}

