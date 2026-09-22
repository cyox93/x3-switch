/* Top-level device-layer aggregate header. Application code includes this
 * single header and gets every dev_*.h driver currently linked into the
 * device component. */
#ifndef X3_DEVICE_H
#define X3_DEVICE_H

#include "dev_key.h"
#include "dev_led.h"
#include "dev_relay.h"
#include "dev_uart.h"

/* Bring up every device peripheral in one call. */
void device_init(void);

#endif /* X3_DEVICE_H */
