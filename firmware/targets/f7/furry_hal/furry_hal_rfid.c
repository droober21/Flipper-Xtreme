#include <furry_hal_rfid.h>
#include <furry_hal_ibutton.h>
#include <furry_hal_interrupt.h>
#include <furry_hal_resources.h>
#include <furry.h>

#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_comp.h>
#include <stm32wbxx_ll_dma.h>

#define FURRY_HAL_RFID_READ_TIMER TIM1
#define FURRY_HAL_RFID_READ_TIMER_CHANNEL LL_TIM_CHANNEL_CH1N
// We can't use N channel for LL_TIM_OC_Init, so...
#define FURRY_HAL_RFID_READ_TIMER_CHANNEL_CONFIG LL_TIM_CHANNEL_CH1

#define FURRY_HAL_RFID_EMULATE_TIMER TIM2
#define FURRY_HAL_RFID_EMULATE_TIMER_IRQ FurryHalInterruptIdTIM2
#define FURRY_HAL_RFID_EMULATE_TIMER_CHANNEL LL_TIM_CHANNEL_CH3

#define RFID_CAPTURE_TIM TIM2
#define RFID_CAPTURE_IND_CH LL_TIM_CHANNEL_CH3
#define RFID_CAPTURE_DIR_CH LL_TIM_CHANNEL_CH4

/* DMA Channels definition */
#define RFID_DMA DMA2
#define RFID_DMA_CH1_CHANNEL LL_DMA_CHANNEL_1
#define RFID_DMA_CH2_CHANNEL LL_DMA_CHANNEL_2
#define RFID_DMA_CH1_IRQ FurryHalInterruptIdDma2Ch1
#define RFID_DMA_CH1_DEF RFID_DMA, RFID_DMA_CH1_CHANNEL
#define RFID_DMA_CH2_DEF RFID_DMA, RFID_DMA_CH2_CHANNEL

typedef struct {
    FurryHalRfidEmulateCallback callback;
    FurryHalRfidDMACallback dma_callback;
    FurryHalRfidReadCaptureCallback read_capture_callback;
    void* context;
} FurryHalRfid;

FurryHalRfid* furry_hal_rfid = NULL;

#define LFRFID_LL_READ_TIM TIM1
#define LFRFID_LL_READ_CONFIG_CHANNEL LL_TIM_CHANNEL_CH1
#define LFRFID_LL_READ_CHANNEL LL_TIM_CHANNEL_CH1N

#define LFRFID_LL_EMULATE_TIM TIM2
#define LFRFID_LL_EMULATE_CHANNEL LL_TIM_CHANNEL_CH3

void furry_hal_rfid_init() {
    furry_assert(furry_hal_rfid == NULL);
    furry_hal_rfid = malloc(sizeof(FurryHalRfid));

    furry_hal_rfid_pins_reset();

    LL_COMP_InitTypeDef COMP_InitStruct = {0};
    COMP_InitStruct.PowerMode = LL_COMP_POWERMODE_MEDIUMSPEED;
    COMP_InitStruct.InputPlus = LL_COMP_INPUT_PLUS_IO1;
    COMP_InitStruct.InputMinus = LL_COMP_INPUT_MINUS_1_2VREFINT;
    COMP_InitStruct.InputHysteresis = LL_COMP_HYSTERESIS_HIGH;
#ifdef INVERT_RFID_IN
    COMP_InitStruct.OutputPolarity = LL_COMP_OUTPUTPOL_INVERTED;
#else
    COMP_InitStruct.OutputPolarity = LL_COMP_OUTPUTPOL_NONINVERTED;
#endif
    COMP_InitStruct.OutputBlankingSource = LL_COMP_BLANKINGSRC_NONE;
    LL_COMP_Init(COMP1, &COMP_InitStruct);
    LL_COMP_SetCommonWindowMode(__LL_COMP_COMMON_INSTANCE(COMP1), LL_COMP_WINDOWMODE_DISABLE);

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_20);
    LL_EXTI_DisableEvent_0_31(LL_EXTI_LINE_20);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_20);

    NVIC_SetPriority(COMP_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(COMP_IRQn);
}

void furry_hal_rfid_pins_reset() {
    // ibutton bus disable
    furry_hal_ibutton_pin_reset();

    // pulldown rfid antenna
    furry_hal_gpio_init(&gpio_rfid_carrier_out, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_write(&gpio_rfid_carrier_out, false);

    // from both sides
    furry_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_write(&gpio_nfc_irq_rfid_pull, true);

    furry_hal_gpio_init_simple(&gpio_rfid_carrier, GpioModeAnalog);

    furry_hal_gpio_init(&gpio_rfid_data_in, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void furry_hal_rfid_pins_emulate() {
    // ibutton low
    furry_hal_ibutton_pin_configure();
    furry_hal_ibutton_pin_write(false);

    // pull pin to timer out
    furry_hal_gpio_init_ex(
        &gpio_nfc_irq_rfid_pull,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedLow,
        GpioAltFn1TIM2);

    // pull rfid antenna from carrier side
    furry_hal_gpio_init(&gpio_rfid_carrier_out, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_write(&gpio_rfid_carrier_out, false);

    furry_hal_gpio_init_ex(
        &gpio_rfid_carrier, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn2TIM2);
}

void furry_hal_rfid_pins_read() {
    // ibutton low
    furry_hal_ibutton_pin_configure();
    furry_hal_ibutton_pin_write(false);

    // dont pull rfid antenna
    furry_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_write(&gpio_nfc_irq_rfid_pull, false);

    // carrier pin to timer out
    furry_hal_gpio_init_ex(
        &gpio_rfid_carrier_out,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedLow,
        GpioAltFn1TIM1);

    // comparator in
    furry_hal_gpio_init(&gpio_rfid_data_in, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void furry_hal_rfid_pin_pull_release() {
    furry_hal_gpio_write(&gpio_nfc_irq_rfid_pull, true);
}

void furry_hal_rfid_pin_pull_pulldown() {
    furry_hal_gpio_write(&gpio_nfc_irq_rfid_pull, false);
}

void furry_hal_rfid_tim_read(float freq, float duty_cycle) {
    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(FURRY_HAL_RFID_READ_TIMER);
    FURRY_CRITICAL_EXIT();

    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Autoreload = (SystemCoreClock / freq) - 1;
    LL_TIM_Init(FURRY_HAL_RFID_READ_TIMER, &TIM_InitStruct);
    LL_TIM_DisableARRPreload(FURRY_HAL_RFID_READ_TIMER);

    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
    TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_InitStruct.CompareValue = TIM_InitStruct.Autoreload * duty_cycle;
    LL_TIM_OC_Init(
        FURRY_HAL_RFID_READ_TIMER, FURRY_HAL_RFID_READ_TIMER_CHANNEL_CONFIG, &TIM_OC_InitStruct);

    LL_TIM_EnableCounter(FURRY_HAL_RFID_READ_TIMER);
}

void furry_hal_rfid_tim_read_start() {
    LL_TIM_EnableAllOutputs(FURRY_HAL_RFID_READ_TIMER);
}

void furry_hal_rfid_tim_read_stop() {
    LL_TIM_DisableAllOutputs(FURRY_HAL_RFID_READ_TIMER);
}

void furry_hal_rfid_tim_emulate(float freq) {
    UNUSED(freq); // FIXME
    // basic PWM setup with needed freq and internal clock
    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(FURRY_HAL_RFID_EMULATE_TIMER);
    FURRY_CRITICAL_EXIT();

    LL_TIM_SetPrescaler(FURRY_HAL_RFID_EMULATE_TIMER, 0);
    LL_TIM_SetCounterMode(FURRY_HAL_RFID_EMULATE_TIMER, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(FURRY_HAL_RFID_EMULATE_TIMER, 1);
    LL_TIM_DisableARRPreload(FURRY_HAL_RFID_EMULATE_TIMER);
    LL_TIM_SetRepetitionCounter(FURRY_HAL_RFID_EMULATE_TIMER, 0);

    LL_TIM_SetClockDivision(FURRY_HAL_RFID_EMULATE_TIMER, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetClockSource(FURRY_HAL_RFID_EMULATE_TIMER, LL_TIM_CLOCKSOURCE_EXT_MODE2);
    LL_TIM_ConfigETR(
        FURRY_HAL_RFID_EMULATE_TIMER,
        LL_TIM_ETR_POLARITY_INVERTED,
        LL_TIM_ETR_PRESCALER_DIV1,
        LL_TIM_ETR_FILTER_FDIV1);

    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_InitStruct.CompareValue = 1;
    LL_TIM_OC_Init(
        FURRY_HAL_RFID_EMULATE_TIMER, FURRY_HAL_RFID_EMULATE_TIMER_CHANNEL, &TIM_OC_InitStruct);

    LL_TIM_GenerateEvent_UPDATE(FURRY_HAL_RFID_EMULATE_TIMER);
}

static void furry_hal_rfid_emulate_isr() {
    if(LL_TIM_IsActiveFlag_UPDATE(FURRY_HAL_RFID_EMULATE_TIMER)) {
        LL_TIM_ClearFlag_UPDATE(FURRY_HAL_RFID_EMULATE_TIMER);
        furry_hal_rfid->callback(furry_hal_rfid->context);
    }
}

void furry_hal_rfid_tim_emulate_start(FurryHalRfidEmulateCallback callback, void* context) {
    furry_assert(furry_hal_rfid);

    furry_hal_rfid->callback = callback;
    furry_hal_rfid->context = context;

    furry_hal_interrupt_set_isr(FURRY_HAL_RFID_EMULATE_TIMER_IRQ, furry_hal_rfid_emulate_isr, NULL);

    LL_TIM_EnableIT_UPDATE(FURRY_HAL_RFID_EMULATE_TIMER);
    LL_TIM_EnableAllOutputs(FURRY_HAL_RFID_EMULATE_TIMER);
    LL_TIM_EnableCounter(FURRY_HAL_RFID_EMULATE_TIMER);
}

void furry_hal_rfid_tim_emulate_stop() {
    LL_TIM_DisableCounter(FURRY_HAL_RFID_EMULATE_TIMER);
    LL_TIM_DisableAllOutputs(FURRY_HAL_RFID_EMULATE_TIMER);
    furry_hal_interrupt_set_isr(FURRY_HAL_RFID_EMULATE_TIMER_IRQ, NULL, NULL);
}

static void furry_hal_capture_dma_isr(void* context) {
    UNUSED(context);

    // Channel 3, positive level
    if(LL_TIM_IsActiveFlag_CC3(RFID_CAPTURE_TIM)) {
        LL_TIM_ClearFlag_CC3(RFID_CAPTURE_TIM);
        furry_hal_rfid->read_capture_callback(
            true, LL_TIM_IC_GetCaptureCH3(RFID_CAPTURE_TIM), furry_hal_rfid->context);
    }

    // Channel 4, overall level
    if(LL_TIM_IsActiveFlag_CC4(RFID_CAPTURE_TIM)) {
        LL_TIM_ClearFlag_CC4(RFID_CAPTURE_TIM);
        LL_TIM_SetCounter(RFID_CAPTURE_TIM, 0);
        furry_hal_rfid->read_capture_callback(
            false, LL_TIM_IC_GetCaptureCH4(RFID_CAPTURE_TIM), furry_hal_rfid->context);
    }
}

void furry_hal_rfid_tim_read_capture_start(FurryHalRfidReadCaptureCallback callback, void* context) {
    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(RFID_CAPTURE_TIM);
    FURRY_CRITICAL_EXIT();

    furry_assert(furry_hal_rfid);

    furry_hal_rfid->read_capture_callback = callback;
    furry_hal_rfid->context = context;

    // Timer: base
    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Prescaler = 64 - 1;
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload = UINT32_MAX;
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(RFID_CAPTURE_TIM, &TIM_InitStruct);

    // Timer: advanced
    LL_TIM_SetClockSource(RFID_CAPTURE_TIM, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(RFID_CAPTURE_TIM);
    LL_TIM_SetTriggerInput(RFID_CAPTURE_TIM, LL_TIM_TS_TI2FP2);
    LL_TIM_SetSlaveMode(RFID_CAPTURE_TIM, LL_TIM_SLAVEMODE_DISABLED);
    LL_TIM_SetTriggerOutput(RFID_CAPTURE_TIM, LL_TIM_TRGO_RESET);
    LL_TIM_EnableMasterSlaveMode(RFID_CAPTURE_TIM);
    LL_TIM_DisableDMAReq_TRIG(RFID_CAPTURE_TIM);
    LL_TIM_DisableIT_TRIG(RFID_CAPTURE_TIM);
    LL_TIM_SetRemap(RFID_CAPTURE_TIM, LL_TIM_TIM2_TI4_RMP_COMP1);

    // Timer: channel 3 indirect
    LL_TIM_IC_SetActiveInput(RFID_CAPTURE_TIM, RFID_CAPTURE_IND_CH, LL_TIM_ACTIVEINPUT_INDIRECTTI);
    LL_TIM_IC_SetPrescaler(RFID_CAPTURE_TIM, RFID_CAPTURE_IND_CH, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetPolarity(RFID_CAPTURE_TIM, RFID_CAPTURE_IND_CH, LL_TIM_IC_POLARITY_FALLING);
    LL_TIM_IC_SetFilter(RFID_CAPTURE_TIM, RFID_CAPTURE_IND_CH, LL_TIM_IC_FILTER_FDIV1);

    // Timer: channel 4 direct
    LL_TIM_IC_SetActiveInput(RFID_CAPTURE_TIM, RFID_CAPTURE_DIR_CH, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(RFID_CAPTURE_TIM, RFID_CAPTURE_DIR_CH, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetPolarity(RFID_CAPTURE_TIM, RFID_CAPTURE_DIR_CH, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetFilter(RFID_CAPTURE_TIM, RFID_CAPTURE_DIR_CH, LL_TIM_IC_FILTER_FDIV1);

    furry_hal_interrupt_set_isr(FURRY_HAL_RFID_EMULATE_TIMER_IRQ, furry_hal_capture_dma_isr, NULL);

    LL_TIM_EnableIT_CC3(RFID_CAPTURE_TIM);
    LL_TIM_EnableIT_CC4(RFID_CAPTURE_TIM);
    LL_TIM_CC_EnableChannel(RFID_CAPTURE_TIM, RFID_CAPTURE_IND_CH);
    LL_TIM_CC_EnableChannel(RFID_CAPTURE_TIM, RFID_CAPTURE_DIR_CH);
    LL_TIM_SetCounter(RFID_CAPTURE_TIM, 0);
    LL_TIM_EnableCounter(RFID_CAPTURE_TIM);

    furry_hal_rfid_comp_start();
}

void furry_hal_rfid_tim_read_capture_stop() {
    furry_hal_rfid_comp_stop();

    furry_hal_interrupt_set_isr(FURRY_HAL_RFID_EMULATE_TIMER_IRQ, NULL, NULL);

    FURRY_CRITICAL_ENTER();
    LL_TIM_DeInit(RFID_CAPTURE_TIM);
    FURRY_CRITICAL_EXIT();
}

static void furry_hal_rfid_dma_isr() {
#if RFID_DMA_CH1_CHANNEL == LL_DMA_CHANNEL_1
    if(LL_DMA_IsActiveFlag_HT1(RFID_DMA)) {
        LL_DMA_ClearFlag_HT1(RFID_DMA);
        furry_hal_rfid->dma_callback(true, furry_hal_rfid->context);
    }

    if(LL_DMA_IsActiveFlag_TC1(RFID_DMA)) {
        LL_DMA_ClearFlag_TC1(RFID_DMA);
        furry_hal_rfid->dma_callback(false, furry_hal_rfid->context);
    }
#else
#error Update this code. Would you kindly?
#endif
}

void furry_hal_rfid_tim_emulate_dma_start(
    uint32_t* duration,
    uint32_t* pulse,
    size_t length,
    FurryHalRfidDMACallback callback,
    void* context) {
    furry_assert(furry_hal_rfid);

    // setup interrupts
    furry_hal_rfid->dma_callback = callback;
    furry_hal_rfid->context = context;

    // setup pins
    furry_hal_rfid_pins_emulate();

    // configure timer
    furry_hal_rfid_tim_emulate(125000);
    LL_TIM_OC_SetPolarity(
        FURRY_HAL_RFID_EMULATE_TIMER, FURRY_HAL_RFID_EMULATE_TIMER_CHANNEL, LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_EnableDMAReq_UPDATE(FURRY_HAL_RFID_EMULATE_TIMER);

    // configure DMA "mem -> ARR" channel
    LL_DMA_InitTypeDef dma_config = {0};
    dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (FURRY_HAL_RFID_EMULATE_TIMER->ARR);
    dma_config.MemoryOrM2MDstAddress = (uint32_t)duration;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_CIRCULAR;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_config.NbData = length;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM2_UP;
    dma_config.Priority = LL_DMA_MODE_NORMAL;
    LL_DMA_Init(RFID_DMA_CH1_DEF, &dma_config);
    LL_DMA_EnableChannel(RFID_DMA_CH1_DEF);

    // configure DMA "mem -> CCR3" channel
#if FURRY_HAL_RFID_EMULATE_TIMER_CHANNEL == LL_TIM_CHANNEL_CH3
    dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (FURRY_HAL_RFID_EMULATE_TIMER->CCR3);
#else
#error Update this code. Would you kindly?
#endif
    dma_config.MemoryOrM2MDstAddress = (uint32_t)pulse;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_CIRCULAR;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_config.NbData = length;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM2_UP;
    dma_config.Priority = LL_DMA_MODE_NORMAL;
    LL_DMA_Init(RFID_DMA_CH2_DEF, &dma_config);
    LL_DMA_EnableChannel(RFID_DMA_CH2_DEF);

    // attach interrupt to one of DMA channels
    furry_hal_interrupt_set_isr(RFID_DMA_CH1_IRQ, furry_hal_rfid_dma_isr, NULL);
    LL_DMA_EnableIT_TC(RFID_DMA_CH1_DEF);
    LL_DMA_EnableIT_HT(RFID_DMA_CH1_DEF);

    // start
    LL_TIM_EnableAllOutputs(FURRY_HAL_RFID_EMULATE_TIMER);

    LL_TIM_SetCounter(FURRY_HAL_RFID_EMULATE_TIMER, 0);
    LL_TIM_EnableCounter(FURRY_HAL_RFID_EMULATE_TIMER);
}

void furry_hal_rfid_tim_emulate_dma_stop() {
    LL_TIM_DisableCounter(FURRY_HAL_RFID_EMULATE_TIMER);
    LL_TIM_DisableAllOutputs(FURRY_HAL_RFID_EMULATE_TIMER);

    furry_hal_interrupt_set_isr(RFID_DMA_CH1_IRQ, NULL, NULL);
    LL_DMA_DisableIT_TC(RFID_DMA_CH1_DEF);
    LL_DMA_DisableIT_HT(RFID_DMA_CH1_DEF);

    FURRY_CRITICAL_ENTER();

    LL_DMA_DeInit(RFID_DMA_CH1_DEF);
    LL_DMA_DeInit(RFID_DMA_CH2_DEF);
    LL_TIM_DeInit(FURRY_HAL_RFID_EMULATE_TIMER);

    FURRY_CRITICAL_EXIT();
}

void furry_hal_rfid_tim_reset() {
    FURRY_CRITICAL_ENTER();

    LL_TIM_DeInit(FURRY_HAL_RFID_READ_TIMER);
    LL_TIM_DeInit(FURRY_HAL_RFID_EMULATE_TIMER);

    FURRY_CRITICAL_EXIT();
}

void furry_hal_rfid_set_emulate_period(uint32_t period) {
    LL_TIM_SetAutoReload(FURRY_HAL_RFID_EMULATE_TIMER, period);
}

void furry_hal_rfid_set_emulate_pulse(uint32_t pulse) {
#if FURRY_HAL_RFID_EMULATE_TIMER_CHANNEL == LL_TIM_CHANNEL_CH3
    LL_TIM_OC_SetCompareCH3(FURRY_HAL_RFID_EMULATE_TIMER, pulse);
#else
#error Update this code. Would you kindly?
#endif
}

void furry_hal_rfid_set_read_period(uint32_t period) {
    LL_TIM_SetAutoReload(FURRY_HAL_RFID_READ_TIMER, period);
}

void furry_hal_rfid_set_read_pulse(uint32_t pulse) {
#if FURRY_HAL_RFID_READ_TIMER_CHANNEL == LL_TIM_CHANNEL_CH1N
    LL_TIM_OC_SetCompareCH1(FURRY_HAL_RFID_READ_TIMER, pulse);
#else
#error Update this code. Would you kindly?
#endif
}

void furry_hal_rfid_change_read_config(float freq, float duty_cycle) {
    uint32_t period = (uint32_t)((SystemCoreClock) / freq) - 1;
    furry_hal_rfid_set_read_period(period);
    furry_hal_rfid_set_read_pulse(period * duty_cycle);
}

void furry_hal_rfid_comp_start() {
    LL_COMP_Enable(COMP1);
    // Magic
    uint32_t wait_loop_index = ((80 / 10UL) * ((SystemCoreClock / (100000UL * 2UL)) + 1UL));
    while(wait_loop_index) {
        wait_loop_index--;
    }
}

void furry_hal_rfid_comp_stop() {
    LL_COMP_Disable(COMP1);
}

FurryHalRfidCompCallback furry_hal_rfid_comp_callback = NULL;
void* furry_hal_rfid_comp_callback_context = NULL;

void furry_hal_rfid_comp_set_callback(FurryHalRfidCompCallback callback, void* context) {
    FURRY_CRITICAL_ENTER();
    furry_hal_rfid_comp_callback = callback;
    furry_hal_rfid_comp_callback_context = context;
    __DMB();
    FURRY_CRITICAL_EXIT();
}

/* Comparator trigger event */
void COMP_IRQHandler() {
    if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_20)) {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_20);
    }
    if(furry_hal_rfid_comp_callback) {
        furry_hal_rfid_comp_callback(
            (LL_COMP_ReadOutputLevel(COMP1) == LL_COMP_OUTPUT_LEVEL_LOW),
            furry_hal_rfid_comp_callback_context);
    }
}