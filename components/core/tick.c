#include "tick.h"
#include "board.h"

static volatile uint32_t s_ms;

/* Weak-aliased to Default_Handler in startup_mm32f0140.s; this overrides it. */
void SysTick_Handler(void)
{
    ++s_ms;
}

void tick_init(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t tick_ms(void)
{
    return s_ms;
}

uint32_t tick_elapsed_ms(uint32_t since)
{
    return (uint32_t)(s_ms - since);
}

void tick_delay_ms(uint32_t ms)
{
    const uint32_t start = s_ms;
    while (tick_elapsed_ms(start) < ms) {
        __asm__ volatile ("wfi");
    }
}
