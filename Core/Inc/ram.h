/*
 * Defines memory management functions for allocate data in the ".ram_data" section
 *
 *  Created on: Nov 4, 2021
 *      Author: Andrea Monzani [Mat 952817]
 */

#ifndef INC_RAM_H_
#define INC_RAM_H_

#include <stddef.h>

/// Allocates a new block of data in the ram_data section
/// @param size Size of the memory block, in bytes
/// 
/// @return On success, a pointer to the memory block allocated by the function.
/// The type of this pointer is always void*, which can be cast to the desired type of data pointer in order to be dereferenceable.
/// If the function failed to allocate the requested block of memory, a null pointer is returned.
/// 
/// @remarks The allocator is not thread safe
void* ralloc(size_t size);

/// Deallocates a block of data from the ram
/// @param ptr Pointer of the block
/// @param size Size of the block to be released in bytes
void rfree(void* ptr, size_t size);
#endif /* INC_RAM_H_ */
