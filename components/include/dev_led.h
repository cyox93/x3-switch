/* Button LEDs K1~K4 (bi-color, anti-phase) plus the shared rail.
 *
 * Per channel (PA12 / PD2 / PB6 / PB7):
 *   GPIO HIGH -> NPN on  -> white
 *   GPIO LOW  -> the MCU sinks the orange cathode -> orange
 * There is no per-channel off. A hi-Z pin still leaves a DC path from the
 * rail through the orange LED and its 120R into the 4k7 base resistor, so
 * the NPN turns on and the white LED lights (measured 2026-09-22).
 * Darkness comes from the rail only.
 *
 * Rail (PB9 = QP1 P-MOSFET gate, LOW = on) is TIM17_CH1 on AF2, driven as a
 * 1 kHz PWM with TIM_OCPolarity_Low so the timer hides the active-low gate.
 *
 * Enable and brightness are separate firmware variables and CCR1 is only
 * their product: turning the rail off keeps the brightness setting, so the
 * ESP32-S3 never has to remember and re-send it.
 * See docs/at-protocol.md §3.3 ~ §3.6. */
#ifndef X3_DEV_LED_H
#define X3_DEV_LED_H

#include <stdbool.h>
#include <stdint.h>

#define DEV_LED_COUNT                 4U

#define DEV_LED_INTERVAL_MIN_MS       100U
#define DEV_LED_INTERVAL_MAX_MS       60000U
#define DEV_LED_INTERVAL_DEFAULT_MS   1000U

#define DEV_LED_BRIGHTNESS_MAX        100U

typedef enum {
    DEV_LED_WHITE = 0,
    DEV_LED_ORANGE,
    DEV_LED_TOGGLE,
} dev_led_mode_t;

/* Boot state: every channel orange, rail on, brightness 100 — the same look
 * as the stock Tuya firmware, so the panel is never dark while the ESP32-S3
 * takes its ~1 s to come up (docs/at-protocol.md §5). */
void dev_led_init(void);

/* Advance the blink phase of every DEV_LED_TOGGLE channel. Call it every
 * main-loop pass; timing comes from tick_ms(). */
void dev_led_tick(void);

/* ch is 1-based. Setting DEV_LED_TOGGLE always restarts the channel at white
 * and resets its phase, so issuing it back-to-back on several channels makes
 * them blink together. False means the channel was out of range. */
bool dev_led_mode_set(uint8_t ch, dev_led_mode_t mode);
dev_led_mode_t dev_led_mode_get(uint8_t ch);

/* Time one colour is held; a full white->orange->white cycle is twice this.
 * False for an out-of-range channel or interval, leaving the old value. */
bool dev_led_interval_set(uint8_t ch, uint32_t ms);
uint32_t dev_led_interval_get(uint8_t ch);

/* Rail master enable. Channel colours and blink phases keep running while
 * the rail is off. */
void dev_led_enable_set(bool on);
bool dev_led_enable_get(void);

/* 0~100 %, linear duty. Stored even while the rail is off. False leaves the
 * old value. */
bool dev_led_brightness_set(uint32_t percent);
uint32_t dev_led_brightness_get(void);

#endif /* X3_DEV_LED_H */
