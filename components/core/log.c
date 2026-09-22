#include "log.h"
#include "SEGGER_RTT.h"
#include <stdarg.h>

/* Map level enum value to its prefix character (E/W/I/D/V). Index 0 is a
 * sentinel for an out-of-range argument. */
static const char level_char[] = "?EWIDV";

void log_init(void)
{
    SEGGER_RTT_Init();
}

void log_write(int level, const char *tag, const char *fmt, ...)
{
    if (level < LOG_LEVEL_ERROR || level > LOG_LEVEL_MAX) {
        return;
    }

    SEGGER_RTT_printf(0, "%c (%s): ", level_char[level], tag);

    va_list args;
    va_start(args, fmt);
    SEGGER_RTT_vprintf(0, fmt, &args);
    va_end(args);

    SEGGER_RTT_WriteString(0, "\n");
}
