/* Free-running 1 ms time base (SysTick).
 *
 * Every timing decision in this firmware — debounce windows, LED blink
 * intervals — is derived from here. Do NOT write NOP-count busy-waits:
 * Cortex-M0 has no hardware divider, so a loop whose condition divides
 * (`i < SystemCoreClock / 8000`) pays an __aeabi_uidiv call per iteration and
 * runs ~21x slow (measured 2026-09-22). See docs/hardware.md §4.
 *
 * tick_init() must run after clock_init_pll48mhz(), because SysTick's reload
 * value is computed from SystemCoreClock. */
#ifndef X3_TICK_H
#define X3_TICK_H

#include <stdint.h>

void tick_init(void);

/* Milliseconds since tick_init(). Wraps every ~49 days. */
uint32_t tick_ms(void);

/* Wrap-safe elapsed time. Always compare with this, never with `now > then`. */
uint32_t tick_elapsed_ms(uint32_t since);

void tick_delay_ms(uint32_t ms);

#endif /* X3_TICK_H */
