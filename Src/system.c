#include "stm32_regs.h"

/*
 * Called by the startup assembly before main().
 * Use the reset-default 16 MHz HSI clock.
 */
void SystemInit(void)
{
    SCB_VTOR = 0x08000000UL;
}

/*
 * Initialization hook used by __libc_init_array().
 * Our application needs no additional initialization here.
 */
void _init(void)
{
}