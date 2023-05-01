#ifndef BC_LOGGING_H_
#define BC_LOGGING_H_

#include <furry.h>
#include "err.h" // appName

//!  WARNING: There is a bug in Furry such that if you crank LOG_LEVEL up to 6=TRACE
//!           AND you have menu->settings->system->logLevel = trace
//!           THEN this program will cause the FZ to crash when the plugin exits!
#define LOG_LEVEL 4

//----------------------------------------------------------------------------- ----------------------------------------
// The FlipperZero Settings->System menu allows you to set the logging level at RUN-time
// ... LOG_LEVEL lets you limit it at COMPILE-time
//
// FURRY logging has 6 levels (numbered 1 thru 6}
//    1.  None
//    2.  Errors       FURRY_LOG_E
//    3.  Warnings     FURRY_LOG_W
//    4.  Information  FURRY_LOG_I
//    5.  Debug        FURRY_LOG_D
//    6.  Trace        FURRY_LOG_T
//
// --> furry/core/log.h
//

// The FlipperZero Settings->System menu allows you to set the logging level at RUN-time
// This lets you limit it at COMPILE-time
#ifndef LOG_LEVEL
#define LOG_LEVEL 6 // default = full logging
#endif

#if(LOG_LEVEL < 2)
#undef FURRY_LOG_E
#define FURRY_LOG_E(tag, fmt, ...)
#endif

#if(LOG_LEVEL < 3)
#undef FURRY_LOG_W
#define FURRY_LOG_W(tag, fmt, ...)
#endif

#if(LOG_LEVEL < 4)
#undef FURRY_LOG_I
#define FURRY_LOG_I(tag, fmt, ...)
#endif

#if(LOG_LEVEL < 5)
#undef FURRY_LOG_D
#define FURRY_LOG_D(tag, fmt, ...)
#endif

#if(LOG_LEVEL < 6)
#undef FURRY_LOG_T
#define FURRY_LOG_T(tag, fmt, ...)
#endif

//----------------------------------------------------------
// Logging helper macros
//
#define ERROR(fmt, ...) FURRY_LOG_E(appName, fmt __VA_OPT__(, ) __VA_ARGS__)
#define WARN(fmt, ...) FURRY_LOG_W(appName, fmt __VA_OPT__(, ) __VA_ARGS__)
#define INFO(fmt, ...) FURRY_LOG_I(appName, fmt __VA_OPT__(, ) __VA_ARGS__)
#define DEBUG(fmt, ...) FURRY_LOG_D(appName, fmt __VA_OPT__(, ) __VA_ARGS__)
#define TRACE(fmt, ...) FURRY_LOG_T(appName, fmt __VA_OPT__(, ) __VA_ARGS__)

#define ENTER TRACE("(+) %s", __func__)
#define LEAVE TRACE("(-) %s", __func__)

#endif //BC_LOGGING_H_
