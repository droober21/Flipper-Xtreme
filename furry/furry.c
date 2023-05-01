#include "furry.h"
#include <string.h>
#include "queue.h"

void furry_init() {
    furry_assert(!furry_kernel_is_irq_or_masked());
    furry_assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);

    furry_log_init();
    furry_record_init();
}

void furry_run() {
    furry_assert(!furry_kernel_is_irq_or_masked());
    furry_assert(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);

#if(__ARM_ARCH_7A__ == 0U)
    /* Service Call interrupt might be configured before kernel start      */
    /* and when its priority is lower or equal to BASEPRI, svc instruction */
    /* causes a Hard Fault.                                                */
    NVIC_SetPriority(SVCall_IRQn, 0U);
#endif

    /* Start the kernel scheduler */
    vTaskStartScheduler();
}
