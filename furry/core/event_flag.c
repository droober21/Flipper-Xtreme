#include "event_flag.h"
#include "common_defines.h"
#include "check.h"

#include <event_groups.h>

#define FURRY_EVENT_FLAG_MAX_BITS_EVENT_GROUPS 24U
#define FURRY_EVENT_FLAG_INVALID_BITS (~((1UL << FURRY_EVENT_FLAG_MAX_BITS_EVENT_GROUPS) - 1U))

FurryEventFlag* furry_event_flag_alloc() {
    furry_assert(!FURRY_IS_IRQ_MODE());

    EventGroupHandle_t handle = xEventGroupCreate();
    furry_check(handle);

    return ((FurryEventFlag*)handle);
}

void furry_event_flag_free(FurryEventFlag* instance) {
    furry_assert(!FURRY_IS_IRQ_MODE());
    vEventGroupDelete((EventGroupHandle_t)instance);
}

uint32_t furry_event_flag_set(FurryEventFlag* instance, uint32_t flags) {
    furry_assert(instance);
    furry_assert((flags & FURRY_EVENT_FLAG_INVALID_BITS) == 0U);

    EventGroupHandle_t hEventGroup = (EventGroupHandle_t)instance;
    uint32_t rflags;
    BaseType_t yield;

    if(FURRY_IS_IRQ_MODE()) {
        yield = pdFALSE;
        if(xEventGroupSetBitsFromISR(hEventGroup, (EventBits_t)flags, &yield) == pdFAIL) {
            rflags = (uint32_t)FurryFlagErrorResource;
        } else {
            rflags = flags;
            portYIELD_FROM_ISR(yield);
        }
    } else {
        rflags = xEventGroupSetBits(hEventGroup, (EventBits_t)flags);
    }

    /* Return event flags after setting */
    return (rflags);
}

uint32_t furry_event_flag_clear(FurryEventFlag* instance, uint32_t flags) {
    furry_assert(instance);
    furry_assert((flags & FURRY_EVENT_FLAG_INVALID_BITS) == 0U);

    EventGroupHandle_t hEventGroup = (EventGroupHandle_t)instance;
    uint32_t rflags;

    if(FURRY_IS_IRQ_MODE()) {
        rflags = xEventGroupGetBitsFromISR(hEventGroup);

        if(xEventGroupClearBitsFromISR(hEventGroup, (EventBits_t)flags) == pdFAIL) {
            rflags = (uint32_t)FurryStatusErrorResource;
        } else {
            /* xEventGroupClearBitsFromISR only registers clear operation in the timer command queue. */
            /* Yield is required here otherwise clear operation might not execute in the right order. */
            /* See https://github.com/FreeRTOS/FreeRTOS-Kernel/issues/93 for more info.               */
            portYIELD_FROM_ISR(pdTRUE);
        }
    } else {
        rflags = xEventGroupClearBits(hEventGroup, (EventBits_t)flags);
    }

    /* Return event flags before clearing */
    return (rflags);
}

uint32_t furry_event_flag_get(FurryEventFlag* instance) {
    furry_assert(instance);

    EventGroupHandle_t hEventGroup = (EventGroupHandle_t)instance;
    uint32_t rflags;

    if(FURRY_IS_IRQ_MODE()) {
        rflags = xEventGroupGetBitsFromISR(hEventGroup);
    } else {
        rflags = xEventGroupGetBits(hEventGroup);
    }

    /* Return current event flags */
    return (rflags);
}

uint32_t furry_event_flag_wait(
    FurryEventFlag* instance,
    uint32_t flags,
    uint32_t options,
    uint32_t timeout) {
    furry_assert(!FURRY_IS_IRQ_MODE());
    furry_assert(instance);
    furry_assert((flags & FURRY_EVENT_FLAG_INVALID_BITS) == 0U);

    EventGroupHandle_t hEventGroup = (EventGroupHandle_t)instance;
    BaseType_t wait_all;
    BaseType_t exit_clr;
    uint32_t rflags;

    if(options & FurryFlagWaitAll) {
        wait_all = pdTRUE;
    } else {
        wait_all = pdFAIL;
    }

    if(options & FurryFlagNoClear) {
        exit_clr = pdFAIL;
    } else {
        exit_clr = pdTRUE;
    }

    rflags = xEventGroupWaitBits(
        hEventGroup, (EventBits_t)flags, exit_clr, wait_all, (TickType_t)timeout);

    if(options & FurryFlagWaitAll) {
        if((flags & rflags) != flags) {
            if(timeout > 0U) {
                rflags = (uint32_t)FurryStatusErrorTimeout;
            } else {
                rflags = (uint32_t)FurryStatusErrorResource;
            }
        }
    } else {
        if((flags & rflags) == 0U) {
            if(timeout > 0U) {
                rflags = (uint32_t)FurryStatusErrorTimeout;
            } else {
                rflags = (uint32_t)FurryStatusErrorResource;
            }
        }
    }

    /* Return event flags before clearing */
    return (rflags);
}
