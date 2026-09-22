/* Relay drivers L1~L4 (NPN low-side, active-HIGH).
 *
 * MCU HIGH -> NPN on -> coil energised -> contact closed.
 *
 * All four channels are exposed because the front PCB populates every
 * driver; only the back PCB differs per room (L1~L3 in the large room,
 * L1~L2 elsewhere, L4 never). Driving an unpopulated channel just switches
 * an unloaded transistor — the MCU cannot tell which are fitted, so knowing
 * that is the ESP32-S3's job. See docs/at-protocol.md §3.2. */
#ifndef X3_DEV_RELAY_H
#define X3_DEV_RELAY_H

#include <stdbool.h>
#include <stdint.h>

#define DEV_RELAY_COUNT  4U

/* All channels off. Boot must reach this state before anything else can
 * energise a mains contact. */
void dev_relay_init(void);

/* ch is 1-based (1~4). Returns false for an out-of-range channel. */
bool dev_relay_set(uint8_t ch, bool on);
bool dev_relay_get(uint8_t ch);

#endif /* X3_DEV_RELAY_H */
