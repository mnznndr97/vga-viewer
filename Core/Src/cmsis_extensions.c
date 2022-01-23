#include <cmsis_extensions.h>

 // ##### Private define declarations #####
#define MAX_STACK_WSIZE 32768

// ##### Private forward declarations #####
static void EnforeStackProtection(TaskHandle_t handle);


// ##### Private function implementations #####

void EnforeStackProtection(TaskHandle_t handle)
{
    // To assert that we have run out of stack, we can ALSO implement this simple check
    // FreeRTOS gives us the possibility to check the remaining space in the stack of a specific task
    // So we simply check that the space is greater than zero or less that the maximum available stack space
    // on the device; SRAM in our STMF407 (the code is in CCMRAM, but we may use some static allocated TCB)

    // NB: If the stack is corrupted, other memory areas where FreeRTOS structure are allocated might have been corrupted
    // so better not rely on FreeRTOS informations where possible (for example, here better not rely on a task stack size but use
    // a constant, like the maximum memory available)
    // NB2: We are using this method (https://www.freertos.org/Stacks-and-stack-overflow-checking.html, method 2) since checking the StackPointer 
    // is not a good technique. Our problem with the stack is mostly on the prinft trace so, in the middleof a function call and not when the scheduler
    // kicks in
    UBaseType_t remainingStackWords = uxTaskGetStackHighWaterMark(handle);
    DebugAssert(remainingStackWords > 0 && remainingStackWords < MAX_STACK_WSIZE);
}

// ##### Public function implementation #####

osStatus_t osExDelayMs(uint32_t ms) {
    // One tick is equal to 1 ms
    return osDelay(ms);
}

void osExEnforeStackProtection(osThreadId_t handle)
{
    EnforeStackProtection((TaskHandle_t)handle);
}
