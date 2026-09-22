#include "at_cmd.h"
#include "at_io.h"
#include "board.h"
#include "device.h"
#include "log.h"
#include "version.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const char *TAG = "at";

/* Handlers get the argument text (mutable, possibly empty) and return false
 * to produce ERROR. Every failure is the same ERROR on the wire — the reason
 * goes to RTT instead, because a four-channel device gives the ESP32 nothing
 * useful to branch on (docs/at-protocol.md §2). */
typedef bool (*at_handler_t)(char *args);

typedef struct {
    const char  *name;    /* text between '+' and the '=' / '?' / end */
    at_handler_t exec;    /* AT+NAME       */
    at_handler_t set;     /* AT+NAME=args  */
    at_handler_t query;   /* AT+NAME?      */
} at_cmd_t;

/* ---------- small parsers ---------- */

static char upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* `text` is compared against an already-uppercase table entry. */
static bool ci_equal(const char *text, size_t len, const char *upper_ref)
{
    for (size_t i = 0; i < len; ++i) {
        if (upper(text[i]) != upper_ref[i]) {
            return false;
        }
    }
    return upper_ref[len] == '\0';
}

/* Strict: digits only, at least one, no sign and no whitespace. The cap is
 * far above every accepted range and only exists to stop the accumulator
 * wrapping on a long digit string. */
static bool parse_uint(const char *s, uint32_t *out)
{
    if (*s == '\0') {
        return false;
    }
    uint32_t value = 0;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        value = (value * 10U) + (uint32_t)(*s - '0');
        if (value > 1000000U) {
            return false;
        }
        ++s;
    }
    *out = value;
    return true;
}

/* Split "a,b" in place. Both halves must be non-empty, and a second comma
 * lands inside `second` where its parser will reject it. */
static bool split_pair(char *args, char **first, char **second)
{
    char *comma = strchr(args, ',');
    if (comma == NULL) {
        return false;
    }
    *comma  = '\0';
    *first  = args;
    *second = comma + 1;
    return (**first != '\0') && (**second != '\0');
}

static bool parse_channel(const char *s, uint32_t max, uint32_t *out)
{
    uint32_t ch;
    if (!parse_uint(s, &ch) || ch < 1U || ch > max) {
        return false;
    }
    *out = ch;
    return true;
}

static bool parse_bool(const char *s, bool *out)
{
    uint32_t v;
    if (!parse_uint(s, &v) || v > 1U) {
        return false;
    }
    *out = (v == 1U);
    return true;
}

/* ---------- value emitters (shared by the individual queries and STATUS) ---------- */

static void emit_relay(void)
{
    at_sendf("+RELAY:%u,%u,%u,%u",
             (unsigned)dev_relay_get(1), (unsigned)dev_relay_get(2),
             (unsigned)dev_relay_get(3), (unsigned)dev_relay_get(4));
}

static char led_mode_char(uint8_t ch)
{
    switch (dev_led_mode_get(ch)) {
    case DEV_LED_WHITE:  return 'W';
    case DEV_LED_TOGGLE: return 'T';
    case DEV_LED_ORANGE:
    default:             return 'O';
    }
}

static void emit_led(void)
{
    /* Reports the configured mode, not the colour a T channel happens to be
     * showing this instant. */
    at_sendf("+LED:%c,%c,%c,%c",
             led_mode_char(1), led_mode_char(2), led_mode_char(3), led_mode_char(4));
}

static void emit_ledivl(void)
{
    at_sendf("+LEDIVL:%u,%u,%u,%u",
             (unsigned)dev_led_interval_get(1), (unsigned)dev_led_interval_get(2),
             (unsigned)dev_led_interval_get(3), (unsigned)dev_led_interval_get(4));
}

static void emit_leden(void)
{
    at_sendf("+LEDEN:%u", (unsigned)dev_led_enable_get());
}

static void emit_ledbri(void)
{
    at_sendf("+LEDBRI:%u", (unsigned)dev_led_brightness_get());
}

static void emit_keys(void)
{
    at_sendf("+KEYS:%u,%u,%u,%u",
             (unsigned)dev_key_is_down(1), (unsigned)dev_key_is_down(2),
             (unsigned)dev_key_is_down(3), (unsigned)dev_key_is_down(4));
}

/* ---------- handlers ---------- */

static bool cmd_relay_set(char *args)
{
    char *ch_text;
    char *value_text;
    uint32_t ch;
    bool on;

    if (!split_pair(args, &ch_text, &value_text)) {
        return false;
    }
    if (!parse_channel(ch_text, DEV_RELAY_COUNT, &ch)) {
        return false;
    }
    if (!parse_bool(value_text, &on)) {
        return false;
    }
    return dev_relay_set((uint8_t)ch, on);
}

static bool cmd_relay_query(char *args)
{
    (void)args;
    emit_relay();
    return true;
}

static bool cmd_led_set(char *args)
{
    char *ch_text;
    char *mode_text;
    uint32_t ch;
    dev_led_mode_t mode;

    if (!split_pair(args, &ch_text, &mode_text)) {
        return false;
    }
    if (!parse_channel(ch_text, DEV_LED_COUNT, &ch)) {
        return false;
    }
    if (mode_text[1] != '\0') {
        return false;
    }
    switch (upper(mode_text[0])) {
    case 'W': mode = DEV_LED_WHITE;  break;
    case 'O': mode = DEV_LED_ORANGE; break;
    case 'T': mode = DEV_LED_TOGGLE; break;
    default:  return false;
    }
    return dev_led_mode_set((uint8_t)ch, mode);
}

static bool cmd_led_query(char *args)
{
    (void)args;
    emit_led();
    return true;
}

static bool cmd_ledivl_set(char *args)
{
    char *ch_text;
    char *ms_text;
    uint32_t ch;
    uint32_t ms;

    if (!split_pair(args, &ch_text, &ms_text)) {
        return false;
    }
    if (!parse_channel(ch_text, DEV_LED_COUNT, &ch)) {
        return false;
    }
    if (!parse_uint(ms_text, &ms)) {
        return false;
    }
    return dev_led_interval_set((uint8_t)ch, ms);
}

static bool cmd_ledivl_query(char *args)
{
    (void)args;
    emit_ledivl();
    return true;
}

static bool cmd_leden_set(char *args)
{
    bool on;
    if (!parse_bool(args, &on)) {
        return false;
    }
    dev_led_enable_set(on);
    return true;
}

static bool cmd_leden_query(char *args)
{
    (void)args;
    emit_leden();
    return true;
}

static bool cmd_ledbri_set(char *args)
{
    uint32_t percent;
    if (!parse_uint(args, &percent)) {
        return false;
    }
    return dev_led_brightness_set(percent);
}

static bool cmd_ledbri_query(char *args)
{
    (void)args;
    emit_ledbri();
    return true;
}

static bool cmd_key_query(char *args)
{
    (void)args;
    emit_keys();
    return true;
}

static bool cmd_status_query(char *args)
{
    (void)args;
    /* Same lines, same order, same formats as the individual queries, so the
     * ESP32 can feed them all to one parser. */
    emit_relay();
    emit_led();
    emit_ledivl();
    emit_leden();
    emit_ledbri();
    emit_keys();
    return true;
}

static bool cmd_ver_query(char *args)
{
    (void)args;
    at_sendf("+VER:%s,%s", X3_FW_NAME, X3_FW_VERSION);
    return true;
}

static bool cmd_rst_exec(char *args)
{
    (void)args;
    /* The UART writer blocks until the transmitter is empty, so OK is fully
     * on the wire before the core is reset. This is the only remote recovery
     * path: x3_uart_board carries the two UART lines and nothing else, so the
     * ESP32 has no way to pull NRST. */
    at_reply_ok();
    NVIC_SystemReset();
    return true;   /* not reached */
}

/* ---------- table and dispatch ---------- */

static const at_cmd_t s_commands[] = {
    { "RELAY",  NULL,         cmd_relay_set,   cmd_relay_query   },
    { "LED",    NULL,         cmd_led_set,     cmd_led_query     },
    { "LEDIVL", NULL,         cmd_ledivl_set,  cmd_ledivl_query  },
    { "LEDEN",  NULL,         cmd_leden_set,   cmd_leden_query   },
    { "LEDBRI", NULL,         cmd_ledbri_set,  cmd_ledbri_query  },
    { "KEY",    NULL,         NULL,            cmd_key_query     },
    { "STATUS", NULL,         NULL,            cmd_status_query  },
    { "VER",    NULL,         NULL,            cmd_ver_query     },
    { "RST",    cmd_rst_exec, NULL,            NULL              },
};

#define COMMAND_COUNT  (sizeof(s_commands) / sizeof(s_commands[0]))

static const at_cmd_t *find_command(const char *name, size_t len)
{
    for (size_t i = 0; i < COMMAND_COUNT; ++i) {
        if (ci_equal(name, len, s_commands[i].name)) {
            return &s_commands[i];
        }
    }
    return NULL;
}

void at_cmd_dispatch(char *line)
{
    if (upper(line[0]) != 'A' || upper(line[1]) != 'T') {
        RTT_LOGW(TAG, "not an AT line");
        at_reply_error();
        return;
    }

    char *rest = line + 2;

    if (*rest == '\0') {
        at_reply_ok();      /* bare AT — liveness check */
        return;
    }
    if (*rest != '+') {
        RTT_LOGW(TAG, "missing '+'");
        at_reply_error();
        return;
    }
    ++rest;

    char *sep = rest;
    while (*sep != '\0' && *sep != '=' && *sep != '?') {
        ++sep;
    }

    const at_cmd_t *cmd = find_command(rest, (size_t)(sep - rest));
    if (cmd == NULL) {
        RTT_LOGW(TAG, "unknown command");
        at_reply_error();
        return;
    }

    at_handler_t handler;
    char *args;

    if (*sep == '=') {
        handler = cmd->set;
        args    = sep + 1;
    } else if (*sep == '?') {
        /* Nothing may follow the '?'. */
        handler = (sep[1] == '\0') ? cmd->query : NULL;
        args    = sep + 1;
    } else {
        handler = cmd->exec;
        args    = sep;
    }

    if (handler == NULL) {
        RTT_LOGW(TAG, "wrong form for %s", cmd->name);
        at_reply_error();
        return;
    }

    if (handler(args)) {
        at_reply_ok();
    } else {
        RTT_LOGW(TAG, "%s rejected its arguments", cmd->name);
        at_reply_error();
    }
}
