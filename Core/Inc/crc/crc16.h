/*
 * crc16.h
 *
 *  Created on: 9 dic 2021
 *      Author: mnznn
 */

#ifndef INC_CRC_CRC16_H_
#define INC_CRC_CRC16_H_

#define CRC16_ZERO 0

#include <stdint.h>

void Crc16Initialize();

uint16_t Crc16Add(uint16_t crc, uint8_t data);

#endif /* INC_CRC_CRC16_H_ */
