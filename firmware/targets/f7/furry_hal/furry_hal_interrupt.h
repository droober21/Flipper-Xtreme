#pragma once

#include <stm32wbxx_ll_tim.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Timer ISR */
typedef void (*FurryHalInterruptISR)(void* context);

typedef enum {
    // TIM1, TIM16, TIM17
    FurryHalInterruptIdTim1TrgComTim17,
    FurryHalInterruptIdTim1Cc,
    FurryHalInterruptIdTim1UpTim16,

    // TIM2
    FurryHalInterruptIdTIM2,

    // DMA1
    FurryHalInterruptIdDma1Ch1,
    FurryHalInterruptIdDma1Ch2,
    FurryHalInterruptIdDma1Ch3,
    FurryHalInterruptIdDma1Ch4,
    FurryHalInterruptIdDma1Ch5,
    FurryHalInterruptIdDma1Ch6,
    FurryHalInterruptIdDma1Ch7,

    // DMA2
    FurryHalInterruptIdDma2Ch1,
    FurryHalInterruptIdDma2Ch2,
    FurryHalInterruptIdDma2Ch3,
    FurryHalInterruptIdDma2Ch4,
    FurryHalInterruptIdDma2Ch5,
    FurryHalInterruptIdDma2Ch6,
    FurryHalInterruptIdDma2Ch7,

    // RCC
    FurryHalInterruptIdRcc,

    // Comp
    FurryHalInterruptIdCOMP,

    // HSEM
    FurryHalInterruptIdHsem,

    // LPTIMx
    FurryHalInterruptIdLpTim1,
    FurryHalInterruptIdLpTim2,

    // Service value
    FurryHalInterruptIdMax,
} FurryHalInterruptId;

/** Initialize interrupt subsystem */
void furry_hal_interrupt_init();

/** Set ISR and enable interrupt with default priority
 * We don't clear interrupt flags for you, do it by your self.
 * @param index - interrupt ID
 * @param isr - your interrupt service routine or use NULL to clear
 * @param context - isr context
 */
void furry_hal_interrupt_set_isr(FurryHalInterruptId index, FurryHalInterruptISR isr, void* context);

/** Set ISR and enable interrupt with custom priority
 * We don't clear interrupt flags for you, do it by your self.
 * @param index - interrupt ID
 * @param priority - 0 to 15, 0 highest
 * @param isr - your interrupt service routine or use NULL to clear
 * @param context - isr context
 */
void furry_hal_interrupt_set_isr_ex(
    FurryHalInterruptId index,
    uint16_t priority,
    FurryHalInterruptISR isr,
    void* context);

#ifdef __cplusplus
}
#endif
