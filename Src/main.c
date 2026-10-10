#include "uart_dma.h"
#include "stm32_regs.h"

int main(void)
{
    /* DBGMCU_CR: enable debugger access during Sleep mode. */
    REG32(0xE0042004UL) |= 1UL;

    uart_dma_init();

    for (;;)
    {
        __asm volatile ("dsb" ::: "memory");
        __asm volatile ("wfi" ::: "memory");
    }
}