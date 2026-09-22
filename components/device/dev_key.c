#include "dev_key.h"
#include "board.h"
#include "tick.h"
#include "log.h"

static const char *TAG = "key";

/* K4 has no decoupling cap (K1~K3 have C24/C8/C26), so it is the channel
 * that sets this number. 25 ms is far longer than tact-switch chatter and
 * still imperceptible to a finger. */
#define DEBOUNCE_MS   25U

/* A human cannot generate more than a couple of transitions between two
 * main-loop passes; 8 only has to cover a burst that arrives while a long
 * AT response is blocking the TX path. */
#define EVENT_QUEUE_LEN  8U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} key_pin_t;

static const key_pin_t s_pins[DEV_KEY_COUNT] = {
    { BTN_K1_PORT, BTN_K1_PIN },
    { BTN_K2_PORT, BTN_K2_PIN },
    { BTN_K3_PORT, BTN_K3_PIN },
    { BTN_K4_PORT, BTN_K4_PIN },
};

static bool     s_stable[DEV_KEY_COUNT];    /* debounced level */
static bool     s_last_raw[DEV_KEY_COUNT];  /* level at the last sample */
static uint32_t s_changed_at[DEV_KEY_COUNT];

static dev_key_event_t s_queue[EVENT_QUEUE_LEN];
static uint8_t s_head;   /* pop position */
static uint8_t s_count;

static void queue_push(uint8_t channel, bool down)
{
    if (s_count >= EVENT_QUEUE_LEN) {
        /* Dropping the oldest keeps the most recent state transitions, which
         * are the ones the ESP32 still needs to act on. */
        s_head = (uint8_t)((s_head + 1U) % EVENT_QUEUE_LEN);
        --s_count;
        RTT_LOGW(TAG, "event queue full, dropped oldest");
    }
    const uint8_t tail = (uint8_t)((s_head + s_count) % EVENT_QUEUE_LEN);
    s_queue[tail].channel = channel;
    s_queue[tail].down    = down;
    ++s_count;
}

void dev_key_init(void)
{
    RCC_GPIO_ClockCmd(GPIOC, ENABLE);
    RCC_GPIO_ClockCmd(GPIOD, ENABLE);

    const uint32_t now = tick_ms();

    for (uint8_t i = 0; i < DEV_KEY_COUNT; ++i) {
        /* The board already pulls each line up with 10k; the internal pull-up
         * on top of it only costs leakage and keeps the pin defined if a
         * resistor is ever missing. */
        GPIO_InitTypeDef init = {
            .GPIO_Pin   = s_pins[i].pin,
            .GPIO_Speed = GPIO_Speed_10MHz,
            .GPIO_Mode  = GPIO_Mode_IPU,
        };
        GPIO_Init(s_pins[i].port, &init);

        /* Seed from the actual pin so a key held down through boot does not
         * produce a spurious DOWN once the debounce window expires. */
        const bool down = (GPIO_ReadInputDataBit(s_pins[i].port, s_pins[i].pin) == Bit_RESET);
        s_stable[i]     = down;
        s_last_raw[i]   = down;
        s_changed_at[i] = now;
    }

    s_head  = 0;
    s_count = 0;
}

void dev_key_poll(void)
{
    const uint32_t now = tick_ms();

    for (uint8_t i = 0; i < DEV_KEY_COUNT; ++i) {
        const bool raw = (GPIO_ReadInputDataBit(s_pins[i].port, s_pins[i].pin) == Bit_RESET);

        if (raw != s_last_raw[i]) {
            /* Still bouncing — restart the window. */
            s_last_raw[i]   = raw;
            s_changed_at[i] = now;
            continue;
        }

        if (raw != s_stable[i] && tick_elapsed_ms(s_changed_at[i]) >= DEBOUNCE_MS) {
            s_stable[i] = raw;
            queue_push((uint8_t)(i + 1U), raw);
            RTT_LOGD(TAG, "K%u %s", (unsigned)(i + 1U), raw ? "DOWN" : "UP");
        }
    }
}

bool dev_key_event_pop(dev_key_event_t *out)
{
    if (s_count == 0U) {
        return false;
    }
    *out = s_queue[s_head];
    s_head = (uint8_t)((s_head + 1U) % EVENT_QUEUE_LEN);
    --s_count;
    return true;
}

bool dev_key_is_down(uint8_t ch)
{
    if (ch < 1U || ch > DEV_KEY_COUNT) {
        return false;
    }
    return s_stable[ch - 1U];
}
