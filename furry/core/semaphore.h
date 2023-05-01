/**
 * @file semaphore.h
 * FurrySemaphore
 */
#pragma once

#include "base.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void FurrySemaphore;

/** Allocate semaphore
 *
 * @param[in]  max_count      The maximum count
 * @param[in]  initial_count  The initial count
 *
 * @return     pointer to FurrySemaphore instance
 */
FurrySemaphore* furry_semaphore_alloc(uint32_t max_count, uint32_t initial_count);

/** Free semaphore
 *
 * @param      instance  The pointer to FurrySemaphore instance
 */
void furry_semaphore_free(FurrySemaphore* instance);

/** Acquire semaphore
 *
 * @param      instance  The pointer to FurrySemaphore instance
 * @param[in]  timeout   The timeout
 *
 * @return     The furry status.
 */
FurryStatus furry_semaphore_acquire(FurrySemaphore* instance, uint32_t timeout);

/** Release semaphore
 *
 * @param      instance  The pointer to FurrySemaphore instance
 *
 * @return     The furry status.
 */
FurryStatus furry_semaphore_release(FurrySemaphore* instance);

/** Get semaphore count
 *
 * @param      instance  The pointer to FurrySemaphore instance
 *
 * @return     Semaphore count
 */
uint32_t furry_semaphore_get_count(FurrySemaphore* instance);

#ifdef __cplusplus
}
#endif
