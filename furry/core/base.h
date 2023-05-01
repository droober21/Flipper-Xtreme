#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FurryWaitForever = 0xFFFFFFFFU,
} FurryWait;

typedef enum {
    FurryFlagWaitAny = 0x00000000U, ///< Wait for any flag (default).
    FurryFlagWaitAll = 0x00000001U, ///< Wait for all flags.
    FurryFlagNoClear = 0x00000002U, ///< Do not clear flags which have been specified to wait for.

    FurryFlagError = 0x80000000U, ///< Error indicator.
    FurryFlagErrorUnknown = 0xFFFFFFFFU, ///< FurryStatusError (-1).
    FurryFlagErrorTimeout = 0xFFFFFFFEU, ///< FurryStatusErrorTimeout (-2).
    FurryFlagErrorResource = 0xFFFFFFFDU, ///< FurryStatusErrorResource (-3).
    FurryFlagErrorParameter = 0xFFFFFFFCU, ///< FurryStatusErrorParameter (-4).
    FurryFlagErrorISR = 0xFFFFFFFAU, ///< FurryStatusErrorISR (-6).
} FurryFlag;

typedef enum {
    FurryStatusOk = 0, ///< Operation completed successfully.
    FurryStatusError =
        -1, ///< Unspecified RTOS error: run-time error but no other error message fits.
    FurryStatusErrorTimeout = -2, ///< Operation not completed within the timeout period.
    FurryStatusErrorResource = -3, ///< Resource not available.
    FurryStatusErrorParameter = -4, ///< Parameter error.
    FurryStatusErrorNoMemory =
        -5, ///< System is out of memory: it was impossible to allocate or reserve memory for the operation.
    FurryStatusErrorISR =
        -6, ///< Not allowed in ISR context: the function cannot be called from interrupt service routines.
    FurryStatusReserved = 0x7FFFFFFF ///< Prevents enum down-size compiler optimization.
} FurryStatus;

#ifdef __cplusplus
}
#endif
