#include "timer.h"
#include "check.h"
#include "memmgr.h"
#include "kernel.h"

#include <FreeRTOS.h>
#include <timers.h>

typedef struct {
    FurryTimerCallback func;
    void* context;
} TimerCallback_t;

static void TimerCallback(TimerHandle_t hTimer) {
    TimerCallback_t* callb;

    /* Retrieve pointer to callback function and context */
    callb = (TimerCallback_t*)pvTimerGetTimerID(hTimer);

    /* Remove dynamic allocation flag */
    callb = (TimerCallback_t*)((uint32_t)callb & ~1U);

    if(callb != NULL) {
        callb->func(callb->context);
    }
}

FurryTimer* furry_timer_alloc(FurryTimerCallback func, FurryTimerType type, void* context) {
    furry_assert((furry_kernel_is_irq_or_masked() == 0U) && (func != NULL));

    TimerHandle_t hTimer;
    TimerCallback_t* callb;
    UBaseType_t reload;

    hTimer = NULL;

    /* Dynamic memory allocation is available: if memory for callback and */
    /* its context is not provided, allocate it from dynamic memory pool */
    callb = (TimerCallback_t*)malloc(sizeof(TimerCallback_t));

    callb->func = func;
    callb->context = context;

    if(type == FurryTimerTypeOnce) {
        reload = pdFALSE;
    } else {
        reload = pdTRUE;
    }

    /* Store callback memory dynamic allocation flag */
    callb = (TimerCallback_t*)((uint32_t)callb | 1U);
    // TimerCallback function is always provided as a callback and is used to call application
    // specified function with its context both stored in structure callb.
    hTimer = xTimerCreate(NULL, 1, reload, callb, TimerCallback);
    furry_check(hTimer);

    /* Return timer ID */
    return ((FurryTimer*)hTimer);
}

void furry_timer_free(FurryTimer* instance) {
    furry_assert(!furry_kernel_is_irq_or_masked());
    furry_assert(instance);

    TimerHandle_t hTimer = (TimerHandle_t)instance;
    TimerCallback_t* callb;

    callb = (TimerCallback_t*)pvTimerGetTimerID(hTimer);

    furry_check(xTimerDelete(hTimer, portMAX_DELAY) == pdPASS);

    while(furry_timer_is_running(instance)) furry_delay_tick(2);

    if((uint32_t)callb & 1U) {
        /* Callback memory was allocated from dynamic pool, clear flag */
        callb = (TimerCallback_t*)((uint32_t)callb & ~1U);

        /* Return allocated memory to dynamic pool */
        free(callb);
    }
}

FurryStatus furry_timer_start(FurryTimer* instance, uint32_t ticks) {
    furry_assert(!furry_kernel_is_irq_or_masked());
    furry_assert(instance);

    TimerHandle_t hTimer = (TimerHandle_t)instance;
    FurryStatus stat;

    if(xTimerChangePeriod(hTimer, ticks, portMAX_DELAY) == pdPASS) {
        stat = FurryStatusOk;
    } else {
        stat = FurryStatusErrorResource;
    }

    /* Return execution status */
    return (stat);
}

FurryStatus furry_timer_stop(FurryTimer* instance) {
    furry_assert(!furry_kernel_is_irq_or_masked());
    furry_assert(instance);

    TimerHandle_t hTimer = (TimerHandle_t)instance;
    FurryStatus stat;

    if(xTimerIsTimerActive(hTimer) == pdFALSE) {
        stat = FurryStatusErrorResource;
    } else {
        furry_check(xTimerStop(hTimer, portMAX_DELAY) == pdPASS);
        stat = FurryStatusOk;
    }

    /* Return execution status */
    return (stat);
}

uint32_t furry_timer_is_running(FurryTimer* instance) {
    furry_assert(!furry_kernel_is_irq_or_masked());
    furry_assert(instance);

    TimerHandle_t hTimer = (TimerHandle_t)instance;

    /* Return 0: not running, 1: running */
    return (uint32_t)xTimerIsTimerActive(hTimer);
}
