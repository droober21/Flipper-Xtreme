/**
 * @file furry_hal_cortex.h
 * ARM Cortex HAL
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Cortex timer provides high precision low level expiring timer */
typedef struct {
    uint32_t start;
    uint32_t value;
} FurryHalCortexTimer;

/** Early init stage for cortex
 */
void furry_hal_cortex_init_early();

/** Microseconds delay
 *
 * @param[in]  microseconds  The microseconds to wait
 */
void furry_hal_cortex_delay_us(uint32_t microseconds);

/** Get instructions per microsecond count
 *
 * @return     instructions per microsecond count
 */
uint32_t furry_hal_cortex_instructions_per_microsecond();

/** Get Timer
 *
 * @param[in]  timeout_us  The expire timeout in us
 *
 * @return     The FurryHalCortexTimer
 */
FurryHalCortexTimer furry_hal_cortex_timer_get(uint32_t timeout_us);

/** Check if timer expired
 *
 * @param[in]  cortex_timer  The FurryHalCortexTimer
 *
 * @return     true if expired
 */
bool furry_hal_cortex_timer_is_expired(FurryHalCortexTimer cortex_timer);

/** Wait for timer expire
 *
 * @param[in]  cortex_timer  The FurryHalCortexTimer
 */
void furry_hal_cortex_timer_wait(FurryHalCortexTimer cortex_timer);

#ifdef __cplusplus
}
#endif
