/* MM32F0140 Cortex-M0 startup (GCC) */

.syntax unified
.cpu cortex-m0
.fpu softvfp
.thumb

.global g_pfnVectors
.global Default_Handler

/* Symbols from linker script */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

/* =========================================================== */
/* Reset Handler                                                */
/* =========================================================== */
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr   r0, =_estack
    mov   sp, r0

    /* Copy .data init values from FLASH to RAM */
    ldr   r0, =_sdata
    ldr   r1, =_edata
    ldr   r2, =_sidata
    movs  r3, #0
    b     CopyDataCheck
CopyDataLoop:
    ldr   r4, [r2, r3]
    str   r4, [r0, r3]
    adds  r3, r3, #4
CopyDataCheck:
    adds  r4, r0, r3
    cmp   r4, r1
    bcc   CopyDataLoop

    /* Zero .bss */
    ldr   r2, =_sbss
    ldr   r4, =_ebss
    movs  r3, #0
    b     ZeroBssCheck
ZeroBssLoop:
    str   r3, [r2]
    adds  r2, r2, #4
ZeroBssCheck:
    cmp   r2, r4
    bcc   ZeroBssLoop

    bl    SystemInit
    bl    __libc_init_array
    bl    main

LoopForever:
    b     LoopForever
.size Reset_Handler, .-Reset_Handler

/* =========================================================== */
/* Default Handler — infinite loop for unhandled IRQs           */
/* =========================================================== */
.section .text.Default_Handler, "ax", %progbits
Default_Handler:
    b     Default_Handler
.size Default_Handler, .-Default_Handler

/* =========================================================== */
/* Vector Table (Cortex-M0: 16 system + up to 32 external)      */
/* =========================================================== */
.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack                       /* 0x00 Initial SP */
    .word Reset_Handler                 /* 0x04 Reset */
    .word NMI_Handler                   /* 0x08 NMI */
    .word HardFault_Handler             /* 0x0C HardFault */
    .word 0                             /* 0x10 Reserved */
    .word 0                             /* 0x14 */
    .word 0                             /* 0x18 */
    .word 0                             /* 0x1C */
    .word 0                             /* 0x20 */
    .word 0                             /* 0x24 */
    .word 0                             /* 0x28 */
    .word SVC_Handler                   /* 0x2C SVCall */
    .word 0                             /* 0x30 */
    .word 0                             /* 0x34 */
    .word PendSV_Handler                /* 0x38 PendSV */
    .word SysTick_Handler               /* 0x3C SysTick */

    /* External interrupts (STM32F0xx-compatible numbering) */
    .word WWDG_IRQHandler               /* 0  */
    .word PVD_IRQHandler                /* 1  */
    .word RTC_IRQHandler                /* 2  */
    .word FLASH_IRQHandler              /* 3  */
    .word RCC_IRQHandler                /* 4  */
    .word EXTI0_1_IRQHandler            /* 5  */
    .word EXTI2_3_IRQHandler            /* 6  */
    .word EXTI4_15_IRQHandler           /* 7  */
    .word 0                             /* 8  */
    .word DMA1_Channel1_IRQHandler      /* 9  */
    .word DMA1_Channel2_3_IRQHandler    /* 10 */
    .word DMA1_Channel4_5_IRQHandler    /* 11 */
    .word ADC1_COMP_IRQHandler          /* 12 */
    .word TIM1_BRK_UP_TRG_COM_IRQHandler/* 13 */
    .word TIM1_CC_IRQHandler            /* 14 */
    .word TIM2_IRQHandler               /* 15 */
    .word TIM3_IRQHandler               /* 16 */
    .word 0                             /* 17 */
    .word 0                             /* 18 */
    .word TIM14_IRQHandler              /* 19 */
    .word 0                             /* 20 */
    .word TIM16_IRQHandler              /* 21 */
    .word TIM17_IRQHandler              /* 22 */
    .word I2C1_IRQHandler               /* 23 */
    .word 0                             /* 24 */
    .word SPI1_IRQHandler               /* 25 */
    .word SPI2_IRQHandler               /* 26 */
    .word UART1_IRQHandler              /* 27 */
    .word UART2_IRQHandler              /* 28 */
    .word UART3_IRQHandler              /* 29 */
    .word CAN_IRQHandler                /* 30 */
    .word 0                             /* 31 */
.size g_pfnVectors, .-g_pfnVectors

/* =========================================================== */
/* Weak handler aliases — all default to Default_Handler        */
/* =========================================================== */
.macro WEAK_ALIAS name
    .weak  \name
    .thumb_set \name, Default_Handler
.endm

WEAK_ALIAS NMI_Handler
WEAK_ALIAS HardFault_Handler
WEAK_ALIAS SVC_Handler
WEAK_ALIAS PendSV_Handler
WEAK_ALIAS SysTick_Handler

WEAK_ALIAS WWDG_IRQHandler
WEAK_ALIAS PVD_IRQHandler
WEAK_ALIAS RTC_IRQHandler
WEAK_ALIAS FLASH_IRQHandler
WEAK_ALIAS RCC_IRQHandler
WEAK_ALIAS EXTI0_1_IRQHandler
WEAK_ALIAS EXTI2_3_IRQHandler
WEAK_ALIAS EXTI4_15_IRQHandler
WEAK_ALIAS DMA1_Channel1_IRQHandler
WEAK_ALIAS DMA1_Channel2_3_IRQHandler
WEAK_ALIAS DMA1_Channel4_5_IRQHandler
WEAK_ALIAS ADC1_COMP_IRQHandler
WEAK_ALIAS TIM1_BRK_UP_TRG_COM_IRQHandler
WEAK_ALIAS TIM1_CC_IRQHandler
WEAK_ALIAS TIM2_IRQHandler
WEAK_ALIAS TIM3_IRQHandler
WEAK_ALIAS TIM14_IRQHandler
WEAK_ALIAS TIM16_IRQHandler
WEAK_ALIAS TIM17_IRQHandler
WEAK_ALIAS I2C1_IRQHandler
WEAK_ALIAS SPI1_IRQHandler
WEAK_ALIAS SPI2_IRQHandler
WEAK_ALIAS UART1_IRQHandler
WEAK_ALIAS UART2_IRQHandler
WEAK_ALIAS UART3_IRQHandler
WEAK_ALIAS CAN_IRQHandler
