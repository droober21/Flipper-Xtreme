#include <furry_hal_cortex.h>

#include <stm32wbxx.h>

#define FURRY_HAL_CORTEX_INSTRUCTIONS_PER_MICROSECOND (SystemCoreClock / 1000000)

void furry_hal_cortex_init_early() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0U;

    /* Enable instruction prefetch */
    SET_BIT(FLASH->ACR, FLASH_ACR_PRFTEN);
}

void furry_hal_cortex_delay_us(uint32_t microseconds) {
    uint32_t start = DWT->CYCCNT;
    uint32_t time_ticks = FURRY_HAL_CORTEX_INSTRUCTIONS_PER_MICROSECOND * microseconds;
    while((DWT->CYCCNT - start) < time_ticks) {
    };
}

uint32_t furry_hal_cortex_instructions_per_microsecond() {
    return FURRY_HAL_CORTEX_INSTRUCTIONS_PER_MICROSECOND;
}

FurryHalCortexTimer furry_hal_cortex_timer_get(uint32_t timeout_us) {
    FurryHalCortexTimer cortex_timer = {0};
    cortex_timer.start = DWT->CYCCNT;
    cortex_timer.value = FURRY_HAL_CORTEX_INSTRUCTIONS_PER_MICROSECOND * timeout_us;
    return cortex_timer;
}

bool furry_hal_cortex_timer_is_expired(FurryHalCortexTimer cortex_timer) {
    return !((DWT->CYCCNT - cortex_timer.start) < cortex_timer.value);
}

void furry_hal_cortex_timer_wait(FurryHalCortexTimer cortex_timer) {
    while(!furry_hal_cortex_timer_is_expired(cortex_timer))
        ;
}