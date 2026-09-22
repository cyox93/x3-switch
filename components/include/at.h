/* AT command interface on UART2 — the only external interface this firmware
 * exposes. The contract is docs/at-protocol.md; that document is normative,
 * this code implements it.
 *
 * The MCU is a complete slave: it executes what the ESP32-S3 asks and
 * reports key transitions. It holds no policy. */
#ifndef X3_AT_H
#define X3_AT_H

#include <stdbool.h>
#include <stdint.h>

void at_init(void);

/* Drain the UART, assemble complete lines and execute them. Call it every
 * main-loop pass. Replies are written synchronously from inside this call. */
void at_poll(void);

/* One received byte into the line assembler; at_poll() is just this applied
 * to everything UART2 has buffered. Kept separate so the framing rules can
 * be driven from another source than the UART. */
void at_feed_byte(uint8_t byte);

/* Unsolicited notifications. They can land between a command and its OK,
 * which the protocol explicitly allows. */
void at_notify_ready(void);
void at_notify_key(uint8_t channel, bool down);

#endif /* X3_AT_H */
