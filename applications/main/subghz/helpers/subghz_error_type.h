#pragma once

#include <furry.h>
#include <furry_hal.h>

/** SubGhzErrorType */
typedef enum {
    SubGhzErrorTypeNoError = 0, /** There are no errors */
    SubGhzErrorTypeParseFile =
        1, /** File parsing error, or wrong file structure, or missing required parameters. more accurate data can be obtained through the debug port */
    SubGhzErrorTypeOnlyRX =
        2, /** Transmission on this frequency is blocked by regional settings */
} SubGhzErrorType;
