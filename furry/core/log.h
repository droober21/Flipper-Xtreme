/**
 * @file log.h
 * Furry Logging system
 */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FurryLogLevelDefault = 0,
    FurryLogLevelNone = 1,
    FurryLogLevelError = 2,
    FurryLogLevelWarn = 3,
    FurryLogLevelInfo = 4,
    FurryLogLevelDebug = 5,
    FurryLogLevelTrace = 6,
} FurryLogLevel;

#define _FURRY_LOG_CLR(clr) "\033[0;" clr "m"
#define _FURRY_LOG_CLR_RESET "\033[0m"

#define _FURRY_LOG_CLR_BLACK "30"
#define _FURRY_LOG_CLR_RED "31"
#define _FURRY_LOG_CLR_GREEN "32"
#define _FURRY_LOG_CLR_BROWN "33"
#define _FURRY_LOG_CLR_BLUE "34"
#define _FURRY_LOG_CLR_PURPLE "35"

#define _FURRY_LOG_CLR_E _FURRY_LOG_CLR(_FURRY_LOG_CLR_RED)
#define _FURRY_LOG_CLR_W _FURRY_LOG_CLR(_FURRY_LOG_CLR_BROWN)
#define _FURRY_LOG_CLR_I _FURRY_LOG_CLR(_FURRY_LOG_CLR_GREEN)
#define _FURRY_LOG_CLR_D _FURRY_LOG_CLR(_FURRY_LOG_CLR_BLUE)
#define _FURRY_LOG_CLR_T _FURRY_LOG_CLR(_FURRY_LOG_CLR_PURPLE)

typedef void (*FurryLogPuts)(const char* data);
typedef uint32_t (*FurryLogTimestamp)(void);

/** Initialize logging */
void furry_log_init();

/** Print log record
 * 
 * @param level 
 * @param tag 
 * @param format 
 * @param ... 
 */
void furry_log_print_format(FurryLogLevel level, const char* tag, const char* format, ...)
    _ATTRIBUTE((__format__(__printf__, 3, 4)));

/** Print log record
 * 
 * @param level 
 * @param format 
 * @param ... 
 */
void furry_log_print_raw_format(FurryLogLevel level, const char* format, ...)
    _ATTRIBUTE((__format__(__printf__, 2, 3)));

/** Set log level
 *
 * @param[in]  level  The level
 */
void furry_log_set_level(FurryLogLevel level);

/** Get log level
 *
 * @return     The furry log level.
 */
FurryLogLevel furry_log_get_level();

/** Set log output callback
 *
 * @param[in]  puts  The puts callback
 */
void furry_log_set_puts(FurryLogPuts puts);

/** Set timestamp callback
 *
 * @param[in]  timestamp  The timestamp callback
 */
void furry_log_set_timestamp(FurryLogTimestamp timestamp);

/** Log methods
 *
 * @param      tag     The application tag
 * @param      format  The format
 * @param      ...     VA Args
 */
#define FURRY_LOG_E(tag, format, ...) \
    furry_log_print_format(FurryLogLevelError, tag, format, ##__VA_ARGS__)
#define FURRY_LOG_W(tag, format, ...) \
    furry_log_print_format(FurryLogLevelWarn, tag, format, ##__VA_ARGS__)
#define FURRY_LOG_I(tag, format, ...) \
    furry_log_print_format(FurryLogLevelInfo, tag, format, ##__VA_ARGS__)
#define FURRY_LOG_D(tag, format, ...) \
    furry_log_print_format(FurryLogLevelDebug, tag, format, ##__VA_ARGS__)
#define FURRY_LOG_T(tag, format, ...) \
    furry_log_print_format(FurryLogLevelTrace, tag, format, ##__VA_ARGS__)

/** Log methods
 *
 * @param      format  The raw format 
 * @param      ...     VA Args
 */
#define FURRY_LOG_RAW_E(format, ...) \
    furry_log_print_raw_format(FurryLogLevelError, format, ##__VA_ARGS__)
#define FURRY_LOG_RAW_W(format, ...) \
    furry_log_print_raw_format(FurryLogLevelWarn, format, ##__VA_ARGS__)
#define FURRY_LOG_RAW_I(format, ...) \
    furry_log_print_raw_format(FurryLogLevelInfo, format, ##__VA_ARGS__)
#define FURRY_LOG_RAW_D(format, ...) \
    furry_log_print_raw_format(FurryLogLevelDebug, format, ##__VA_ARGS__)
#define FURRY_LOG_RAW_T(format, ...) \
    furry_log_print_raw_format(FurryLogLevelTrace, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
