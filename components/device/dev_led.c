#include "dev_led.h"
#include "board.h"
#include "tick.h"

/* 1 kHz: no visible flicker, and slow enough against the QP1 gate RC
 * (AO3401A Ciss ~730 pF in series with RD9) that the edges stay clean.
 * Counting at 1 MHz gives 1000 steps per period, so a 1 % duty step is an
 * exact CCR1 increment of 10 and no rounding is lost. */
#define PWM_HZ        1000U
#define PWM_TICK_HZ   1000000U
#define PWM_PERIOD    (PWM_TICK_HZ / PWM_HZ)

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} led_pin_t;

static const led_pin_t s_pins[DEV_LED_COUNT] = {
    { LED_K1_PORT, LED_K1_PIN },
    { LED_K2_PORT, LED_K2_PIN },
    { LED_K3_PORT, LED_K3_PIN },
    { LED_K4_PORT, LED_K4_PIN },
};

static dev_led_mode_t s_mode[DEV_LED_COUNT];
static uint32_t       s_interval[DEV_LED_COUNT];
static bool           s_white[DEV_LED_COUNT];      /* colour on the pin right now */
static uint32_t       s_phase_at[DEV_LED_COUNT];   /* when that colour started */

/* AT+LEDEN and AT+LEDBRI own these; CCR1 is only ever their product. Keeping
 * the state here and not in the register is what makes brightness survive an
 * off/on cycle. */
static bool     s_enabled;
static uint32_t s_brightness;

static void channel_write(uint8_t idx, bool white)
{
    s_white[idx] = white;
    GPIO_WriteBit(s_pins[idx].port, s_pins[idx].pin, white ? Bit_SET : Bit_RESET);
}

static void rail_apply(void)
{
    /* CCR1 = 0 with PWM1 + active-low leaves the output permanently inactive,
     * i.e. actively driven HIGH, so QP1 is held firmly closed. Using BDTR.MOE
     * instead would hand the pin state to OSSI and could float the gate. */
    const uint32_t duty = s_enabled ? (PWM_PERIOD * s_brightness) / 100U : 0U;
    TIM_SetCompare1(LED_EN_TIM, duty);
}

static void rail_pwm_init(void)
{
    RCC_APB2PeriphClockCmd(LED_EN_TIM_CLK, ENABLE);

    TIM_TimeBaseInitTypeDef tb;
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler     = (uint16_t)((SystemCoreClock / PWM_TICK_HZ) - 1U);
    tb.TIM_CounterMode   = TIM_CounterMode_Up;
    tb.TIM_Period        = PWM_PERIOD - 1U;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(LED_EN_TIM, &tb);

    TIM_OCInitTypeDef oc;
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = 0;
    oc.TIM_OCPolarity  = TIM_OCPolarity_Low;    /* LED_EN is active-low */
    oc.TIM_OCIdleState = TIM_OCIdleState_Set;   /* idle HIGH = rail off */
    TIM_OC1Init(LED_EN_TIM, &oc);
    TIM_OC1PreloadConfig(LED_EN_TIM, TIM_OCPreload_Enable);

    /* Mode bits and function select are separate registers here: without
     * GPIO_PinAFConfig the pad stays a plain GPIO and the timer drives
     * nothing. */
    GPIO_PinAFConfig(LED_EN_PORT, LED_EN_PINSRC, LED_EN_AF);
    GPIO_InitTypeDef en = {
        .GPIO_Pin   = LED_EN_PIN,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_Mode  = GPIO_Mode_AF_PP,
    };
    GPIO_Init(LED_EN_PORT, &en);

    TIM_Cmd(LED_EN_TIM, ENABLE);
    TIM_CtrlPWMOutputs(LED_EN_TIM, ENABLE);     /* TIM16/17 gate on MOE */
}

void dev_led_init(void)
{
    RCC_GPIO_ClockCmd(GPIOA, ENABLE);
    RCC_GPIO_ClockCmd(GPIOB, ENABLE);
    RCC_GPIO_ClockCmd(GPIOD, ENABLE);

    const uint32_t now = tick_ms();

    for (uint8_t i = 0; i < DEV_LED_COUNT; ++i) {
        GPIO_InitTypeDef init = {
            .GPIO_Pin   = s_pins[i].pin,
            .GPIO_Speed = GPIO_Speed_10MHz,
            .GPIO_Mode  = GPIO_Mode_Out_PP,
        };
        GPIO_Init(s_pins[i].port, &init);

        s_mode[i]     = DEV_LED_ORANGE;
        s_interval[i] = DEV_LED_INTERVAL_DEFAULT_MS;
        s_phase_at[i] = now;
        channel_write(i, false);
    }

    s_enabled    = true;
    s_brightness = DEV_LED_BRIGHTNESS_MAX;

    rail_pwm_init();
    rail_apply();
}

void dev_led_tick(void)
{
    for (uint8_t i = 0; i < DEV_LED_COUNT; ++i) {
        if (s_mode[i] != DEV_LED_TOGGLE) {
            continue;
        }
        if (tick_elapsed_ms(s_phase_at[i]) >= s_interval[i]) {
            s_phase_at[i] = tick_ms();
            channel_write(i, !s_white[i]);
        }
    }
}

bool dev_led_mode_set(uint8_t ch, dev_led_mode_t mode)
{
    if (ch < 1U || ch > DEV_LED_COUNT) {
        return false;
    }
    const uint8_t i = (uint8_t)(ch - 1U);

    s_mode[i] = mode;
    switch (mode) {
    case DEV_LED_WHITE:
        channel_write(i, true);
        break;
    case DEV_LED_ORANGE:
        channel_write(i, false);
        break;
    case DEV_LED_TOGGLE:
    default:
        /* Always restart, even if the channel was already toggling: that is
         * what lets several AT+LED=<ch>,T in a row line the channels up. */
        s_mode[i]     = DEV_LED_TOGGLE;
        s_phase_at[i] = tick_ms();
        channel_write(i, true);
        break;
    }
    return true;
}

dev_led_mode_t dev_led_mode_get(uint8_t ch)
{
    if (ch < 1U || ch > DEV_LED_COUNT) {
        return DEV_LED_ORANGE;
    }
    return s_mode[ch - 1U];
}

bool dev_led_interval_set(uint8_t ch, uint32_t ms)
{
    if (ch < 1U || ch > DEV_LED_COUNT) {
        return false;
    }
    if (ms < DEV_LED_INTERVAL_MIN_MS || ms > DEV_LED_INTERVAL_MAX_MS) {
        return false;
    }
    /* Deliberately does not touch s_phase_at: the new interval takes effect
     * at the next transition, and a colour already held longer than the new
     * interval flips on the very next tick. */
    s_interval[ch - 1U] = ms;
    return true;
}

uint32_t dev_led_interval_get(uint8_t ch)
{
    if (ch < 1U || ch > DEV_LED_COUNT) {
        return 0;
    }
    return s_interval[ch - 1U];
}

void dev_led_enable_set(bool on)
{
    s_enabled = on;
    rail_apply();
}

bool dev_led_enable_get(void)
{
    return s_enabled;
}

bool dev_led_brightness_set(uint32_t percent)
{
    if (percent > DEV_LED_BRIGHTNESS_MAX) {
        return false;
    }
    s_brightness = percent;
    rail_apply();
    return true;
}

uint32_t dev_led_brightness_get(void)
{
    return s_brightness;
}
