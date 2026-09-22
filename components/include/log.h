/* Lightweight logging facade over SEGGER RTT (channel 0).
 *
 * Usage:
 *   #include "log.h"
 *   static const char *TAG = "led";
 *   RTT_LOGI(TAG, "channel %u white", channel);
 *
 * Output format on the RTT terminal:
 *   I (tag): message text
 *
 * Compile-time filter: define LOG_LEVEL_MAX before including log.h to drop
 * higher-numbered (more verbose) levels at the call site. Default = VERBOSE. */
#ifndef X3_LOG_H
#define X3_LOG_H

#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_VERBOSE 5

#ifndef LOG_LEVEL_MAX
#define LOG_LEVEL_MAX LOG_LEVEL_VERBOSE
#endif

void log_init(void);
void log_write(int level, const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#if LOG_LEVEL_MAX >= LOG_LEVEL_ERROR
#define RTT_LOGE(tag, fmt, ...) log_write(LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#else
#define RTT_LOGE(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_WARN
#define RTT_LOGW(tag, fmt, ...) log_write(LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
#else
#define RTT_LOGW(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_INFO
#define RTT_LOGI(tag, fmt, ...) log_write(LOG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__)
#else
#define RTT_LOGI(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_DEBUG
#define RTT_LOGD(tag, fmt, ...) log_write(LOG_LEVEL_DEBUG,   tag, fmt, ##__VA_ARGS__)
#else
#define RTT_LOGD(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_VERBOSE
#define RTT_LOGV(tag, fmt, ...) log_write(LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__)
#else
#define RTT_LOGV(tag, fmt, ...) ((void)0)
#endif

#endif /* X3_LOG_H */
