#include <furry_hal_interrupt.h>
#include <furry_hal_os.h>

#include <furry.h>

#include <stm32wbxx.h>
#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_cortex.h>

#define TAG "FurryHalInterrupt"

#define FURRY_HAL_INTERRUPT_DEFAULT_PRIORITY 5

typedef struct {
    FurryHalInterruptISR isr;
    void* context;
} FurryHalInterruptISRPair;

FurryHalInterruptISRPair furry_hal_interrupt_isr[FurryHalInterruptIdMax] = {0};

const IRQn_Type furry_hal_interrupt_irqn[FurryHalInterruptIdMax] = {
    // TIM1, TIM16, TIM17
    [FurryHalInterruptIdTim1TrgComTim17] = TIM1_TRG_COM_TIM17_IRQn,
    [FurryHalInterruptIdTim1Cc] = TIM1_CC_IRQn,
    [FurryHalInterruptIdTim1UpTim16] = TIM1_UP_TIM16_IRQn,

    // TIM2
    [FurryHalInterruptIdTIM2] = TIM2_IRQn,

    // DMA1
    [FurryHalInterruptIdDma1Ch1] = DMA1_Channel1_IRQn,
    [FurryHalInterruptIdDma1Ch2] = DMA1_Channel2_IRQn,
    [FurryHalInterruptIdDma1Ch3] = DMA1_Channel3_IRQn,
    [FurryHalInterruptIdDma1Ch4] = DMA1_Channel4_IRQn,
    [FurryHalInterruptIdDma1Ch5] = DMA1_Channel5_IRQn,
    [FurryHalInterruptIdDma1Ch6] = DMA1_Channel6_IRQn,
    [FurryHalInterruptIdDma1Ch7] = DMA1_Channel7_IRQn,

    // DMA2
    [FurryHalInterruptIdDma2Ch1] = DMA2_Channel1_IRQn,
    [FurryHalInterruptIdDma2Ch2] = DMA2_Channel2_IRQn,
    [FurryHalInterruptIdDma2Ch3] = DMA2_Channel3_IRQn,
    [FurryHalInterruptIdDma2Ch4] = DMA2_Channel4_IRQn,
    [FurryHalInterruptIdDma2Ch5] = DMA2_Channel5_IRQn,
    [FurryHalInterruptIdDma2Ch6] = DMA2_Channel6_IRQn,
    [FurryHalInterruptIdDma2Ch7] = DMA2_Channel7_IRQn,

    // RCC
    [FurryHalInterruptIdRcc] = RCC_IRQn,

    // COMP
    [FurryHalInterruptIdCOMP] = COMP_IRQn,

    // HSEM
    [FurryHalInterruptIdHsem] = HSEM_IRQn,

    // LPTIMx
    [FurryHalInterruptIdLpTim1] = LPTIM1_IRQn,
    [FurryHalInterruptIdLpTim2] = LPTIM2_IRQn,
};

__attribute__((always_inline)) static inline void
    furry_hal_interrupt_call(FurryHalInterruptId index) {
    furry_assert(furry_hal_interrupt_isr[index].isr);
    furry_hal_interrupt_isr[index].isr(furry_hal_interrupt_isr[index].context);
}

__attribute__((always_inline)) static inline void
    furry_hal_interrupt_enable(FurryHalInterruptId index, uint16_t priority) {
    NVIC_SetPriority(
        furry_hal_interrupt_irqn[index],
        NVIC_EncodePriority(NVIC_GetPriorityGrouping(), priority, 0));
    NVIC_EnableIRQ(furry_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void
    furry_hal_interrupt_clear_pending(FurryHalInterruptId index) {
    NVIC_ClearPendingIRQ(furry_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void
    furry_hal_interrupt_get_pending(FurryHalInterruptId index) {
    NVIC_GetPendingIRQ(furry_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void
    furry_hal_interrupt_set_pending(FurryHalInterruptId index) {
    NVIC_SetPendingIRQ(furry_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void
    furry_hal_interrupt_disable(FurryHalInterruptId index) {
    NVIC_DisableIRQ(furry_hal_interrupt_irqn[index]);
}

void furry_hal_interrupt_init() {
    NVIC_SetPriority(
        TAMP_STAMP_LSECSS_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_EnableIRQ(TAMP_STAMP_LSECSS_IRQn);

    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));

    NVIC_SetPriority(FPU_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_EnableIRQ(FPU_IRQn);

    LL_SYSCFG_DisableIT_FPU_IOC();
    LL_SYSCFG_DisableIT_FPU_DZC();
    LL_SYSCFG_DisableIT_FPU_UFC();
    LL_SYSCFG_DisableIT_FPU_OFC();
    LL_SYSCFG_DisableIT_FPU_IDC();
    LL_SYSCFG_DisableIT_FPU_IXC();

    LL_HANDLER_EnableFault(LL_HANDLER_FAULT_USG);
    LL_HANDLER_EnableFault(LL_HANDLER_FAULT_BUS);
    LL_HANDLER_EnableFault(LL_HANDLER_FAULT_MEM);

    FURRY_LOG_I(TAG, "Init OK");
}

void furry_hal_interrupt_set_isr(FurryHalInterruptId index, FurryHalInterruptISR isr, void* context) {
    furry_hal_interrupt_set_isr_ex(index, FURRY_HAL_INTERRUPT_DEFAULT_PRIORITY, isr, context);
}

void furry_hal_interrupt_set_isr_ex(
    FurryHalInterruptId index,
    uint16_t priority,
    FurryHalInterruptISR isr,
    void* context) {
    furry_assert(index < FurryHalInterruptIdMax);
    furry_assert(priority < 15);
    furry_assert(furry_hal_interrupt_irqn[index]);

    if(isr) {
        // Pre ISR set
        furry_assert(furry_hal_interrupt_isr[index].isr == NULL);
    } else {
        // Pre ISR clear
        furry_assert(furry_hal_interrupt_isr[index].isr != NULL);
        furry_hal_interrupt_disable(index);
        furry_hal_interrupt_clear_pending(index);
    }

    furry_hal_interrupt_isr[index].isr = isr;
    furry_hal_interrupt_isr[index].context = context;
    __DMB();

    if(isr) {
        // Post ISR set
        furry_hal_interrupt_clear_pending(index);
        furry_hal_interrupt_enable(index, priority);
    } else {
        // Post ISR clear
    }
}

/* Timer 2 */
void TIM2_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdTIM2);
}

/* Timer 1 Update */
void TIM1_UP_TIM16_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdTim1UpTim16);
}

void TIM1_TRG_COM_TIM17_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdTim1TrgComTim17);
}

void TIM1_CC_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdTim1Cc);
}

/* DMA 1 */
void DMA1_Channel1_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch1);
}

void DMA1_Channel2_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch2);
}

void DMA1_Channel3_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch3);
}

void DMA1_Channel4_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch4);
}

void DMA1_Channel5_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch5);
}

void DMA1_Channel6_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch6);
}

void DMA1_Channel7_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma1Ch7);
}

/* DMA 2 */
void DMA2_Channel1_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch1);
}

void DMA2_Channel2_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch2);
}

void DMA2_Channel3_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch3);
}

void DMA2_Channel4_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch4);
}

void DMA2_Channel5_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch5);
}

void DMA2_Channel6_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch6);
}

void DMA2_Channel7_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdDma2Ch7);
}

void HSEM_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdHsem);
}

void TAMP_STAMP_LSECSS_IRQHandler(void) {
    if(LL_RCC_IsActiveFlag_LSECSS()) {
        LL_RCC_ClearFlag_LSECSS();
        if(!LL_RCC_LSE_IsReady()) {
            FURRY_LOG_E(TAG, "LSE CSS fired: resetting system");
            NVIC_SystemReset();
        } else {
            FURRY_LOG_E(TAG, "LSE CSS fired: but LSE is alive");
        }
    }
}

void RCC_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdRcc);
}

void NMI_Handler() {
    if(LL_RCC_IsActiveFlag_HSECSS()) {
        LL_RCC_ClearFlag_HSECSS();
        FURRY_LOG_E(TAG, "HSE CSS fired: resetting system");
        NVIC_SystemReset();
    }
}

void HardFault_Handler() {
    furry_crash("HardFault");
}

void MemManage_Handler() {
    if(FURRY_BIT(SCB->CFSR, SCB_CFSR_MMARVALID_Pos)) {
        uint32_t memfault_address = SCB->MMFAR;
        if(memfault_address < (1024 * 1024)) {
            // from 0x00 to 1MB, see FurryHalMpuRegionNULL
            furry_crash("NULL pointer dereference");
        } else {
            // write or read of MPU region 1 (FurryHalMpuRegionStack)
            furry_crash("MPU fault, possibly stack overflow");
        }
    } else if(FURRY_BIT(SCB->CFSR, SCB_CFSR_MSTKERR_Pos)) {
        // push to stack on MPU region 1 (FurryHalMpuRegionStack)
        furry_crash("MemManage fault, possibly stack overflow");
    }

    furry_crash("MemManage");
}

void BusFault_Handler() {
    furry_crash("BusFault");
}

void UsageFault_Handler() {
    furry_crash("UsageFault");
}

void DebugMon_Handler() {
}

#include "usbd_core.h"

extern usbd_device udev;

extern void HW_IPCC_Tx_Handler();
extern void HW_IPCC_Rx_Handler();

void SysTick_Handler() {
    furry_hal_os_tick();
}

void USB_LP_IRQHandler() {
#ifndef FURRY_RAM_EXEC
    usbd_poll(&udev);
#endif
}

void USB_HP_IRQHandler() {
}

void IPCC_C1_TX_IRQHandler() {
    HW_IPCC_Tx_Handler();
}

void IPCC_C1_RX_IRQHandler() {
    HW_IPCC_Rx_Handler();
}

void FPU_IRQHandler() {
    furry_crash("FpuFault");
}

void LPTIM1_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdLpTim1);
}

void LPTIM2_IRQHandler() {
    furry_hal_interrupt_call(FurryHalInterruptIdLpTim2);
}
