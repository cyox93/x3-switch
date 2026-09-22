#include "dev_uart.h"
#include "board.h"

/* One byte at 115200 takes ~87 us; at 48 MHz that is ~4000 loop iterations.
 * 100k gives two orders of magnitude of headroom while still bounding the
 * wait if the peripheral clock never came up. */
#define UART_TX_SPIN_LIMIT  100000U

/* Power of two so the wrap is a mask. 128 covers ~11 ms of continuous input,
 * far more than the longest response (AT+STATUS?, ~75 bytes = 6.5 ms) can
 * block the loop for. */
#define UART_RX_RING_LEN    128U
#define UART_RX_RING_MASK   (UART_RX_RING_LEN - 1U)

static uint32_t s_pclk_hz;

/* Single producer (the ISR) and single consumer (the main loop), so the two
 * indices need no critical section on Cortex-M0: each side writes only its
 * own index, and a 16-bit aligned store is atomic. */
static volatile uint8_t  s_rx_ring[UART_RX_RING_LEN];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
/* Should stay 0. Nothing reads it at runtime — it exists to be sampled over
 * SWD (arm-none-eabi-nm gives the address) when a command goes missing, to
 * tell a lost byte apart from a malformed one. */
static volatile uint32_t s_rx_dropped;

void UART2_IRQHandler(void)
{
    /* An overrun means the peripheral lost a byte before this ran. Count it
     * while the flag is still standing, then clear it with the RX flag. */
    if (UART_GetITStatus(DBG_UART, UART_ISR_RXOERR) == SET) {
        ++s_rx_dropped;
    }

    /* Clear the latched flags first and drain on the CSR.RXAVL *level*: a
     * byte that lands during the drain is picked up by the loop, and one
     * that lands after it re-latches ISR.RX and re-enters the handler.
     * Clearing last would swallow that edge and stall the receiver. */
    UART_ClearITPendingBit(DBG_UART, UART_ISR_RX | UART_ISR_RXOERR);

    while (UART_GetFlagStatus(DBG_UART, UART_CSR_RXAVL) == SET) {
        /* RDR is read even when the ring is full, because leaving the byte
         * there would keep RXAVL — and the interrupt — asserted forever. */
        const uint8_t byte = (uint8_t)UART_ReceiveData(DBG_UART);
        const uint16_t next = (uint16_t)((s_rx_head + 1U) & UART_RX_RING_MASK);

        if (next == s_rx_tail) {
            ++s_rx_dropped;
        } else {
            s_rx_ring[s_rx_head] = byte;
            s_rx_head = next;
        }
    }
}

void dev_uart_init(void)
{
    RCC_GPIO_ClockCmd(DBG_UART_PORT, ENABLE);
    RCC_APB1PeriphClockCmd(DBG_UART_CLK, ENABLE);

    /* AFRL select is independent of the CRL mode bits on MM32F0140: setting
     * GPIO_Mode_AF_PP alone leaves the pad on its default function and TX
     * stays silent (same trap as the SPI TXEN quirk). */
    GPIO_PinAFConfig(DBG_UART_PORT, DBG_UART_TX_SOURCE, DBG_UART_AF);
    GPIO_PinAFConfig(DBG_UART_PORT, DBG_UART_RX_SOURCE, DBG_UART_AF);

    GPIO_InitTypeDef tx = {
        .GPIO_Pin   = DBG_UART_TX_PIN,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_Mode  = GPIO_Mode_AF_PP,
    };
    GPIO_Init(DBG_UART_PORT, &tx);

    /* Pull-up so an unconnected RX idles at mark instead of framing noise. */
    GPIO_InitTypeDef rx = {
        .GPIO_Pin   = DBG_UART_RX_PIN,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_Mode  = GPIO_Mode_IPU,
    };
    GPIO_Init(DBG_UART_PORT, &rx);

    UART_InitTypeDef cfg;
    UART_StructInit(&cfg);
    cfg.BaudRate      = DEV_UART_BAUDRATE;
    cfg.WordLength    = UART_WordLength_8b;
    cfg.StopBits      = UART_StopBits_1;
    cfg.Parity        = UART_Parity_No;
    cfg.HWFlowControl = UART_HWFlowControl_None;
    cfg.Mode          = UART_Mode_Tx | UART_Mode_Rx;
    UART_Init(DBG_UART, &cfg);

    /* UART_Init read this internally; keep a copy so callers can verify the
     * clock tree actually was at 48 MHz when the divisor got fixed. */
    s_pclk_hz = RCC_GetPCLK1Freq();

    s_rx_head    = 0;
    s_rx_tail    = 0;
    s_rx_dropped = 0;

    UART_ClearITPendingBit(DBG_UART, UART_ISR_RX | UART_ISR_RXOERR);
    /* Receive only. The overrun flag is read inside the handler instead of
     * being given its own interrupt, so there is no second source that could
     * storm if its flag did not clear the way the header suggests. */
    UART_ITConfig(DBG_UART, UART_IER_RX, ENABLE);
    NVIC_EnableIRQ(UART2_IRQn);

    UART_Cmd(DBG_UART, ENABLE);
}

static bool uart_tx_byte(uint8_t byte)
{
    uint32_t spin = UART_TX_SPIN_LIMIT;
    while (UART_GetFlagStatus(DBG_UART, UART_CSR_TXFULL) == SET) {
        if (--spin == 0U) {
            return false;
        }
    }

    UART_SendData(DBG_UART, byte);

    spin = UART_TX_SPIN_LIMIT;
    while (UART_GetFlagStatus(DBG_UART, UART_CSR_TXEPT) == RESET) {
        if (--spin == 0U) {
            return false;
        }
    }
    return true;
}

void dev_uart_write_bytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (!uart_tx_byte(data[i])) {
            return;
        }
    }
}

void dev_uart_write(const char *str)
{
    while (*str != '\0') {
        if (!uart_tx_byte((uint8_t)*str++)) {
            return;
        }
    }
}

bool dev_uart_read_byte(uint8_t *out)
{
    if (s_rx_tail == s_rx_head) {
        return false;
    }
    *out = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) & UART_RX_RING_MASK);
    return true;
}


uint32_t dev_uart_pclk_hz(void)
{
    return s_pclk_hz;
}
