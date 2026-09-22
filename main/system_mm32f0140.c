#include "board.h"

/* X3 has no HSE crystal populated — the SDK's stock SystemInit() in
 * vendor/.../Source/system_mm32f0140.c routes the PLL off HSE and deadlocks
 * the chip when run on this board. So we ship our own empty SystemInit and
 * stay on the reset-default HSI clock. */
void SystemInit(void)
{
}

/* HSI reset-default. The MM32F0140 datasheet rates HSI as 48MHz divided by 6
 * (=8MHz) before any PLL/MUX changes, mirroring the SDK's HSI_VALUE. */
u32 SystemCoreClock = 8000000U;

/* newlib-nano's __libc_init_array calls _init / _fini around the C++ static
 * constructor pass. We don't use C++, so provide weak no-op stubs. */
__attribute__((weak)) void _init(void) {}
__attribute__((weak)) void _fini(void) {}
