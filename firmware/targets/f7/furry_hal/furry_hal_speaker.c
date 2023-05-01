#include <furry_hal_speaker.h>
#include <furry_hal_gpio.h>
#include <furry_hal_resources.h>
#include <furry_hal_power.h>

#include <stm32wbxx_ll_tim.h>
#include <furry_hal_cortex.h>

#define TAG "FurryHalSpeaker"

#define FURRY_HAL_SPEAKER_TIMER TIM16
#define FURRY_HAL_SPEAKER_CHANNEL LL_TIM_CHANNEL_CH1
#define FURRY_HAL_SPEAKER_PRESCALER 500
#define FURRY_HAL_SPEAKER_MAX_VOLUME 60

static FurryMutex* furry_hal_speaker_mutex = NULL;

// #define FURRY_HAL_SPEAKER_NEW_VOLUME

void furry_hal_speaker_init() {
    furry_assert(furry_hal_speaker_mutex == NULL);
    furry_hal_speaker_mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(FURRY_HAL_SPEAKER_TIMER);
    FURRY_CRITICAL_EXIT();
    FURRY_LOG_I(TAG, "Init OK");
}

void furry_hal_speaker_deinit() {
    furry_check(furry_hal_speaker_mutex != NULL);
    LL_TIM_DeInit(FURRY_HAL_SPEAKER_TIMER);
    furry_hal_gpio_init(&gpio_speaker, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_mutex_free(furry_hal_speaker_mutex);
    furry_hal_speaker_mutex = NULL;
}

bool furry_hal_speaker_acquire(uint32_t timeout) {
    furry_check(!FURRY_IS_IRQ_MODE());

    if(furry_mutex_acquire(furry_hal_speaker_mutex, timeout) == FurryStatusOk) {
        furry_hal_power_insomnia_enter();
        furry_hal_gpio_init_ex(
            &gpio_speaker, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn14TIM16);
        return true;
    } else {
        return false;
    }
}

void furry_hal_speaker_release() {
    furry_check(!FURRY_IS_IRQ_MODE());
    furry_check(furry_hal_speaker_is_mine());

    furry_hal_speaker_stop();
    furry_hal_gpio_init(&gpio_speaker, GpioModeAnalog, GpioPullDown, GpioSpeedLow);
    furry_hal_power_insomnia_exit();

    furry_check(furry_mutex_release(furry_hal_speaker_mutex) == FurryStatusOk);
}

bool furry_hal_speaker_is_mine() {
    return (FURRY_IS_IRQ_MODE()) ||
           (furry_mutex_get_owner(furry_hal_speaker_mutex) == furry_thread_get_current_id());
}

static inline uint32_t furry_hal_speaker_calculate_autoreload(float frequency) {
    uint32_t autoreload = (SystemCoreClock / FURRY_HAL_SPEAKER_PRESCALER / frequency) - 1;
    if(autoreload < 2) {
        autoreload = 2;
    } else if(autoreload > UINT16_MAX) {
        autoreload = UINT16_MAX;
    }

    return autoreload;
}

static inline uint32_t furry_hal_speaker_calculate_compare(float volume) {
    if(volume < 0) volume = 0;
    if(volume > 1) volume = 1;
    volume = volume * volume * volume;

#ifdef FURRY_HAL_SPEAKER_NEW_VOLUME
    uint32_t compare_value = volume * FURRY_HAL_SPEAKER_MAX_VOLUME;
    uint32_t clip_value = volume * LL_TIM_GetAutoReload(FURRY_HAL_SPEAKER_TIMER) / 2;
    if(compare_value > clip_value) {
        compare_value = clip_value;
    }
#else
    uint32_t compare_value = volume * LL_TIM_GetAutoReload(FURRY_HAL_SPEAKER_TIMER) / 2;
#endif

    if(compare_value == 0) {
        compare_value = 1;
    }

    return compare_value;
}

void furry_hal_speaker_start(float frequency, float volume) {
    furry_check(furry_hal_speaker_is_mine());

    if(volume <= 0) {
        furry_hal_speaker_stop();
        return;
    }

    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Prescaler = FURRY_HAL_SPEAKER_PRESCALER - 1;
    TIM_InitStruct.Autoreload = furry_hal_speaker_calculate_autoreload(frequency);
    LL_TIM_Init(FURRY_HAL_SPEAKER_TIMER, &TIM_InitStruct);

    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_InitStruct.CompareValue = furry_hal_speaker_calculate_compare(volume);
    LL_TIM_OC_Init(FURRY_HAL_SPEAKER_TIMER, FURRY_HAL_SPEAKER_CHANNEL, &TIM_OC_InitStruct);

    LL_TIM_EnableAllOutputs(FURRY_HAL_SPEAKER_TIMER);
    LL_TIM_EnableCounter(FURRY_HAL_SPEAKER_TIMER);
}

void furry_hal_speaker_set_volume(float volume) {
    furry_check(furry_hal_speaker_is_mine());
    if(volume <= 0) {
        furry_hal_speaker_stop();
        return;
    }

#if FURRY_HAL_SPEAKER_CHANNEL == LL_TIM_CHANNEL_CH1
    LL_TIM_OC_SetCompareCH1(FURRY_HAL_SPEAKER_TIMER, furry_hal_speaker_calculate_compare(volume));
#else
#error Invalid channel
#endif
}

void furry_hal_speaker_stop() {
    furry_check(furry_hal_speaker_is_mine());
    LL_TIM_DisableAllOutputs(FURRY_HAL_SPEAKER_TIMER);
    LL_TIM_DisableCounter(FURRY_HAL_SPEAKER_TIMER);
}
