#include "speaker_hal.h"

#define FURRY_HAL_SPEAKER_TIMER TIM16
#define FURRY_HAL_SPEAKER_CHANNEL LL_TIM_CHANNEL_CH1
#define FURRY_HAL_SPEAKER_PRESCALER 500

void tracker_speaker_play(float frequency, float pwm) {
    uint32_t autoreload = (SystemCoreClock / FURRY_HAL_SPEAKER_PRESCALER / frequency) - 1;
    if(autoreload < 2) {
        autoreload = 2;
    } else if(autoreload > UINT16_MAX) {
        autoreload = UINT16_MAX;
    }

    if(pwm < 0) pwm = 0;
    if(pwm > 1) pwm = 1;

    uint32_t compare_value = pwm * autoreload;

    if(compare_value == 0) {
        compare_value = 1;
    }

    if(LL_TIM_OC_GetCompareCH1(FURRY_HAL_SPEAKER_TIMER) != compare_value) {
        LL_TIM_OC_SetCompareCH1(FURRY_HAL_SPEAKER_TIMER, compare_value);
    }

    if(LL_TIM_GetAutoReload(FURRY_HAL_SPEAKER_TIMER) != autoreload) {
        LL_TIM_SetAutoReload(FURRY_HAL_SPEAKER_TIMER, autoreload);
        if(LL_TIM_GetCounter(FURRY_HAL_SPEAKER_TIMER) > autoreload) {
            LL_TIM_SetCounter(FURRY_HAL_SPEAKER_TIMER, 0);
        }
    }

    LL_TIM_EnableAllOutputs(FURRY_HAL_SPEAKER_TIMER);
}

void tracker_speaker_stop() {
    LL_TIM_DisableAllOutputs(FURRY_HAL_SPEAKER_TIMER);
}

void tracker_speaker_init() {
    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
        furry_hal_speaker_start(200.0f, 0.01f);
        tracker_speaker_stop();
    }
}

void tracker_speaker_deinit() {
    if(furry_hal_speaker_is_mine()) {
        furry_hal_speaker_stop();
        furry_hal_speaker_release();
    }
}

static FurryHalInterruptISR tracker_isr;
static void* tracker_isr_context;
static void tracker_interrupt_cb(void* context) {
    UNUSED(context);

    if(LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
        LL_TIM_ClearFlag_UPDATE(TIM2);

        if(tracker_isr) {
            tracker_isr(tracker_isr_context);
        }
    }
}

void tracker_interrupt_init(float freq, FurryHalInterruptISR isr, void* context) {
    tracker_isr = isr;
    tracker_isr_context = context;

    furry_hal_interrupt_set_isr(FurryHalInterruptIdTIM2, tracker_interrupt_cb, NULL);

    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    // Prescaler to get 1kHz clock
    TIM_InitStruct.Prescaler = SystemCoreClock / 1000000 - 1;
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    // Auto reload to get freq Hz interrupt
    TIM_InitStruct.Autoreload = (1000000 / freq) - 1;
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(TIM2, &TIM_InitStruct);
    LL_TIM_EnableIT_UPDATE(TIM2);
    LL_TIM_EnableAllOutputs(TIM2);
    LL_TIM_EnableCounter(TIM2);
}

void tracker_interrupt_deinit() {
    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(TIM2);
    FURRY_CRITICAL_EXIT();

    furry_hal_interrupt_set_isr(FurryHalInterruptIdTIM2, NULL, NULL);
}

void tracker_debug_init() {
    furry_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
}

void tracker_debug_set(bool value) {
    furry_hal_gpio_write(&gpio_ext_pc3, value);
}

void tracker_debug_deinit() {
    furry_hal_gpio_init(&gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}