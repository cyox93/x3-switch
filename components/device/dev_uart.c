#include "dev_uart.h"
#include "board.h"

/* One byte at 115200 takes ~87 us; at 48 MHz that is ~4000 loop iterations.
 * 100k gives two orders of magnitude of headroom while still bounding the
 * wait if the peripheral clock never came up. */
#define UART_TX_SPIN_LIMIT  100000U

static uint32_t s_pclk_hz;

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
    if (UART_GetFlagStatus(DBG_UART, UART_CSR_RXAVL) == RESET) {
        return false;
    }
    *out = (uint8_t)UART_ReceiveData(DBG_UART);
    return true;
}

uint32_t dev_uart_pclk_hz(void)
{
    return s_pclk_hz;
}
