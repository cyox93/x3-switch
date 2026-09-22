#include "dev_relay.h"
#include "board.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} relay_pin_t;

static const relay_pin_t s_pins[DEV_RELAY_COUNT] = {
    { RELAY_L1_PORT, RELAY_L1_PIN },
    { RELAY_L2_PORT, RELAY_L2_PIN },
    { RELAY_L3_PORT, RELAY_L3_PIN },
    { RELAY_L4_PORT, RELAY_L4_PIN },
};

static bool s_state[DEV_RELAY_COUNT];

void dev_relay_init(void)
{
    RCC_GPIO_ClockCmd(GPIOA, ENABLE);
    RCC_GPIO_ClockCmd(GPIOB, ENABLE);

    for (uint8_t i = 0; i < DEV_RELAY_COUNT; ++i) {
        /* Park the pin LOW before switching it to an output, so the coil is
         * never pulsed by whatever the reset-state latch happened to hold. */
        GPIO_ResetBits(s_pins[i].port, s_pins[i].pin);

        GPIO_InitTypeDef init = {
            .GPIO_Pin   = s_pins[i].pin,
            .GPIO_Speed = GPIO_Speed_10MHz,
            .GPIO_Mode  = GPIO_Mode_Out_PP,
        };
        GPIO_Init(s_pins[i].port, &init);

        s_state[i] = false;
    }
}

bool dev_relay_set(uint8_t ch, bool on)
{
    if (ch < 1U || ch > DEV_RELAY_COUNT) {
        return false;
    }
    const relay_pin_t *p = &s_pins[ch - 1U];
    GPIO_WriteBit(p->port, p->pin, on ? Bit_SET : Bit_RESET);
    s_state[ch - 1U] = on;
    return true;
}

bool dev_relay_get(uint8_t ch)
{
    if (ch < 1U || ch > DEV_RELAY_COUNT) {
        return false;
    }
    return s_state[ch - 1U];
}
