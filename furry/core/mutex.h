/**
 * @file mutex.h
 * FurryMutex
 */
#pragma once

#include "base.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FurryMutexTypeNormal,
    FurryMutexTypeRecursive,
} FurryMutexType;

typedef void FurryMutex;

/** Allocate FurryMutex
 *
 * @param[in]  type  The mutex type
 *
 * @return     pointer to FurryMutex instance
 */
FurryMutex* furry_mutex_alloc(FurryMutexType type);

/** Free FurryMutex
 *
 * @param      instance  The pointer to FurryMutex instance
 */
void furry_mutex_free(FurryMutex* instance);

/** Acquire mutex
 *
 * @param      instance  The pointer to FurryMutex instance
 * @param[in]  timeout   The timeout
 *
 * @return     The furry status.
 */
FurryStatus furry_mutex_acquire(FurryMutex* instance, uint32_t timeout);

/** Release mutex
 *
 * @param      instance  The pointer to FurryMutex instance
 *
 * @return     The furry status.
 */
FurryStatus furry_mutex_release(FurryMutex* instance);

/** Get mutex owner thread id
 *
 * @param      instance  The pointer to FurryMutex instance
 *
 * @return     The furry thread identifier.
 */
FurryThreadId furry_mutex_get_owner(FurryMutex* instance);

#ifdef __cplusplus
}
#endif
