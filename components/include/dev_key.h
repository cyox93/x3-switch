/* Buttons K1~K4 — polled scan with a software debounce and an event queue.
 *
 * Wiring: +3V3 - 10k - pin - tact switch - GND, so a press reads LOW.
 * K4 is the only channel without a decoupling cap, so its chatter can run
 * longer; the debounce window is sized for it and shared by all four.
 *
 * !! K4 currently resets the MCU — its net reaches NRST, so it can never be
 * observed as a press. The channel stays in the API because the protocol
 * has four of them and the board is going to be repaired.
 * See docs/hardware.md §2.1.1.
 *
 * This layer reports raw DOWN/UP only. Short/long/double-click belong to the
 * ESP32-S3 so that a policy change never touches this firmware
 * (docs/at-protocol.md §8). */
#ifndef X3_DEV_KEY_H
#define X3_DEV_KEY_H

#include <stdbool.h>
#include <stdint.h>

#define DEV_KEY_COUNT  4U

typedef struct {
    uint8_t channel;   /* 1~4 */
    bool    down;
} dev_key_event_t;

void dev_key_init(void);

/* Sample the pins and debounce. Call it every main-loop pass; it derives its
 * own timing from tick_ms(), so the loop period only has to be well under
 * the debounce window. */
void dev_key_poll(void);

/* Oldest pending transition, FIFO. False when the queue is empty. */
bool dev_key_event_pop(dev_key_event_t *out);

/* Debounced level right now (what AT+KEY? reports). ch is 1-based. */
bool dev_key_is_down(uint8_t ch);

#endif /* X3_DEV_KEY_H */
