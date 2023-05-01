#include "infrared.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <furry.h>
#include <furry_hal_infrared.h>

static uint32_t infrared_tx_number_of_transmissions = 0;
static uint32_t infrared_tx_raw_timings_index = 0;
static uint32_t infrared_tx_raw_timings_number = 0;
static uint32_t infrared_tx_raw_start_from_mark = 0;
static bool infrared_tx_raw_add_silence = false;

FurryHalInfraredTxGetDataState
    infrared_get_raw_data_callback(void* context, uint32_t* duration, bool* level) {
    furry_assert(duration);
    furry_assert(level);
    furry_assert(context);

    FurryHalInfraredTxGetDataState state = FurryHalInfraredTxGetDataStateOk;
    const uint32_t* timings = context;

    if(infrared_tx_raw_add_silence && (infrared_tx_raw_timings_index == 0)) {
        infrared_tx_raw_add_silence = false;
        *level = false;
        *duration = INFRARED_RAW_TX_TIMING_DELAY_US;
    } else {
        *level = infrared_tx_raw_start_from_mark ^ (infrared_tx_raw_timings_index % 2);
        *duration = timings[infrared_tx_raw_timings_index++];
    }

    if(infrared_tx_raw_timings_number == infrared_tx_raw_timings_index) {
        state = FurryHalInfraredTxGetDataStateLastDone;
    }

    return state;
}

void infrared_send_raw_ext(
    const uint32_t timings[],
    uint32_t timings_cnt,
    bool start_from_mark,
    uint32_t frequency,
    float duty_cycle) {
    furry_assert(timings);

    infrared_tx_raw_start_from_mark = start_from_mark;
    infrared_tx_raw_timings_index = 0;
    infrared_tx_raw_timings_number = timings_cnt;
    infrared_tx_raw_add_silence = start_from_mark;
    furry_hal_infrared_async_tx_set_data_isr_callback(
        infrared_get_raw_data_callback, (void*)timings);
    furry_hal_infrared_async_tx_start(frequency, duty_cycle);
    furry_hal_infrared_async_tx_wait_termination();

    furry_assert(!furry_hal_infrared_is_busy());
}

void infrared_send_raw(const uint32_t timings[], uint32_t timings_cnt, bool start_from_mark) {
    infrared_send_raw_ext(
        timings,
        timings_cnt,
        start_from_mark,
        INFRARED_COMMON_CARRIER_FREQUENCY,
        INFRARED_COMMON_DUTY_CYCLE);
}

FurryHalInfraredTxGetDataState
    infrared_get_data_callback(void* context, uint32_t* duration, bool* level) {
    FurryHalInfraredTxGetDataState state;
    InfraredEncoderHandler* handler = context;
    InfraredStatus status = InfraredStatusError;

    if(infrared_tx_number_of_transmissions > 0) {
        status = infrared_encode(handler, duration, level);
    }

    if(status == InfraredStatusError) {
        state = FurryHalInfraredTxGetDataStateLastDone;
        *duration = 0;
        *level = 0;
    } else if(status == InfraredStatusOk) {
        state = FurryHalInfraredTxGetDataStateOk;
    } else if(status == InfraredStatusDone) {
        if(--infrared_tx_number_of_transmissions == 0) {
            state = FurryHalInfraredTxGetDataStateLastDone;
        } else {
            state = FurryHalInfraredTxGetDataStateDone;
        }
    } else {
        furry_crash(NULL);
    }

    return state;
}

void infrared_send(const InfraredMessage* message, int times) {
    furry_assert(message);
    furry_assert(times);
    furry_assert(infrared_is_protocol_valid(message->protocol));

    InfraredEncoderHandler* handler = infrared_alloc_encoder();
    infrared_reset_encoder(handler, message);
    infrared_tx_number_of_transmissions =
        MAX((int)infrared_get_protocol_min_repeat_count(message->protocol), times);

    uint32_t frequency = infrared_get_protocol_frequency(message->protocol);
    float duty_cycle = infrared_get_protocol_duty_cycle(message->protocol);

    furry_hal_infrared_async_tx_set_data_isr_callback(infrared_get_data_callback, handler);
    furry_hal_infrared_async_tx_start(frequency, duty_cycle);
    furry_hal_infrared_async_tx_wait_termination();

    infrared_free_encoder(handler);

    furry_assert(!furry_hal_infrared_is_busy());
}
