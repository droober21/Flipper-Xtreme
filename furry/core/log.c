#include "log.h"
#include "check.h"
#include "mutex.h"
#include <furry_hal.h>

#define FURRY_LOG_LEVEL_DEFAULT FurryLogLevelInfo

typedef struct {
    FurryLogLevel log_level;
    FurryLogPuts puts;
    FurryLogTimestamp timestamp;
    FurryMutex* mutex;
} FurryLogParams;

static FurryLogParams furry_log;

void furry_log_init() {
    // Set default logging parameters
    furry_log.log_level = FURRY_LOG_LEVEL_DEFAULT;
    furry_log.puts = furry_hal_console_puts;
    furry_log.timestamp = furry_get_tick;
    furry_log.mutex = furry_mutex_alloc(FurryMutexTypeNormal);
}

void furry_log_print_format(FurryLogLevel level, const char* tag, const char* format, ...) {
    if(level <= furry_log.log_level &&
       furry_mutex_acquire(furry_log.mutex, FurryWaitForever) == FurryStatusOk) {
        FurryString* string;
        string = furry_string_alloc();

        const char* color = _FURRY_LOG_CLR_RESET;
        const char* log_letter = " ";
        switch(level) {
        case FurryLogLevelError:
            color = _FURRY_LOG_CLR_E;
            log_letter = "E";
            break;
        case FurryLogLevelWarn:
            color = _FURRY_LOG_CLR_W;
            log_letter = "W";
            break;
        case FurryLogLevelInfo:
            color = _FURRY_LOG_CLR_I;
            log_letter = "I";
            break;
        case FurryLogLevelDebug:
            color = _FURRY_LOG_CLR_D;
            log_letter = "D";
            break;
        case FurryLogLevelTrace:
            color = _FURRY_LOG_CLR_T;
            log_letter = "T";
            break;
        default:
            break;
        }

        // Timestamp
        furry_string_printf(
            string,
            "%lu %s[%s][%s] " _FURRY_LOG_CLR_RESET,
            furry_log.timestamp(),
            color,
            log_letter,
            tag);
        furry_log.puts(furry_string_get_cstr(string));
        furry_string_reset(string);

        va_list args;
        va_start(args, format);
        furry_string_vprintf(string, format, args);
        va_end(args);

        furry_log.puts(furry_string_get_cstr(string));
        furry_string_free(string);

        furry_log.puts("\r\n");

        furry_mutex_release(furry_log.mutex);
    }
}

void furry_log_print_raw_format(FurryLogLevel level, const char* format, ...) {
    if(level <= furry_log.log_level &&
       furry_mutex_acquire(furry_log.mutex, FurryWaitForever) == FurryStatusOk) {
        FurryString* string;
        string = furry_string_alloc();
        va_list args;
        va_start(args, format);
        furry_string_vprintf(string, format, args);
        va_end(args);

        furry_log.puts(furry_string_get_cstr(string));
        furry_string_free(string);

        furry_mutex_release(furry_log.mutex);
    }
}

void furry_log_set_level(FurryLogLevel level) {
    if(level == FurryLogLevelDefault) {
        level = FURRY_LOG_LEVEL_DEFAULT;
    }
    furry_log.log_level = level;
}

FurryLogLevel furry_log_get_level(void) {
    return furry_log.log_level;
}

void furry_log_set_puts(FurryLogPuts puts) {
    furry_assert(puts);
    furry_log.puts = puts;
}

void furry_log_set_timestamp(FurryLogTimestamp timestamp) {
    furry_assert(timestamp);
    furry_log.timestamp = timestamp;
}
