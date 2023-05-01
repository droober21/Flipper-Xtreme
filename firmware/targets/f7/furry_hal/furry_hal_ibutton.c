#include <furry_hal_ibutton.h>
#include <furry_hal_interrupt.h>
#include <furry_hal_resources.h>

#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_exti.h>

#include <furry.h>

#define TAG "FurryHalIbutton"
#define FURRY_HAL_IBUTTON_TIMER TIM1
#define FURRY_HAL_IBUTTON_TIMER_IRQ FurryHalInterruptIdTim1UpTim16

typedef enum {
    FurryHalIbuttonStateIdle,
    FurryHalIbuttonStateRunning,
} FurryHalIbuttonState;

typedef struct {
    FurryHalIbuttonState state;
    FurryHalIbuttonEmulateCallback callback;
    void* context;
} FurryHalIbutton;

FurryHalIbutton* furry_hal_ibutton = NULL;

static void furry_hal_ibutton_emulate_isr() {
    if(LL_TIM_IsActiveFlag_UPDATE(FURRY_HAL_IBUTTON_TIMER)) {
        LL_TIM_ClearFlag_UPDATE(FURRY_HAL_IBUTTON_TIMER);
        furry_hal_ibutton->callback(furry_hal_ibutton->context);
    }
}

void furry_hal_ibutton_init() {
    furry_hal_ibutton = malloc(sizeof(FurryHalIbutton));
    furry_hal_ibutton->state = FurryHalIbuttonStateIdle;

    FURRY_LOG_I(TAG, "Init OK");
}

void furry_hal_ibutton_emulate_start(
    uint32_t period,
    FurryHalIbuttonEmulateCallback callback,
    void* context) {
    furry_assert(furry_hal_ibutton);
    furry_assert(furry_hal_ibutton->state == FurryHalIbuttonStateIdle);

    furry_hal_ibutton->state = FurryHalIbuttonStateRunning;
    furry_hal_ibutton->callback = callback;
    furry_hal_ibutton->context = context;

    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(FURRY_HAL_IBUTTON_TIMER);
    FURRY_CRITICAL_EXIT();

    furry_hal_interrupt_set_isr(FURRY_HAL_IBUTTON_TIMER_IRQ, furry_hal_ibutton_emulate_isr, NULL);

    LL_TIM_SetPrescaler(FURRY_HAL_IBUTTON_TIMER, 0);
    LL_TIM_SetCounterMode(FURRY_HAL_IBUTTON_TIMER, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(FURRY_HAL_IBUTTON_TIMER, period);
    LL_TIM_DisableARRPreload(FURRY_HAL_IBUTTON_TIMER);
    LL_TIM_SetRepetitionCounter(FURRY_HAL_IBUTTON_TIMER, 0);

    LL_TIM_SetClockDivision(FURRY_HAL_IBUTTON_TIMER, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetClockSource(FURRY_HAL_IBUTTON_TIMER, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_GenerateEvent_UPDATE(FURRY_HAL_IBUTTON_TIMER);

    LL_TIM_EnableIT_UPDATE(FURRY_HAL_IBUTTON_TIMER);

    LL_TIM_EnableCounter(FURRY_HAL_IBUTTON_TIMER);
}

void furry_hal_ibutton_emulate_set_next(uint32_t period) {
    LL_TIM_SetAutoReload(FURRY_HAL_IBUTTON_TIMER, period);
}

void furry_hal_ibutton_emulate_stop() {
    furry_assert(furry_hal_ibutton);

    if(furry_hal_ibutton->state == FurryHalIbuttonStateRunning) {
        furry_hal_ibutton->state = FurryHalIbuttonStateIdle;
        LL_TIM_DisableCounter(FURRY_HAL_IBUTTON_TIMER);

        FURRY_CRITICAL_ENTER();
        LL_TIM_DeInit(FURRY_HAL_IBUTTON_TIMER);
        FURRY_CRITICAL_EXIT();

        furry_hal_interrupt_set_isr(FURRY_HAL_IBUTTON_TIMER_IRQ, NULL, NULL);

        furry_hal_ibutton->callback = NULL;
        furry_hal_ibutton->context = NULL;
    }
}

void furry_hal_ibutton_pin_configure() {
    furry_hal_gpio_write(&gpio_ibutton, true);
    furry_hal_gpio_init(&gpio_ibutton, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
}

void furry_hal_ibutton_pin_reset() {
    furry_hal_gpio_write(&gpio_ibutton, true);
    furry_hal_gpio_init(&gpio_ibutton, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void furry_hal_ibutton_pin_write(const bool state) {
    furry_hal_gpio_write(&gpio_ibutton, state);
}
