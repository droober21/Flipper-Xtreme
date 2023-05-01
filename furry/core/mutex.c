#include "mutex.h"
#include "check.h"
#include "common_defines.h"

#include <semphr.h>

FurryMutex* furry_mutex_alloc(FurryMutexType type) {
    furry_assert(!FURRY_IS_IRQ_MODE());

    SemaphoreHandle_t hMutex = NULL;

    if(type == FurryMutexTypeNormal) {
        hMutex = xSemaphoreCreateMutex();
    } else if(type == FurryMutexTypeRecursive) {
        hMutex = xSemaphoreCreateRecursiveMutex();
    } else {
        furry_crash("Programming error");
    }

    furry_check(hMutex != NULL);

    if(type == FurryMutexTypeRecursive) {
        /* Set LSB as 'recursive mutex flag' */
        hMutex = (SemaphoreHandle_t)((uint32_t)hMutex | 1U);
    }

    /* Return mutex ID */
    return ((FurryMutex*)hMutex);
}

void furry_mutex_free(FurryMutex* instance) {
    furry_assert(!FURRY_IS_IRQ_MODE());
    furry_assert(instance);

    vSemaphoreDelete((SemaphoreHandle_t)((uint32_t)instance & ~1U));
}

FurryStatus furry_mutex_acquire(FurryMutex* instance, uint32_t timeout) {
    SemaphoreHandle_t hMutex;
    FurryStatus stat;
    uint32_t rmtx;

    hMutex = (SemaphoreHandle_t)((uint32_t)instance & ~1U);

    /* Extract recursive mutex flag */
    rmtx = (uint32_t)instance & 1U;

    stat = FurryStatusOk;

    if(FURRY_IS_IRQ_MODE()) {
        stat = FurryStatusErrorISR;
    } else if(hMutex == NULL) {
        stat = FurryStatusErrorParameter;
    } else {
        if(rmtx != 0U) {
            if(xSemaphoreTakeRecursive(hMutex, timeout) != pdPASS) {
                if(timeout != 0U) {
                    stat = FurryStatusErrorTimeout;
                } else {
                    stat = FurryStatusErrorResource;
                }
            }
        } else {
            if(xSemaphoreTake(hMutex, timeout) != pdPASS) {
                if(timeout != 0U) {
                    stat = FurryStatusErrorTimeout;
                } else {
                    stat = FurryStatusErrorResource;
                }
            }
        }
    }

    /* Return execution status */
    return (stat);
}

FurryStatus furry_mutex_release(FurryMutex* instance) {
    SemaphoreHandle_t hMutex;
    FurryStatus stat;
    uint32_t rmtx;

    hMutex = (SemaphoreHandle_t)((uint32_t)instance & ~1U);

    /* Extract recursive mutex flag */
    rmtx = (uint32_t)instance & 1U;

    stat = FurryStatusOk;

    if(FURRY_IS_IRQ_MODE()) {
        stat = FurryStatusErrorISR;
    } else if(hMutex == NULL) {
        stat = FurryStatusErrorParameter;
    } else {
        if(rmtx != 0U) {
            if(xSemaphoreGiveRecursive(hMutex) != pdPASS) {
                stat = FurryStatusErrorResource;
            }
        } else {
            if(xSemaphoreGive(hMutex) != pdPASS) {
                stat = FurryStatusErrorResource;
            }
        }
    }

    /* Return execution status */
    return (stat);
}

FurryThreadId furry_mutex_get_owner(FurryMutex* instance) {
    SemaphoreHandle_t hMutex;
    FurryThreadId owner;

    hMutex = (SemaphoreHandle_t)((uint32_t)instance & ~1U);

    if((FURRY_IS_IRQ_MODE()) || (hMutex == NULL)) {
        owner = 0;
    } else {
        owner = (FurryThreadId)xSemaphoreGetMutexHolder(hMutex);
    }

    /* Return owner thread ID */
    return (owner);
}
