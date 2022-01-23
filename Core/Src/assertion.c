#include <assertion.h>
#include <assert.h>
#include <stm32f407xx.h>

// Error_Handler is defined in the main.c module
extern void Error_Handler(void);

void DebugAssert(bool condition) {
#ifdef CUSTOMASSERT
    if (!condition) Error_Handler();
#else
    assert(condition);
#endif
}

void DebugWriteChar(uint32_t c)
{
#if STM32F407xx
    ITM_SendChar(c);
#endif
}
