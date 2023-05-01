/**
 * @file furry_hal_light.h
 * Light control HAL API
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furry_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Init light driver
 */
void furry_hal_light_init();

/** Set light value
 *
 * @param      light  Light
 * @param      value  light brightness [0-255]
 */
void furry_hal_light_set(Light light, uint8_t value);

/** Start hardware LED blinking mode
 *
 * @param      light  Light
 * @param      brightness  light brightness [0-255]
 * @param      on_time  LED on time in ms
 * @param      period  LED blink period in ms
 */
void furry_hal_light_blink_start(Light light, uint8_t brightness, uint16_t on_time, uint16_t period);

/** Stop hardware LED blinking mode
 */
void furry_hal_light_blink_stop();

/** Set color in hardware LED blinking mode
 *
 * @param      light  Light
 */
void furry_hal_light_blink_set_color(Light light);

/** Execute sequence
 *
 * @param      sequence  Sequence to execute
 */
void furry_hal_light_sequence(const char* sequence);

#ifdef __cplusplus
}
#endif
