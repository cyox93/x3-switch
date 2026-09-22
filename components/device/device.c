#include "device.h"

void device_init(void)
{
    /* Relays first: nothing else matters if a mains contact is left closed
     * by whatever the previous run or the reset latch left behind. */
    dev_relay_init();

    dev_uart_init();
    dev_led_init();
    dev_key_init();
}
