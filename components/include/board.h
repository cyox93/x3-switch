/* Board-level wrapper: pulls in the MindMotion SDK HAL umbrella and exposes
 * the X3 pin map this firmware owns.
 *
 * HAL drivers (GPIO/RCC/UART/TIM/...) live under
 * vendor/mm32f0140/Device/MM32F0140/HAL_Lib/.  hal_conf.h drags in every
 * peripheral header; we link only the .c files we actually call (the
 * top-level CMakeLists enumerates them).
 *
 * Scope note: the LCD, rotary encoder and LED ring are no longer driven from
 * this MCU — the panel FFC moves from CON1 to x3_uart_board and those signals
 * go to the ESP32-S3. Their pins are left untouched. See docs/hardware.md §3. */
#ifndef X3_BOARD_H
#define X3_BOARD_H

#include "hal_conf.h"

/* SystemCoreClock is defined by vendor's system_mm32f0140.c but never
 * publicly declared. Expose it here so application code can derive
 * delays / baud-rates from it. */
extern u32 SystemCoreClock;

/* ----- Buttons K1~K4 (active-low, 10k pull-up on the board) --------------
 * Pressing pulls the pin LOW. K4 has no decoupling cap (K1~K3 do), so its
 * chatter can run longer.
 *
 * ⚠️ K4 currently RESETS the MCU — its net reaches NRST. Unusable as an input
 * until the board is cut and rerouted. See docs/hardware.md §2.1.1. */
#define BTN_K1_PORT     GPIOC
#define BTN_K1_PIN      GPIO_Pin_13
#define BTN_K2_PORT     GPIOC
#define BTN_K2_PIN      GPIO_Pin_14
#define BTN_K3_PORT     GPIOC
#define BTN_K3_PIN      GPIO_Pin_15
#define BTN_K4_PORT     GPIOD
#define BTN_K4_PIN      GPIO_Pin_0

/* ----- Button LEDs K1~K4 (bi-color, anti-phase) --------------------------
 * HIGH -> NPN on  -> white.  LOW -> MCU sinks the orange cathode -> orange.
 * There is no per-channel off: hi-Z leaves a DC path from the rail through
 * the orange LED into the NPN base and the white LED lights (measured
 * 2026-09-22). Use LED_EN for darkness. */
#define LED_K1_PORT     GPIOA
#define LED_K1_PIN      GPIO_Pin_12
#define LED_K2_PORT     GPIOD
#define LED_K2_PIN      GPIO_Pin_2
#define LED_K3_PORT     GPIOB
#define LED_K3_PIN      GPIO_Pin_6
#define LED_K4_PORT     GPIOB
#define LED_K4_PIN      GPIO_Pin_7

/* ----- LED rail master enable + brightness -------------------------------
 * PB9 = QP1 (AO3401A P-MOSFET) gate via RD9. LOW = rail ON.
 * PB9 is also TIM17_CH1 on AF2, so the rail doubles as a 1 kHz PWM dimmer
 * for all four channels at once (verified 2026-09-22, 1~100 % usable).
 * Drive it with TIM_OCPolarity_Low so the timer hides the active-low gate. */
#define LED_EN_PORT     GPIOB
#define LED_EN_PIN      GPIO_Pin_9
#define LED_EN_PINSRC   GPIO_PinSource9
#define LED_EN_AF       GPIO_AF_2
#define LED_EN_TIM      TIM17
#define LED_EN_TIM_CLK  RCC_APB2ENR_TIM17

/* ----- Relay drivers (NPN low-side, active-HIGH) -------------------------
 * MCU HIGH -> NPN on -> coil energised -> contact closed.
 * L3 is only populated on the back PCB of the 3-lamp room; the front-side
 * driver exists on every unit. L4 has no relay and no terminal on the back
 * PCB either, but its driver (Q8) is populated, so it is exposed anyway —
 * driving it just switches an unloaded transistor, and the channel is there
 * if a use turns up. */
#define RELAY_L1_PORT   GPIOA
#define RELAY_L1_PIN    GPIO_Pin_8
#define RELAY_L2_PORT   GPIOB
#define RELAY_L2_PIN    GPIO_Pin_8
#define RELAY_L3_PORT   GPIOA
#define RELAY_L3_PIN    GPIO_Pin_10
#define RELAY_L4_PORT   GPIOA
#define RELAY_L4_PIN    GPIO_Pin_11

/* ----- Debug / module UART (UART2, PA2 = TX, PA3 = RX) -------------------
 * MM32F0140 maps UART2 onto PA2/PA3 via GPIO_AF_1. Mode bits (CRL) and
 * function select (AFRL) are separate registers here — both must be
 * programmed, or the pad stays on its default function and TX is silent.
 *
 * Runs to the ESP32-S3 main-board through x3_uart_board in the ZS3L socket:
 * PA2 -> R6 120R -> header A-2 -> R2 330R -> CN2-12, and PA3 <- R10 120R
 * <- header A-1 <- R1 330R <- CN2-4. */
#define DBG_UART            UART2
#define DBG_UART_CLK        RCC_APB1ENR_UART2
#define DBG_UART_PORT       GPIOA
#define DBG_UART_TX_PIN     GPIO_Pin_2
#define DBG_UART_TX_SOURCE  GPIO_PinSource2
#define DBG_UART_RX_PIN     GPIO_Pin_3
#define DBG_UART_RX_SOURCE  GPIO_PinSource3
#define DBG_UART_AF         GPIO_AF_1

#endif /* X3_BOARD_H */
