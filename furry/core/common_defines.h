#pragma once

#include "core_defines.h"
#include <stdbool.h>
#include <FreeRTOS.h>
#include <task.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <cmsis_compiler.h>

#ifndef FURRY_WARN_UNUSED
#define FURRY_WARN_UNUSED __attribute__((warn_unused_result))
#endif

#ifndef FURRY_IS_IRQ_MASKED
#define FURRY_IS_IRQ_MASKED() (__get_PRIMASK() != 0U)
#endif

#ifndef FURRY_IS_IRQ_MODE
#define FURRY_IS_IRQ_MODE() (__get_IPSR() != 0U)
#endif

#ifndef FURRY_IS_ISR
#define FURRY_IS_ISR() (FURRY_IS_IRQ_MODE() || FURRY_IS_IRQ_MASKED())
#endif

#ifndef FURRY_CRITICAL_ENTER
#define FURRY_CRITICAL_ENTER()                                                    \
    uint32_t __isrm = 0;                                                         \
    bool __from_isr = FURRY_IS_ISR();                                             \
    bool __kernel_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING); \
    if(__from_isr) {                                                             \
        __isrm = taskENTER_CRITICAL_FROM_ISR();                                  \
    } else if(__kernel_running) {                                                \
        taskENTER_CRITICAL();                                                    \
    } else {                                                                     \
        __disable_irq();                                                         \
    }
#endif

#ifndef FURRY_CRITICAL_EXIT
#define FURRY_CRITICAL_EXIT()                \
    if(__from_isr) {                        \
        taskEXIT_CRITICAL_FROM_ISR(__isrm); \
    } else if(__kernel_running) {           \
        taskEXIT_CRITICAL();                \
    } else {                                \
        __enable_irq();                     \
    }
#endif

#ifdef __cplusplus
}
#endif
