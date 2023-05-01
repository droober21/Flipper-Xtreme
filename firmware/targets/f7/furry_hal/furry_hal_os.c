#include <furry_hal_os.h>
#include <furry_hal_clock.h>
#include <furry_hal_console.h>
#include <furry_hal_power.h>
#include <furry_hal_gpio.h>
#include <furry_hal_resources.h>
#include <furry_hal_idle_timer.h>

#include <stm32wbxx_ll_cortex.h>

#include <furry.h>

#define TAG "FurryHalOs"

#define FURRY_HAL_IDLE_TIMER_CLK_HZ 32768
#define FURRY_HAL_OS_TICK_HZ configTICK_RATE_HZ

#define FURRY_HAL_OS_IDLE_CNT_TO_TICKS(x) (((x)*FURRY_HAL_OS_TICK_HZ) / FURRY_HAL_IDLE_TIMER_CLK_HZ)
#define FURRY_HAL_OS_TICKS_TO_IDLE_CNT(x) (((x)*FURRY_HAL_IDLE_TIMER_CLK_HZ) / FURRY_HAL_OS_TICK_HZ)

#define FURRY_HAL_IDLE_TIMER_TICK_PER_EPOCH (FURRY_HAL_OS_IDLE_CNT_TO_TICKS(FURRY_HAL_IDLE_TIMER_MAX))
#define FURRY_HAL_OS_MAX_SLEEP (FURRY_HAL_IDLE_TIMER_TICK_PER_EPOCH - 1)

#define FURRY_HAL_OS_NVIC_IS_PENDING() (NVIC->ISPR[0] || NVIC->ISPR[1])
#define FURRY_HAL_OS_EXTI_LINE_0_31 0
#define FURRY_HAL_OS_EXTI_LINE_32_63 1

// Arbitrary (but small) number for better tick consistency
#define FURRY_HAL_OS_EXTRA_CNT 3

#ifndef FURRY_HAL_OS_DEBUG_AWAKE_GPIO
#define FURRY_HAL_OS_DEBUG_AWAKE_GPIO (&gpio_ext_pa7)
#endif

#ifndef FURRY_HAL_OS_DEBUG_TICK_GPIO
#define FURRY_HAL_OS_DEBUG_TICK_GPIO (&gpio_ext_pa6)
#endif

#ifndef FURRY_HAL_OS_DEBUG_SECOND_GPIO
#define FURRY_HAL_OS_DEBUG_SECOND_GPIO (&gpio_ext_pa4)
#endif

#ifdef FURRY_HAL_OS_DEBUG
#include <stm32wbxx_ll_gpio.h>

void furry_hal_os_timer_callback() {
    furry_hal_gpio_write(
        FURRY_HAL_OS_DEBUG_SECOND_GPIO, !furry_hal_gpio_read(FURRY_HAL_OS_DEBUG_SECOND_GPIO));
}
#endif

extern void xPortSysTickHandler();

static volatile uint32_t furry_hal_os_skew;

void furry_hal_os_init() {
    furry_hal_idle_timer_init();

#ifdef FURRY_HAL_OS_DEBUG
    furry_hal_gpio_init_simple(FURRY_HAL_OS_DEBUG_AWAKE_GPIO, GpioModeOutputPushPull);
    furry_hal_gpio_init_simple(FURRY_HAL_OS_DEBUG_TICK_GPIO, GpioModeOutputPushPull);
    furry_hal_gpio_init_simple(FURRY_HAL_OS_DEBUG_SECOND_GPIO, GpioModeOutputPushPull);
    furry_hal_gpio_write(FURRY_HAL_OS_DEBUG_AWAKE_GPIO, 1);

    FurryTimer* second_timer =
        furry_timer_alloc(furry_hal_os_timer_callback, FurryTimerTypePeriodic, NULL);
    furry_timer_start(second_timer, FURRY_HAL_OS_TICK_HZ);
#endif

    FURRY_LOG_I(TAG, "Init OK");
}

void furry_hal_os_tick() {
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
#ifdef FURRY_HAL_OS_DEBUG
        furry_hal_gpio_write(
            FURRY_HAL_OS_DEBUG_TICK_GPIO, !furry_hal_gpio_read(FURRY_HAL_OS_DEBUG_TICK_GPIO));
#endif
        xPortSysTickHandler();
    }
}

#ifdef FURRY_HAL_OS_DEBUG
// Find out the IRQ number while debugging
static void furry_hal_os_nvic_dbg_trap() {
    for(int32_t i = WWDG_IRQn; i <= DMAMUX1_OVR_IRQn; i++) {
        if(NVIC_GetPendingIRQ(i)) {
            (void)i;
            // Break here
            __NOP();
        }
    }
}

// Find out the EXTI line number while debugging
static void furry_hal_os_exti_dbg_trap(uint32_t exti, uint32_t val) {
    for(uint32_t i = 0; val; val >>= 1U, ++i) {
        if(val & 1U) {
            (void)exti;
            (void)i;
            // Break here
            __NOP();
        }
    }
}
#endif

static inline bool furry_hal_os_is_pending_irq() {
    if(FURRY_HAL_OS_NVIC_IS_PENDING()) {
#ifdef FURRY_HAL_OS_DEBUG
        furry_hal_os_nvic_dbg_trap();
#endif
        return true;
    }

    uint32_t exti_lines_active;
    if((exti_lines_active = LL_EXTI_ReadFlag_0_31(LL_EXTI_LINE_ALL_0_31))) {
#ifdef FURRY_HAL_OS_DEBUG
        furry_hal_os_exti_dbg_trap(FURRY_HAL_OS_EXTI_LINE_0_31, exti_lines_active);
#endif
        return true;
    } else if((exti_lines_active = LL_EXTI_ReadFlag_32_63(LL_EXTI_LINE_ALL_32_63))) {
#ifdef FURRY_HAL_OS_DEBUG
        furry_hal_os_exti_dbg_trap(FURRY_HAL_OS_EXTI_LINE_32_63, exti_lines_active);
#endif
        return true;
    }

    return false;
}

static inline uint32_t furry_hal_os_sleep(TickType_t expected_idle_ticks) {
    // Stop ticks
    furry_hal_clock_suspend_tick();

    // Start wakeup timer
    furry_hal_idle_timer_start(FURRY_HAL_OS_TICKS_TO_IDLE_CNT(expected_idle_ticks));

#ifdef FURRY_HAL_OS_DEBUG
    furry_hal_gpio_write(FURRY_HAL_OS_DEBUG_AWAKE_GPIO, 0);
#endif

    // Go to sleep mode
    furry_hal_power_sleep();

#ifdef FURRY_HAL_OS_DEBUG
    furry_hal_gpio_write(FURRY_HAL_OS_DEBUG_AWAKE_GPIO, 1);
#endif

    // Calculate how much time we spent in the sleep
    uint32_t after_cnt = furry_hal_idle_timer_get_cnt() + furry_hal_os_skew + FURRY_HAL_OS_EXTRA_CNT;
    uint32_t after_tick = FURRY_HAL_OS_IDLE_CNT_TO_TICKS(after_cnt);
    furry_hal_os_skew = after_cnt - FURRY_HAL_OS_TICKS_TO_IDLE_CNT(after_tick);

    bool cmpm = LL_LPTIM_IsActiveFlag_CMPM(FURRY_HAL_IDLE_TIMER);
    bool arrm = LL_LPTIM_IsActiveFlag_ARRM(FURRY_HAL_IDLE_TIMER);
    if(cmpm && arrm) after_tick += expected_idle_ticks;

    // Prepare tick timer for new round
    furry_hal_idle_timer_reset();

    // Resume ticks
    furry_hal_clock_resume_tick();
    return after_tick;
}

void vPortSuppressTicksAndSleep(TickType_t expected_idle_ticks) {
    if(!furry_hal_power_sleep_available()) {
        __WFI();
        return;
    }

    // Limit amount of ticks to maximum that timer can count
    if(expected_idle_ticks > FURRY_HAL_OS_MAX_SLEEP) {
        expected_idle_ticks = FURRY_HAL_OS_MAX_SLEEP;
    }

    // Stop IRQ handling, no one should disturb us till we finish
    __disable_irq();

    // Confirm OS that sleep is still possible
    if(eTaskConfirmSleepModeStatus() == eAbortSleep || furry_hal_os_is_pending_irq()) {
        __enable_irq();
        return;
    }

    // Sleep and track how much ticks we spent sleeping
    uint32_t completed_ticks = furry_hal_os_sleep(expected_idle_ticks);
    // Notify system about time spent in sleep
    if(completed_ticks > 0) {
        vTaskStepTick(MIN(completed_ticks, expected_idle_ticks));
    }

    // Reenable IRQ
    __enable_irq();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    UNUSED(xTask);
    furry_hal_console_puts("\r\n\r\n stack overflow in ");
    furry_hal_console_puts(pcTaskName);
    furry_hal_console_puts("\r\n\r\n");
    furry_crash("StackOverflow");
}
