#include <furry_hal_infrared.h>
#include <core/check.h>
#include "stm32wbxx_ll_dma.h"
#include "sys/_stdint.h"
#include <furry_hal_interrupt.h>
#include <furry_hal_resources.h>

#include <stdint.h>
#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_gpio.h>

#include <stdio.h>
#include <furry.h>
#include <math.h>

#define INFRARED_TIM_TX_DMA_BUFFER_SIZE 200
#define INFRARED_POLARITY_SHIFT 1

#define INFRARED_TX_CCMR_HIGH \
    (TIM_CCMR2_OC3PE | LL_TIM_OCMODE_PWM2) /* Mark time - enable PWM2 mode */
#define INFRARED_TX_CCMR_LOW \
    (TIM_CCMR2_OC3PE | LL_TIM_OCMODE_FORCED_INACTIVE) /* Space time - force low */

/* DMA Channels definition */
#define IR_DMA DMA2
#define IR_DMA_CH1_CHANNEL LL_DMA_CHANNEL_1
#define IR_DMA_CH2_CHANNEL LL_DMA_CHANNEL_2
#define IR_DMA_CH1_IRQ FurryHalInterruptIdDma2Ch1
#define IR_DMA_CH2_IRQ FurryHalInterruptIdDma2Ch2
#define IR_DMA_CH1_DEF IR_DMA, IR_DMA_CH1_CHANNEL
#define IR_DMA_CH2_DEF IR_DMA, IR_DMA_CH2_CHANNEL

typedef struct {
    FurryHalInfraredRxCaptureCallback capture_callback;
    void* capture_context;
    FurryHalInfraredRxTimeoutCallback timeout_callback;
    void* timeout_context;
} InfraredTimRx;

typedef struct {
    uint8_t* polarity;
    uint16_t* data;
    size_t size;
    bool packet_end;
    bool last_packet_end;
} InfraredTxBuf;

typedef struct {
    float cycle_duration;
    FurryHalInfraredTxGetDataISRCallback data_callback;
    FurryHalInfraredTxSignalSentISRCallback signal_sent_callback;
    void* data_context;
    void* signal_sent_context;
    InfraredTxBuf buffer[2];
    FurrySemaphore* stop_semaphore;
    uint32_t
        tx_timing_rest_duration; /** if timing is too long (> 0xFFFF), send it in few iterations */
    bool tx_timing_rest_level;
    FurryHalInfraredTxGetDataState tx_timing_rest_status;
} InfraredTimTx;

typedef enum {
    InfraredStateIdle, /** Furry Hal Infrared is ready to start RX or TX */
    InfraredStateAsyncRx, /** Async RX started */
    InfraredStateAsyncTx, /** Async TX started, DMA and timer is on */
    InfraredStateAsyncTxStopReq, /** Async TX started, async stop request received */
    InfraredStateAsyncTxStopInProgress, /** Async TX started, stop request is processed and we wait for last data to be sent */
    InfraredStateAsyncTxStopped, /** Async TX complete, cleanup needed */
    InfraredStateMAX,
} InfraredState;

static volatile InfraredState furry_hal_infrared_state = InfraredStateIdle;
static InfraredTimTx infrared_tim_tx;
static InfraredTimRx infrared_tim_rx;
static bool infrared_external_output;

static void furry_hal_infrared_tx_fill_buffer(uint8_t buf_num, uint8_t polarity_shift);
static void furry_hal_infrared_async_tx_free_resources(void);
static void furry_hal_infrared_tx_dma_set_polarity(uint8_t buf_num, uint8_t polarity_shift);
static void furry_hal_infrared_tx_dma_set_buffer(uint8_t buf_num);
static void furry_hal_infrared_tx_fill_buffer_last(uint8_t buf_num);
static uint8_t furry_hal_infrared_get_current_dma_tx_buffer(void);
static void furry_hal_infrared_tx_dma_polarity_isr();
static void furry_hal_infrared_tx_dma_isr();

void furry_hal_infrared_set_debug_out(bool enable) {
    infrared_external_output = enable;
}

bool furry_hal_infrared_get_debug_out_status(void) {
    return infrared_external_output;
}

static void furry_hal_infrared_tim_rx_isr() {
    static uint32_t previous_captured_ch2 = 0;

    /* Timeout */
    if(LL_TIM_IsActiveFlag_CC3(TIM2)) {
        LL_TIM_ClearFlag_CC3(TIM2);
        furry_assert(furry_hal_infrared_state == InfraredStateAsyncRx);

        /* Timers CNT register starts to counting from 0 to ARR, but it is
         * reseted when Channel 1 catches interrupt. It is not reseted by
         * channel 2, though, so we have to distract it's values (see TimerIRQSourceCCI1 ISR).
         * This can cause false timeout: when time is over, but we started
         * receiving new signal few microseconds ago, because CNT register
         * is reseted once per period, not per sample. */
        if(LL_GPIO_IsInputPinSet(gpio_infrared_rx.port, gpio_infrared_rx.pin) != 0) {
            if(infrared_tim_rx.timeout_callback)
                infrared_tim_rx.timeout_callback(infrared_tim_rx.timeout_context);
        }
    }

    /* Rising Edge */
    if(LL_TIM_IsActiveFlag_CC1(TIM2)) {
        LL_TIM_ClearFlag_CC1(TIM2);
        furry_assert(furry_hal_infrared_state == InfraredStateAsyncRx);

        if(READ_BIT(TIM2->CCMR1, TIM_CCMR1_CC1S)) {
            /* Low pin level is a Mark state of INFRARED signal. Invert level for further processing. */
            uint32_t duration = LL_TIM_IC_GetCaptureCH1(TIM2) - previous_captured_ch2;
            if(infrared_tim_rx.capture_callback)
                infrared_tim_rx.capture_callback(infrared_tim_rx.capture_context, 1, duration);
        } else {
            furry_assert(0);
        }
    }

    /* Falling Edge */
    if(LL_TIM_IsActiveFlag_CC2(TIM2)) {
        LL_TIM_ClearFlag_CC2(TIM2);
        furry_assert(furry_hal_infrared_state == InfraredStateAsyncRx);

        if(READ_BIT(TIM2->CCMR1, TIM_CCMR1_CC2S)) {
            /* High pin level is a Space state of INFRARED signal. Invert level for further processing. */
            uint32_t duration = LL_TIM_IC_GetCaptureCH2(TIM2);
            previous_captured_ch2 = duration;
            if(infrared_tim_rx.capture_callback)
                infrared_tim_rx.capture_callback(infrared_tim_rx.capture_context, 0, duration);
        } else {
            furry_assert(0);
        }
    }
}

void furry_hal_infrared_async_rx_start(void) {
    furry_assert(furry_hal_infrared_state == InfraredStateIdle);

    furry_hal_gpio_init_ex(
        &gpio_infrared_rx, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedLow, GpioAltFn1TIM2);

    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Prescaler = 64 - 1;
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload = 0x7FFFFFFE;
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(TIM2, &TIM_InitStruct);

    LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM2);
    LL_TIM_SetTriggerInput(TIM2, LL_TIM_TS_TI1FP1);
    LL_TIM_SetSlaveMode(TIM2, LL_TIM_SLAVEMODE_RESET);
    LL_TIM_CC_DisableChannel(TIM2, LL_TIM_CHANNEL_CH2);
    LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_FALLING);
    LL_TIM_DisableIT_TRIG(TIM2);
    LL_TIM_DisableDMAReq_TRIG(TIM2);
    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
    LL_TIM_EnableMasterSlaveMode(TIM2);
    LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_INDIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ICPSC_DIV1);

    furry_hal_interrupt_set_isr(FurryHalInterruptIdTIM2, furry_hal_infrared_tim_rx_isr, NULL);
    furry_hal_infrared_state = InfraredStateAsyncRx;

    LL_TIM_EnableIT_CC1(TIM2);
    LL_TIM_EnableIT_CC2(TIM2);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);

    LL_TIM_SetCounter(TIM2, 0);
    LL_TIM_EnableCounter(TIM2);
}

void furry_hal_infrared_async_rx_stop(void) {
    furry_assert(furry_hal_infrared_state == InfraredStateAsyncRx);

    FURRY_CRITICAL_ENTER();

    LL_TIM_DeInit(TIM2);
    furry_hal_interrupt_set_isr(FurryHalInterruptIdTIM2, NULL, NULL);
    furry_hal_infrared_state = InfraredStateIdle;

    FURRY_CRITICAL_EXIT();
}

void furry_hal_infrared_async_rx_set_timeout(uint32_t timeout_us) {
    LL_TIM_OC_SetCompareCH3(TIM2, timeout_us);
    LL_TIM_OC_SetMode(TIM2, LL_TIM_CHANNEL_CH3, LL_TIM_OCMODE_ACTIVE);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH3);
    LL_TIM_EnableIT_CC3(TIM2);
}

bool furry_hal_infrared_is_busy(void) {
    return furry_hal_infrared_state != InfraredStateIdle;
}

void furry_hal_infrared_async_rx_set_capture_isr_callback(
    FurryHalInfraredRxCaptureCallback callback,
    void* ctx) {
    infrared_tim_rx.capture_callback = callback;
    infrared_tim_rx.capture_context = ctx;
}

void furry_hal_infrared_async_rx_set_timeout_isr_callback(
    FurryHalInfraredRxTimeoutCallback callback,
    void* ctx) {
    infrared_tim_rx.timeout_callback = callback;
    infrared_tim_rx.timeout_context = ctx;
}

static void furry_hal_infrared_tx_dma_terminate(void) {
    LL_DMA_DisableIT_TC(IR_DMA_CH1_DEF);
    LL_DMA_DisableIT_HT(IR_DMA_CH2_DEF);
    LL_DMA_DisableIT_TC(IR_DMA_CH2_DEF);

    furry_assert(furry_hal_infrared_state == InfraredStateAsyncTxStopInProgress);

    LL_DMA_DisableIT_TC(IR_DMA_CH1_DEF);
    LL_DMA_DisableChannel(IR_DMA_CH2_DEF);
    LL_DMA_DisableChannel(IR_DMA_CH1_DEF);
    LL_TIM_DisableCounter(TIM1);
    FurryStatus status = furry_semaphore_release(infrared_tim_tx.stop_semaphore);
    furry_check(status == FurryStatusOk);
    furry_hal_infrared_state = InfraredStateAsyncTxStopped;
}

static uint8_t furry_hal_infrared_get_current_dma_tx_buffer(void) {
    uint8_t buf_num = 0;
    uint32_t buffer_adr = LL_DMA_GetMemoryAddress(IR_DMA_CH2_DEF);
    if(buffer_adr == (uint32_t)infrared_tim_tx.buffer[0].data) {
        buf_num = 0;
    } else if(buffer_adr == (uint32_t)infrared_tim_tx.buffer[1].data) {
        buf_num = 1;
    } else {
        furry_assert(0);
    }
    return buf_num;
}

static void furry_hal_infrared_tx_dma_polarity_isr() {
#if IR_DMA_CH1_CHANNEL == LL_DMA_CHANNEL_1
    if(LL_DMA_IsActiveFlag_TE1(IR_DMA)) {
        LL_DMA_ClearFlag_TE1(IR_DMA);
        furry_crash(NULL);
    }
    if(LL_DMA_IsActiveFlag_TC1(IR_DMA) && LL_DMA_IsEnabledIT_TC(IR_DMA_CH1_DEF)) {
        LL_DMA_ClearFlag_TC1(IR_DMA);

        furry_check(
            (furry_hal_infrared_state == InfraredStateAsyncTx) ||
            (furry_hal_infrared_state == InfraredStateAsyncTxStopReq) ||
            (furry_hal_infrared_state == InfraredStateAsyncTxStopInProgress));
        /* actually TC2 is processed and buffer is next buffer */
        uint8_t next_buf_num = furry_hal_infrared_get_current_dma_tx_buffer();
        furry_hal_infrared_tx_dma_set_polarity(next_buf_num, 0);
    }
#else
#error Update this code. Would you kindly?
#endif
}

static void furry_hal_infrared_tx_dma_isr() {
#if IR_DMA_CH2_CHANNEL == LL_DMA_CHANNEL_2
    if(LL_DMA_IsActiveFlag_TE2(IR_DMA)) {
        LL_DMA_ClearFlag_TE2(IR_DMA);
        furry_crash(NULL);
    }
    if(LL_DMA_IsActiveFlag_HT2(IR_DMA) && LL_DMA_IsEnabledIT_HT(IR_DMA_CH2_DEF)) {
        LL_DMA_ClearFlag_HT2(IR_DMA);
        uint8_t buf_num = furry_hal_infrared_get_current_dma_tx_buffer();
        uint8_t next_buf_num = !buf_num;
        if(infrared_tim_tx.buffer[buf_num].last_packet_end) {
            LL_DMA_DisableIT_HT(IR_DMA_CH2_DEF);
        } else if(
            !infrared_tim_tx.buffer[buf_num].packet_end ||
            (furry_hal_infrared_state == InfraredStateAsyncTx)) {
            furry_hal_infrared_tx_fill_buffer(next_buf_num, 0);
            if(infrared_tim_tx.buffer[next_buf_num].last_packet_end) {
                LL_DMA_DisableIT_HT(IR_DMA_CH2_DEF);
            }
        } else if(furry_hal_infrared_state == InfraredStateAsyncTxStopReq) {
            /* fallthrough */
        } else {
            furry_crash(NULL);
        }
    }
    if(LL_DMA_IsActiveFlag_TC2(IR_DMA) && LL_DMA_IsEnabledIT_TC(IR_DMA_CH2_DEF)) {
        LL_DMA_ClearFlag_TC2(IR_DMA);
        furry_check(
            (furry_hal_infrared_state == InfraredStateAsyncTxStopInProgress) ||
            (furry_hal_infrared_state == InfraredStateAsyncTxStopReq) ||
            (furry_hal_infrared_state == InfraredStateAsyncTx));

        uint8_t buf_num = furry_hal_infrared_get_current_dma_tx_buffer();
        uint8_t next_buf_num = !buf_num;
        if(furry_hal_infrared_state == InfraredStateAsyncTxStopInProgress) {
            furry_hal_infrared_tx_dma_terminate();
        } else if(
            infrared_tim_tx.buffer[buf_num].last_packet_end ||
            (infrared_tim_tx.buffer[buf_num].packet_end &&
             (furry_hal_infrared_state == InfraredStateAsyncTxStopReq))) {
            furry_hal_infrared_state = InfraredStateAsyncTxStopInProgress;
            furry_hal_infrared_tx_fill_buffer_last(next_buf_num);
            furry_hal_infrared_tx_dma_set_buffer(next_buf_num);
        } else {
            /* if it's not end of the packet - continue receiving */
            furry_hal_infrared_tx_dma_set_buffer(next_buf_num);
        }
        if(infrared_tim_tx.signal_sent_callback && infrared_tim_tx.buffer[buf_num].packet_end &&
           (furry_hal_infrared_state != InfraredStateAsyncTxStopped)) {
            infrared_tim_tx.signal_sent_callback(infrared_tim_tx.signal_sent_context);
        }
    }
#else
#error Update this code. Would you kindly?
#endif
}

static void furry_hal_infrared_configure_tim_pwm_tx(uint32_t freq, float duty_cycle) {
    /*    LL_DBGMCU_APB2_GRP1_FreezePeriph(LL_DBGMCU_APB2_GRP1_TIM1_STOP); */

    LL_TIM_DisableCounter(TIM1);
    LL_TIM_SetRepetitionCounter(TIM1, 0);
    LL_TIM_SetCounter(TIM1, 0);
    LL_TIM_SetPrescaler(TIM1, 0);
    LL_TIM_SetCounterMode(TIM1, LL_TIM_COUNTERMODE_UP);
    LL_TIM_EnableARRPreload(TIM1);
    LL_TIM_SetAutoReload(
        TIM1, __LL_TIM_CALC_ARR(SystemCoreClock, LL_TIM_GetPrescaler(TIM1), freq));
    if(infrared_external_output) {
        LL_TIM_OC_SetCompareCH1(TIM1, ((LL_TIM_GetAutoReload(TIM1) + 1) * (1 - duty_cycle)));
        LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH1);
        /* LL_TIM_OCMODE_PWM2 set by DMA */
        LL_TIM_OC_SetMode(TIM1, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_FORCED_INACTIVE);
        LL_TIM_OC_SetPolarity(TIM1, LL_TIM_CHANNEL_CH1N, LL_TIM_OCPOLARITY_HIGH);
        LL_TIM_OC_DisableFast(TIM1, LL_TIM_CHANNEL_CH1);
        LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1N);
        LL_TIM_DisableIT_CC1(TIM1);
    } else {
        LL_TIM_OC_SetCompareCH3(TIM1, ((LL_TIM_GetAutoReload(TIM1) + 1) * (1 - duty_cycle)));
        LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH3);
        /* LL_TIM_OCMODE_PWM2 set by DMA */
        LL_TIM_OC_SetMode(TIM1, LL_TIM_CHANNEL_CH3, LL_TIM_OCMODE_FORCED_INACTIVE);
        LL_TIM_OC_SetPolarity(TIM1, LL_TIM_CHANNEL_CH3N, LL_TIM_OCPOLARITY_HIGH);
        LL_TIM_OC_DisableFast(TIM1, LL_TIM_CHANNEL_CH3);
        LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH3N);
        LL_TIM_DisableIT_CC3(TIM1);
    }
    LL_TIM_DisableMasterSlaveMode(TIM1);
    LL_TIM_EnableAllOutputs(TIM1);
    LL_TIM_DisableIT_UPDATE(TIM1);
    LL_TIM_EnableDMAReq_UPDATE(TIM1);

    NVIC_SetPriority(TIM1_UP_TIM16_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
}

static void furry_hal_infrared_configure_tim_cmgr2_dma_tx(void) {
    LL_DMA_InitTypeDef dma_config = {0};
    if(infrared_external_output) {
        dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (TIM1->CCMR1);
    } else {
        dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (TIM1->CCMR2);
    }
    dma_config.MemoryOrM2MDstAddress = (uint32_t)NULL;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_NORMAL;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    /* fill word to have other bits set to 0 */
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
    dma_config.NbData = 0;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM1_UP;
    dma_config.Priority = LL_DMA_PRIORITY_VERYHIGH;
    LL_DMA_Init(IR_DMA_CH1_DEF, &dma_config);

#if IR_DMA_CH1_CHANNEL == LL_DMA_CHANNEL_1
    LL_DMA_ClearFlag_TE1(IR_DMA);
    LL_DMA_ClearFlag_TC1(IR_DMA);
#else
#error Update this code. Would you kindly?
#endif

    LL_DMA_EnableIT_TE(IR_DMA_CH1_DEF);
    LL_DMA_EnableIT_TC(IR_DMA_CH1_DEF);

    furry_hal_interrupt_set_isr_ex(IR_DMA_CH1_IRQ, 4, furry_hal_infrared_tx_dma_polarity_isr, NULL);
}

static void furry_hal_infrared_configure_tim_rcr_dma_tx(void) {
    LL_DMA_InitTypeDef dma_config = {0};
    dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (TIM1->RCR);
    dma_config.MemoryOrM2MDstAddress = (uint32_t)NULL;
    dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_config.Mode = LL_DMA_MODE_NORMAL;
    dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
    dma_config.NbData = 0;
    dma_config.PeriphRequest = LL_DMAMUX_REQ_TIM1_UP;
    dma_config.Priority = LL_DMA_PRIORITY_MEDIUM;
    LL_DMA_Init(IR_DMA_CH2_DEF, &dma_config);

#if IR_DMA_CH2_CHANNEL == LL_DMA_CHANNEL_2
    LL_DMA_ClearFlag_TC2(IR_DMA);
    LL_DMA_ClearFlag_HT2(IR_DMA);
    LL_DMA_ClearFlag_TE2(IR_DMA);
#else
#error Update this code. Would you kindly?
#endif

    LL_DMA_EnableIT_TC(IR_DMA_CH2_DEF);
    LL_DMA_EnableIT_HT(IR_DMA_CH2_DEF);
    LL_DMA_EnableIT_TE(IR_DMA_CH2_DEF);

    furry_hal_interrupt_set_isr_ex(IR_DMA_CH2_IRQ, 5, furry_hal_infrared_tx_dma_isr, NULL);
}

static void furry_hal_infrared_tx_fill_buffer_last(uint8_t buf_num) {
    furry_assert(buf_num < 2);
    furry_assert(furry_hal_infrared_state != InfraredStateAsyncRx);
    furry_assert(furry_hal_infrared_state < InfraredStateMAX);
    furry_assert(infrared_tim_tx.data_callback);
    InfraredTxBuf* buffer = &infrared_tim_tx.buffer[buf_num];
    furry_assert(buffer->data != NULL);
    (void)buffer->data;
    furry_assert(buffer->polarity != NULL);
    (void)buffer->polarity;

    infrared_tim_tx.buffer[buf_num].data[0] = 0; // 1 pulse
    infrared_tim_tx.buffer[buf_num].polarity[0] = INFRARED_TX_CCMR_LOW;
    infrared_tim_tx.buffer[buf_num].data[1] = 0; // 1 pulse
    infrared_tim_tx.buffer[buf_num].polarity[1] = INFRARED_TX_CCMR_LOW;
    infrared_tim_tx.buffer[buf_num].size = 2;
    infrared_tim_tx.buffer[buf_num].last_packet_end = true;
    infrared_tim_tx.buffer[buf_num].packet_end = true;
}

static void furry_hal_infrared_tx_fill_buffer(uint8_t buf_num, uint8_t polarity_shift) {
    furry_assert(buf_num < 2);
    furry_assert(furry_hal_infrared_state != InfraredStateAsyncRx);
    furry_assert(furry_hal_infrared_state < InfraredStateMAX);
    furry_assert(infrared_tim_tx.data_callback);
    InfraredTxBuf* buffer = &infrared_tim_tx.buffer[buf_num];
    furry_assert(buffer->data != NULL);
    furry_assert(buffer->polarity != NULL);

    FurryHalInfraredTxGetDataState status = FurryHalInfraredTxGetDataStateOk;
    uint32_t duration = 0;
    bool level = 0;
    size_t* size = &buffer->size;
    size_t polarity_counter = 0;
    while(polarity_shift--) {
        buffer->polarity[polarity_counter++] = INFRARED_TX_CCMR_LOW;
    }

    for(*size = 0; (*size < INFRARED_TIM_TX_DMA_BUFFER_SIZE) &&
                   (status == FurryHalInfraredTxGetDataStateOk);) {
        if(infrared_tim_tx.tx_timing_rest_duration > 0) {
            if(infrared_tim_tx.tx_timing_rest_duration > 0xFFFF) {
                buffer->data[*size] = 0xFFFF;
                status = FurryHalInfraredTxGetDataStateOk;
            } else {
                buffer->data[*size] = infrared_tim_tx.tx_timing_rest_duration;
                status = infrared_tim_tx.tx_timing_rest_status;
            }
            infrared_tim_tx.tx_timing_rest_duration -= buffer->data[*size];
            buffer->polarity[polarity_counter] = infrared_tim_tx.tx_timing_rest_level ?
                                                     INFRARED_TX_CCMR_HIGH :
                                                     INFRARED_TX_CCMR_LOW;
            ++(*size);
            ++polarity_counter;
            continue;
        }

        status = infrared_tim_tx.data_callback(infrared_tim_tx.data_context, &duration, &level);

        uint32_t num_of_impulses = roundf(duration / infrared_tim_tx.cycle_duration);

        if(num_of_impulses == 0) {
            if((*size == 0) && (status == FurryHalInfraredTxGetDataStateDone)) {
                /* if this is one sample in current buffer, but we
                 * have more to send - continue
                 */
                status = FurryHalInfraredTxGetDataStateOk;
            }
        } else if((num_of_impulses - 1) > 0xFFFF) {
            infrared_tim_tx.tx_timing_rest_duration = num_of_impulses - 1;
            infrared_tim_tx.tx_timing_rest_status = status;
            infrared_tim_tx.tx_timing_rest_level = level;
            status = FurryHalInfraredTxGetDataStateOk;
        } else {
            buffer->polarity[polarity_counter] = level ? INFRARED_TX_CCMR_HIGH :
                                                         INFRARED_TX_CCMR_LOW;
            buffer->data[*size] = num_of_impulses - 1;
            ++(*size);
            ++polarity_counter;
        }
    }

    buffer->last_packet_end = (status == FurryHalInfraredTxGetDataStateLastDone);
    buffer->packet_end = buffer->last_packet_end || (status == FurryHalInfraredTxGetDataStateDone);

    if(*size == 0) {
        buffer->data[0] = 0; // 1 pulse
        buffer->polarity[0] = INFRARED_TX_CCMR_LOW;
        buffer->size = 1;
    }
}

static void furry_hal_infrared_tx_dma_set_polarity(uint8_t buf_num, uint8_t polarity_shift) {
    furry_assert(buf_num < 2);
    furry_assert(furry_hal_infrared_state < InfraredStateMAX);
    InfraredTxBuf* buffer = &infrared_tim_tx.buffer[buf_num];
    furry_assert(buffer->polarity != NULL);

    FURRY_CRITICAL_ENTER();
    bool channel_enabled = LL_DMA_IsEnabledChannel(IR_DMA_CH1_DEF);
    if(channel_enabled) {
        LL_DMA_DisableChannel(IR_DMA_CH1_DEF);
    }
    LL_DMA_SetMemoryAddress(IR_DMA_CH1_DEF, (uint32_t)buffer->polarity);
    LL_DMA_SetDataLength(IR_DMA_CH1_DEF, buffer->size + polarity_shift);
    if(channel_enabled) {
        LL_DMA_EnableChannel(IR_DMA_CH1_DEF);
    }
    FURRY_CRITICAL_EXIT();
}

static void furry_hal_infrared_tx_dma_set_buffer(uint8_t buf_num) {
    furry_assert(buf_num < 2);
    furry_assert(furry_hal_infrared_state < InfraredStateMAX);
    InfraredTxBuf* buffer = &infrared_tim_tx.buffer[buf_num];
    furry_assert(buffer->data != NULL);

    /* non-circular mode requires disabled channel before setup */
    FURRY_CRITICAL_ENTER();
    bool channel_enabled = LL_DMA_IsEnabledChannel(IR_DMA_CH2_DEF);
    if(channel_enabled) {
        LL_DMA_DisableChannel(IR_DMA_CH2_DEF);
    }
    LL_DMA_SetMemoryAddress(IR_DMA_CH2_DEF, (uint32_t)buffer->data);
    LL_DMA_SetDataLength(IR_DMA_CH2_DEF, buffer->size);
    if(channel_enabled) {
        LL_DMA_EnableChannel(IR_DMA_CH2_DEF);
    }
    FURRY_CRITICAL_EXIT();
}

static void furry_hal_infrared_async_tx_free_resources(void) {
    furry_assert(
        (furry_hal_infrared_state == InfraredStateIdle) ||
        (furry_hal_infrared_state == InfraredStateAsyncTxStopped));

    if(infrared_external_output) {
        furry_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullDown, GpioSpeedLow);
    } else {
        furry_hal_gpio_init(&gpio_infrared_tx, GpioModeAnalog, GpioPullDown, GpioSpeedLow);
    }
    furry_hal_interrupt_set_isr(IR_DMA_CH1_IRQ, NULL, NULL);
    furry_hal_interrupt_set_isr(IR_DMA_CH2_IRQ, NULL, NULL);
    LL_TIM_DeInit(TIM1);

    furry_semaphore_free(infrared_tim_tx.stop_semaphore);
    free(infrared_tim_tx.buffer[0].data);
    free(infrared_tim_tx.buffer[1].data);
    free(infrared_tim_tx.buffer[0].polarity);
    free(infrared_tim_tx.buffer[1].polarity);

    infrared_tim_tx.buffer[0].data = NULL;
    infrared_tim_tx.buffer[1].data = NULL;
    infrared_tim_tx.buffer[0].polarity = NULL;
    infrared_tim_tx.buffer[1].polarity = NULL;
}

void furry_hal_infrared_async_tx_start(uint32_t freq, float duty_cycle) {
    if((duty_cycle > 1) || (duty_cycle <= 0) || (freq > INFRARED_MAX_FREQUENCY) ||
       (freq < INFRARED_MIN_FREQUENCY) || (infrared_tim_tx.data_callback == NULL)) {
        furry_crash(NULL);
    }

    furry_assert(furry_hal_infrared_state == InfraredStateIdle);
    furry_assert(infrared_tim_tx.buffer[0].data == NULL);
    furry_assert(infrared_tim_tx.buffer[1].data == NULL);
    furry_assert(infrared_tim_tx.buffer[0].polarity == NULL);
    furry_assert(infrared_tim_tx.buffer[1].polarity == NULL);

    size_t alloc_size_data = INFRARED_TIM_TX_DMA_BUFFER_SIZE * sizeof(uint16_t);
    infrared_tim_tx.buffer[0].data = malloc(alloc_size_data);
    infrared_tim_tx.buffer[1].data = malloc(alloc_size_data);

    size_t alloc_size_polarity =
        (INFRARED_TIM_TX_DMA_BUFFER_SIZE + INFRARED_POLARITY_SHIFT) * sizeof(uint8_t);
    infrared_tim_tx.buffer[0].polarity = malloc(alloc_size_polarity);
    infrared_tim_tx.buffer[1].polarity = malloc(alloc_size_polarity);

    infrared_tim_tx.stop_semaphore = furry_semaphore_alloc(1, 0);
    infrared_tim_tx.cycle_duration = 1000000.0 / freq;
    infrared_tim_tx.tx_timing_rest_duration = 0;

    furry_hal_infrared_tx_fill_buffer(0, INFRARED_POLARITY_SHIFT);

    furry_hal_infrared_configure_tim_pwm_tx(freq, duty_cycle);
    furry_hal_infrared_configure_tim_cmgr2_dma_tx();
    furry_hal_infrared_configure_tim_rcr_dma_tx();
    furry_hal_infrared_tx_dma_set_polarity(0, INFRARED_POLARITY_SHIFT);
    furry_hal_infrared_tx_dma_set_buffer(0);

    furry_hal_infrared_state = InfraredStateAsyncTx;

    LL_TIM_ClearFlag_UPDATE(TIM1);
    LL_DMA_EnableChannel(IR_DMA_CH1_DEF);
    LL_DMA_EnableChannel(IR_DMA_CH2_DEF);
    furry_delay_us(5);
    LL_TIM_GenerateEvent_UPDATE(TIM1); /* DMA -> TIMx_RCR */
    furry_delay_us(5);
    if(infrared_external_output) {
        LL_GPIO_ResetOutputPin(
            gpio_ext_pa7.port, gpio_ext_pa7.pin); /* when disable it prevents false pulse */
        furry_hal_gpio_init_ex(
            &gpio_ext_pa7, GpioModeAltFunctionPushPull, GpioPullUp, GpioSpeedHigh, GpioAltFn1TIM1);
    } else {
        LL_GPIO_ResetOutputPin(
            gpio_infrared_tx.port,
            gpio_infrared_tx.pin); /* when disable it prevents false pulse */
        furry_hal_gpio_init_ex(
            &gpio_infrared_tx,
            GpioModeAltFunctionPushPull,
            GpioPullUp,
            GpioSpeedHigh,
            GpioAltFn1TIM1);
    }

    FURRY_CRITICAL_ENTER();
    LL_TIM_GenerateEvent_UPDATE(TIM1); /* TIMx_RCR -> Repetition counter */
    LL_TIM_EnableCounter(TIM1);
    FURRY_CRITICAL_EXIT();
}

void furry_hal_infrared_async_tx_wait_termination(void) {
    furry_assert(furry_hal_infrared_state >= InfraredStateAsyncTx);
    furry_assert(furry_hal_infrared_state < InfraredStateMAX);

    FurryStatus status;
    status = furry_semaphore_acquire(infrared_tim_tx.stop_semaphore, FurryWaitForever);
    furry_check(status == FurryStatusOk);
    furry_hal_infrared_async_tx_free_resources();
    furry_hal_infrared_state = InfraredStateIdle;
}

void furry_hal_infrared_async_tx_stop(void) {
    furry_assert(furry_hal_infrared_state >= InfraredStateAsyncTx);
    furry_assert(furry_hal_infrared_state < InfraredStateMAX);

    FURRY_CRITICAL_ENTER();
    if(furry_hal_infrared_state == InfraredStateAsyncTx)
        furry_hal_infrared_state = InfraredStateAsyncTxStopReq;
    FURRY_CRITICAL_EXIT();

    furry_hal_infrared_async_tx_wait_termination();
}

void furry_hal_infrared_async_tx_set_data_isr_callback(
    FurryHalInfraredTxGetDataISRCallback callback,
    void* context) {
    furry_assert(furry_hal_infrared_state == InfraredStateIdle);
    infrared_tim_tx.data_callback = callback;
    infrared_tim_tx.data_context = context;
}

void furry_hal_infrared_async_tx_set_signal_sent_isr_callback(
    FurryHalInfraredTxSignalSentISRCallback callback,
    void* context) {
    infrared_tim_tx.signal_sent_callback = callback;
    infrared_tim_tx.signal_sent_context = context;
}
