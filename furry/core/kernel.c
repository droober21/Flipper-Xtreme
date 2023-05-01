#include "kernel.h"
#include "base.h"
#include "check.h"
#include "common_defines.h"

#include <furry_hal.h>

#include CMSIS_device_header

bool furry_kernel_is_irq_or_masked() {
    bool irq = false;
    BaseType_t state;

    if(FURRY_IS_IRQ_MODE()) {
        /* Called from interrupt context */
        irq = true;
    } else {
        /* Get FreeRTOS scheduler state */
        state = xTaskGetSchedulerState();

        if(state != taskSCHEDULER_NOT_STARTED) {
            /* Scheduler was started */
            if(FURRY_IS_IRQ_MASKED()) {
                /* Interrupts are masked */
                irq = true;
            }
        }
    }

    /* Return context, 0: thread context, 1: IRQ context */
    return (irq);
}

int32_t furry_kernel_lock() {
    furry_assert(!furry_kernel_is_irq_or_masked());

    int32_t lock;

    switch(xTaskGetSchedulerState()) {
    case taskSCHEDULER_SUSPENDED:
        lock = 1;
        break;

    case taskSCHEDULER_RUNNING:
        vTaskSuspendAll();
        lock = 0;
        break;

    case taskSCHEDULER_NOT_STARTED:
    default:
        lock = (int32_t)FurryStatusError;
        break;
    }

    /* Return previous lock state */
    return (lock);
}

int32_t furry_kernel_unlock() {
    furry_assert(!furry_kernel_is_irq_or_masked());

    int32_t lock;

    switch(xTaskGetSchedulerState()) {
    case taskSCHEDULER_SUSPENDED:
        lock = 1;

        if(xTaskResumeAll() != pdTRUE) {
            if(xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED) {
                lock = (int32_t)FurryStatusError;
            }
        }
        break;

    case taskSCHEDULER_RUNNING:
        lock = 0;
        break;

    case taskSCHEDULER_NOT_STARTED:
    default:
        lock = (int32_t)FurryStatusError;
        break;
    }

    /* Return previous lock state */
    return (lock);
}

int32_t furry_kernel_restore_lock(int32_t lock) {
    furry_assert(!furry_kernel_is_irq_or_masked());

    switch(xTaskGetSchedulerState()) {
    case taskSCHEDULER_SUSPENDED:
    case taskSCHEDULER_RUNNING:
        if(lock == 1) {
            vTaskSuspendAll();
        } else {
            if(lock != 0) {
                lock = (int32_t)FurryStatusError;
            } else {
                if(xTaskResumeAll() != pdTRUE) {
                    if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
                        lock = (int32_t)FurryStatusError;
                    }
                }
            }
        }
        break;

    case taskSCHEDULER_NOT_STARTED:
    default:
        lock = (int32_t)FurryStatusError;
        break;
    }

    /* Return new lock state */
    return (lock);
}

uint32_t furry_kernel_get_tick_frequency() {
    /* Return frequency in hertz */
    return (configTICK_RATE_HZ_RAW);
}

void furry_delay_tick(uint32_t ticks) {
    furry_assert(!furry_kernel_is_irq_or_masked());
    if(ticks == 0U) {
        taskYIELD();
    } else {
        vTaskDelay(ticks);
    }
}

FurryStatus furry_delay_until_tick(uint32_t tick) {
    furry_assert(!furry_kernel_is_irq_or_masked());

    TickType_t tcnt, delay;
    FurryStatus stat;

    stat = FurryStatusOk;
    tcnt = xTaskGetTickCount();

    /* Determine remaining number of tick to delay */
    delay = (TickType_t)tick - tcnt;

    /* Check if target tick has not expired */
    if((delay != 0U) && (0 == (delay >> (8 * sizeof(TickType_t) - 1)))) {
        if(xTaskDelayUntil(&tcnt, delay) == pdFALSE) {
            /* Did not delay */
            stat = FurryStatusError;
        }
    } else {
        /* No delay or already expired */
        stat = FurryStatusErrorParameter;
    }

    /* Return execution status */
    return (stat);
}

uint32_t furry_get_tick() {
    TickType_t ticks;

    if(furry_kernel_is_irq_or_masked() != 0U) {
        ticks = xTaskGetTickCountFromISR();
    } else {
        ticks = xTaskGetTickCount();
    }

    return ticks;
}

uint32_t furry_ms_to_ticks(uint32_t milliseconds) {
#if configTICK_RATE_HZ_RAW == 1000
    return milliseconds;
#else
    return (uint32_t)((float)configTICK_RATE_HZ_RAW) / 1000.0f * (float)milliseconds;
#endif
}

void furry_delay_ms(uint32_t milliseconds) {
    if(!FURRY_IS_ISR() && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        if(milliseconds > 0 && milliseconds < portMAX_DELAY - 1) {
            milliseconds += 1;
        }
#if configTICK_RATE_HZ_RAW == 1000
        furry_delay_tick(milliseconds);
#else
        furry_delay_tick(furry_ms_to_ticks(milliseconds));
#endif
    } else if(milliseconds > 0) {
        furry_delay_us(milliseconds * 1000);
    }
}

void furry_delay_us(uint32_t microseconds) {
    furry_hal_cortex_delay_us(microseconds);
}
