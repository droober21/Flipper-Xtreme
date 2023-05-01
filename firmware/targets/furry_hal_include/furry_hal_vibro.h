/**
 * @file furry_hal_vibro.h
 * Vibro HAL API
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furry_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize vibro
 */
void furry_hal_vibro_init();

/** Turn on/off vibro
 *
 * @param[in]  value  new state
 */
void furry_hal_vibro_on(bool value);

#ifdef __cplusplus
}
#endif
