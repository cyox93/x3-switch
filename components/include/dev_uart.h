/* X3 debug / module UART — UART2 on PA2 (TX) / PA3 (RX), 8N1.
 *
 * Pin map and the AFRL-vs-CRL caveat live in board.h (DBG_UART_*).
 * The baud divisor is derived from PCLK1 at init time, so this driver must
 * be initialised *after* clock_init_pll48mhz() — otherwise it latches the
 * 8 MHz HSI-reset divisor and every byte comes out at the wrong rate.
 *
 * Polling only: no interrupts, no ring buffer. Enough for boot banners and
 * for poking the ZS3L module by hand; revisit when a real Zigbee transport
 * needs framing. */
#ifndef X3_DEV_UART_H
#define X3_DEV_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEV_UART_BAUDRATE   115200U

void dev_uart_init(void);

/* Blocking transmit. Both bail out rather than spin forever if the TX path
 * never drains — a mis-clocked UART must not wedge boot. */
void dev_uart_write_bytes(const uint8_t *data, size_t len);
void dev_uart_write(const char *str);

/* Non-blocking receive: false when the RX register holds nothing. */
bool dev_uart_read_byte(uint8_t *out);

/* PCLK1 (Hz) the divisor was computed from — log it to confirm the 115200
 * fit is what you think it is. */
uint32_t dev_uart_pclk_hz(void);

#endif /* X3_DEV_UART_H */
