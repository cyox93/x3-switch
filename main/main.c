/* x3-switch — I/O sub-controller for the X3 Thermostat main-board.
 *
 * This MCU owns buttons K1~K4, the bi-color button LEDs with their shared
 * rail, and relays L1~L4, and exposes all of it over UART2 as an AT command
 * slave. It holds no policy: it reports key transitions and executes what
 * the ESP32-S3 asks for. See docs/architecture.md and docs/at-protocol.md.
 *
 * Everything is cooperatively polled from one loop. There are no interrupts
 * beyond SysTick, so no driver state needs guarding. */
#include "at.h"
#include "board.h"
#include "clock.h"
#include "device.h"
#include "log.h"
#include "tick.h"
#include "version.h"

static const char *TAG = "main";

int main(void)
{
    /* Order matters: the UART divisor and the SysTick reload are both
     * derived from the clock tree, so the PLL has to be up first. */
    clock_init_pll48mhz();
    tick_init();
    log_init();

    device_init();
    at_init();

    RTT_LOGI(TAG, "%s %s — HCLK %u Hz, PCLK1 %u Hz",
             X3_FW_NAME, X3_FW_VERSION,
             (unsigned)SystemCoreClock, (unsigned)dev_uart_pclk_hz());

    /* Outputs are already in their safe boot state (relays off, LEDs orange,
     * rail on at full brightness), so it is honest to announce readiness. */
    at_notify_ready();

    for (;;) {
        at_poll();

        dev_key_poll();
        dev_key_event_t event;
        while (dev_key_event_pop(&event)) {
            at_notify_key(event.channel, event.down);
        }

        dev_led_tick();
    }
}
