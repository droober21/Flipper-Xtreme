#pragma once

#include "core/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FurryTimerCallback)(void* context);

typedef enum {
    FurryTimerTypeOnce = 0, ///< One-shot timer.
    FurryTimerTypePeriodic = 1 ///< Repeating timer.
} FurryTimerType;

typedef void FurryTimer;

/** Allocate timer
 *
 * @param[in]  func     The callback function
 * @param[in]  type     The timer type
 * @param      context  The callback context
 *
 * @return     The pointer to FurryTimer instance
 */
FurryTimer* furry_timer_alloc(FurryTimerCallback func, FurryTimerType type, void* context);

/** Free timer
 *
 * @param      instance  The pointer to FurryTimer instance
 */
void furry_timer_free(FurryTimer* instance);

/** Start timer
 *
 * @param      instance  The pointer to FurryTimer instance
 * @param[in]  ticks     The ticks
 *
 * @return     The furry status.
 */
FurryStatus furry_timer_start(FurryTimer* instance, uint32_t ticks);

/** Stop timer
 *
 * @param      instance  The pointer to FurryTimer instance
 *
 * @return     The furry status.
 */
FurryStatus furry_timer_stop(FurryTimer* instance);

/** Is timer running
 *
 * @param      instance  The pointer to FurryTimer instance
 *
 * @return     0: not running, 1: running
 */
uint32_t furry_timer_is_running(FurryTimer* instance);

#ifdef __cplusplus
}
#endif
