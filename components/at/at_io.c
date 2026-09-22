#include "at.h"
#include "at_io.h"
#include "at_cmd.h"
#include "dev_uart.h"
#include "log.h"
#include "version.h"

#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "at";

/* docs/at-protocol.md §2: a line over 64 bytes is dropped with one ERROR.
 * The longest legal command, AT+LEDIVL=4,60000, is 18. */
#define AT_LINE_MAX  64U

/* Longest line the firmware emits is +LEDIVL with four 5-digit values (31);
 * 96 leaves room for a format that grows without silent truncation. */
#define AT_OUT_MAX   96U

static char    s_line[AT_LINE_MAX + 1U];
static uint8_t s_len;
static bool    s_overflow;

void at_send_line(const char *text)
{
    dev_uart_write(text);
    dev_uart_write("\r\n");
}

void at_sendf(const char *fmt, ...)
{
    char buf[AT_OUT_MAX];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    at_send_line(buf);
}

void at_reply_ok(void)
{
    at_send_line("OK");
}

void at_reply_error(void)
{
    at_send_line("ERROR");
}

void at_notify_ready(void)
{
    at_sendf("+READY:%s,%s", X3_FW_NAME, X3_FW_VERSION);
}

void at_notify_key(uint8_t channel, bool down)
{
    at_sendf("+KEY:%u,%s", (unsigned)channel, down ? "DOWN" : "UP");
}

void at_init(void)
{
    s_len      = 0;
    s_overflow = false;
}

static bool is_space(char c)
{
    return (c == ' ') || (c == '\t');
}

/* Strip surrounding whitespace in place. Whitespace *inside* a command is
 * not tolerated — that is the protocol's rule, not an oversight. */
static char *trim(char *s)
{
    while (is_space(*s)) {
        ++s;
    }
    char *end = s;
    while (*end != '\0') {
        ++end;
    }
    while (end > s && is_space(end[-1])) {
        --end;
    }
    *end = '\0';
    return s;
}

static void line_complete(void)
{
    if (s_overflow) {
        /* One ERROR for the whole oversized line, however long it ran. */
        RTT_LOGW(TAG, "line over %u bytes, dropped", (unsigned)AT_LINE_MAX);
        at_reply_error();
        s_overflow = false;
        s_len      = 0;
        return;
    }

    s_line[s_len] = '\0';
    s_len = 0;

    char *cmd = trim(s_line);
    if (*cmd == '\0') {
        return;   /* blank line, including the \n of a \r\n pair */
    }

    RTT_LOGD(TAG, "< %s", cmd);
    at_cmd_dispatch(cmd);
}

void at_feed_byte(uint8_t byte)
{
    if (byte == '\r' || byte == '\n') {
        line_complete();
        return;
    }

    if (s_overflow) {
        return;   /* swallow the rest of the offending line */
    }

    if (s_len >= AT_LINE_MAX) {
        s_overflow = true;
        return;
    }

    s_line[s_len++] = (char)byte;
}

void at_poll(void)
{
    uint8_t byte;

    while (dev_uart_read_byte(&byte)) {
        at_feed_byte(byte);
    }
}
