/*
 * Contains the typedefs for the common integer types (signed and unsigned)
 * and some useful macro for VisualStudio intellisense
 * 
 *  Created on: Sep 9, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef INC_TYPEDEFS_H_
#define INC_TYPEDEFS_H_

#ifdef __INTELLISENSE__

#define __attribute__(A) /* do nothing */
#define section(A) /* do nothing */

#endif

typedef bool BOOL;
typedef int8_t SBYTE;
typedef int16_t Int16;
typedef int32_t Int32;
typedef int64_t Int64;

typedef uint8_t BYTE;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef uint64_t UInt64;

typedef const UInt32* PCUInt32;
typedef const UInt16* PCUInt16;
typedef const BYTE* PCBYTE;

#endif /* INC_TYPEDEFS_H_ */
