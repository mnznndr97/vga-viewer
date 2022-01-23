#include <ram.h>
#include <stdint.h>

#ifdef STM32F407xx

// For STM32F407 we know the ram size
#define RAM_SIZE 128 * 1024

// We allocate the entire block
static __attribute__((section(".ram_data")))        uint8_t s_ramdata[RAM_SIZE];

#endif // STM32F407xx

static size_t s_remaining = RAM_SIZE;
static size_t s_current = (size_t) &s_ramdata[0];

void* ralloc(size_t size) {
    // Simple size check
	if (size <= 0 || size > s_remaining)
		return NULL;

    // Super simple allocation mechanism: increment current pointer and decrement remaining counter
    // This is not thread safe for the moment
	void *dataPtr = (void*) s_current;
	s_current += size;
	s_remaining -= size;

	return dataPtr;
}

void rfree(void* ptr, size_t size) {
    // Super simple deallocation mechanism: decrement current pointer and increment remaining counter
    // This is not thread safe for the moment and no checks are performed
    s_current -= size;
    s_remaining += size;
}
