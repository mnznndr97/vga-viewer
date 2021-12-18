/*
 * ocr.h
 *
 *  Created on: 18 dic 2021
 *      Author: mnznn
 */

#ifndef INC_SD_OCR_H_
#define INC_SD_OCR_H_

#include <typedefs.h>

 // OCR register definition
 /// \remarks The struct is layed out as if read in little endian from an int 32
 /// This is what happens when casting the result buffer to an R3 pointer
typedef struct _OCRRegister {
    /********* UInt32 LSB -> Bits from 31 to 24 ******/
    /// Only USH-I card support this bit
    UInt32 Switch1p8Accepted : 1; // Bit 24
    UInt32 Reserved : 2; // Bit 25-26
    /// Over 2TB support Status. Only SDUC card support this bit
    UInt32 CO2T : 1; // Bit 27
    UInt32 Reserved2 : 1; // Bit 28
    UInt32 UHSIICardStatus : 1; // Bit 29
    /// Card Capacity Status. This bit is valid only whe the canrd power up status bit is set
    UInt32 CCS : 1; // Bit 30
    /// Power up status. Set to low if the card has not finished the power up routine
    UInt32 PowerUpStatus : 1; // Bit 31

    /********* UInt32 byte 2 -> Bits from 23 to 16 ******/
    UInt32 v2p8tov2p9 : 1; // Bit 16
    UInt32 v2p9tov3p0 : 1; // Bit 17
    UInt32 v3p0tov3p1 : 1; // Bit 18
    UInt32 v3p1tov3p2 : 1; // Bit 19
    UInt32 v3p2tov3p3 : 1; // Bit 20
    UInt32 v3p3tov3p4 : 1; // Bit 21
    UInt32 v3p4tov3p5 : 1; // Bit 22
    UInt32 v3p5tov3p6 : 1; // Bit 23

    /********* UInt32 byte 3 -> Bits from 15 to 8 ******/
    UInt32 VddWindowReserved1 : 7; // Bits 14 - 8
    UInt32 v2p7tov2p8 : 1; // Bit 15

    /********* UInt32 MSB -> Bits from 7 to 0 ******/
    UInt32 VddWindowReserved : 8; // Bits 7 - 0
} OCRRegister;

#endif /* INC_SD_OCR_H_ */
