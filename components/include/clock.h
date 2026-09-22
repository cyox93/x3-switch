#ifndef X3_CLOCK_H
#define X3_CLOCK_H

/* Bring SYSCLK up from the reset-default HSI (~8 MHz) to 48 MHz by running
 * the PLL off HSI × 6. After this returns:
 *   SYSCLK = HCLK = PCLK2 = 48 MHz, PCLK1 = 24 MHz.
 * SystemCoreClock is updated so existing delay loops keep timing correctly. */
void clock_init_pll48mhz(void);

#endif /* X3_CLOCK_H */
