#include "semaphore.h"
#include "check.h"
#include "common_defines.h"

#include <semphr.h>

FurrySemaphore* furry_semaphore_alloc(uint32_t max_count, uint32_t initial_count) {
    furry_assert(!FURRY_IS_IRQ_MODE());
    furry_assert((max_count > 0U) && (initial_count <= max_count));

    SemaphoreHandle_t hSemaphore = NULL;
    if(max_count == 1U) {
        hSemaphore = xSemaphoreCreateBinary();
        if((hSemaphore != NULL) && (initial_count != 0U)) {
            if(xSemaphoreGive(hSemaphore) != pdPASS) {
                vSemaphoreDelete(hSemaphore);
                hSemaphore = NULL;
            }
        }
    } else {
        hSemaphore = xSemaphoreCreateCounting(max_count, initial_count);
    }

    furry_check(hSemaphore);

    /* Return semaphore ID */
    return ((FurrySemaphore*)hSemaphore);
}

void furry_semaphore_free(FurrySemaphore* instance) {
    furry_assert(instance);
    furry_assert(!FURRY_IS_IRQ_MODE());

    SemaphoreHandle_t hSemaphore = (SemaphoreHandle_t)instance;

    vSemaphoreDelete(hSemaphore);
}

FurryStatus furry_semaphore_acquire(FurrySemaphore* instance, uint32_t timeout) {
    furry_assert(instance);

    SemaphoreHandle_t hSemaphore = (SemaphoreHandle_t)instance;
    FurryStatus stat;
    BaseType_t yield;

    stat = FurryStatusOk;

    if(FURRY_IS_IRQ_MODE()) {
        if(timeout != 0U) {
            stat = FurryStatusErrorParameter;
        } else {
            yield = pdFALSE;

            if(xSemaphoreTakeFromISR(hSemaphore, &yield) != pdPASS) {
                stat = FurryStatusErrorResource;
            } else {
                portYIELD_FROM_ISR(yield);
            }
        }
    } else {
        if(xSemaphoreTake(hSemaphore, (TickType_t)timeout) != pdPASS) {
            if(timeout != 0U) {
                stat = FurryStatusErrorTimeout;
            } else {
                stat = FurryStatusErrorResource;
            }
        }
    }

    /* Return execution status */
    return (stat);
}

FurryStatus furry_semaphore_release(FurrySemaphore* instance) {
    furry_assert(instance);

    SemaphoreHandle_t hSemaphore = (SemaphoreHandle_t)instance;
    FurryStatus stat;
    BaseType_t yield;

    stat = FurryStatusOk;

    if(FURRY_IS_IRQ_MODE()) {
        yield = pdFALSE;

        if(xSemaphoreGiveFromISR(hSemaphore, &yield) != pdTRUE) {
            stat = FurryStatusErrorResource;
        } else {
            portYIELD_FROM_ISR(yield);
        }
    } else {
        if(xSemaphoreGive(hSemaphore) != pdPASS) {
            stat = FurryStatusErrorResource;
        }
    }

    /* Return execution status */
    return (stat);
}

uint32_t furry_semaphore_get_count(FurrySemaphore* instance) {
    furry_assert(instance);

    SemaphoreHandle_t hSemaphore = (SemaphoreHandle_t)instance;
    uint32_t count;

    if(FURRY_IS_IRQ_MODE()) {
        count = (uint32_t)uxSemaphoreGetCountFromISR(hSemaphore);
    } else {
        count = (uint32_t)uxSemaphoreGetCount(hSemaphore);
    }

    /* Return number of tokens */
    return (count);
}
