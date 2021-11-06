/*
 * ram.h
 *
 *  Created on: Nov 4, 2021
 *      Author: mnznn
 */

#ifndef INC_RAM_H_
#define INC_RAM_H_

#include <stddef.h>

/// Allocates a new block of data in the ram_data section
/// \param size Size of the memory block, in bytes
/// 
/// \return On success, a pointer to the memory block allocated by the function.
/// The type of this pointer is always void*, which can be cast to the desired type of data pointer in order to be dereferenceable.
/// If the function failed to allocate the requested block of memory, a null pointer is returned.
/// 
/// \remarks The allocator is not thread safe
void* ralloc(size_t size);

#endif /* INC_RAM_H_ */
