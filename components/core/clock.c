#include "clock.h"
#include "board.h"

/* MM32F0140 PLL is N/M-fractional; the HAL's RCC_PLLConfig + RCC_PLLMul_x
 * enum hides that and presents an STM32-style integer multiplier. HSI here
 * is wired as the 8 MHz reference (HSI_VALUE_PLL_ON = 48 MHz / 6 in
 * reg_common.h), so × 6 lands cleanly on 48 MHz.
 *
 * The two-step HCLK ramp (/4 -> /2 -> /1) mirrors the vendor's
 * SetSysClockToXX_HSI sequence so the AHB never momentarily overshoots
 * while PLL settling and bus dividers are being reshuffled. */
void clock_init_pll48mhz(void)
{
    RCC_HSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET) { }

    RCC_SYSCLKConfig(RCC_HSI);
    while ((RCC_GetSYSCLKSource() & 0x0CU) != 0x00U) { }
    RCC_PLLCmd(DISABLE);

    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    FLASH_SetLatency(FLASH_Latency_1);

    RCC_HCLKConfig(RCC_SYSCLK_Div4);
    RCC_PCLK1Config(RCC_HCLK_Div2);
    RCC_PCLK2Config(RCC_HCLK_Div1);

    /* The MM32F0140 PLL won't lock unless the PWR interface clock is on
     * — vendor's SetSysClockToXX_HSI does this right before PLLCFGR writes
     * and PLLRDY never goes high without it. */
    RCC_APB1PeriphClockCmd(RCC_APB1ENR_PWR, ENABLE);

    RCC_PLLConfig(RCC_HSI_Div, RCC_PLLMul_6);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) { }

    RCC_SYSCLKConfig(RCC_PLL);
    while ((RCC_GetSYSCLKSource() & 0x0CU) != 0x08U) { }

    RCC_HCLKConfig(RCC_SYSCLK_Div2);
    for (volatile uint32_t i = 0; i < 1000U; ++i) {
        __asm__ volatile ("nop");
    }
    RCC_HCLKConfig(RCC_SYSCLK_Div1);

    SystemCoreClock = 48000000U;
}
