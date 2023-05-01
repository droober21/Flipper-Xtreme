#include "subbrute_worker_private.h"
#include <string.h>
#include <toolbox/stream/stream.h>
#include <flipper_format.h>
#include <flipper_format_i.h>
#include <lib/subghz/protocols/protocol_items.h>

#define TAG "SubBruteWorker"
#define SUBBRUTE_TX_TIMEOUT 5
#define SUBBRUTE_MANUAL_TRANSMIT_INTERVAL 400

SubBruteWorker* subbrute_worker_alloc() {
    SubBruteWorker* instance = malloc(sizeof(SubBruteWorker));

    instance->state = SubBruteWorkerStateIDLE;
    instance->step = 0;
    instance->worker_running = false;
    instance->initiated = false;
    instance->last_time_tx_data = 0;
    instance->load_index = 0;

    instance->thread = furry_thread_alloc();
    furry_thread_set_name(instance->thread, "SubBruteAttackWorker");
    furry_thread_set_stack_size(instance->thread, 2048);
    furry_thread_set_context(instance->thread, instance);
    furry_thread_set_callback(instance->thread, subbrute_worker_thread);

    instance->context = NULL;
    instance->callback = NULL;

    instance->decoder_result = NULL;
    instance->transmitter = NULL;
    instance->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(
        instance->environment, (void*)&subghz_protocol_registry);

    instance->transmit_mode = false;

    return instance;
}

void subbrute_worker_free(SubBruteWorker* instance) {
    furry_assert(instance);

    // I don't know how to free this
    instance->decoder_result = NULL;

    if(instance->transmitter != NULL) {
        subghz_transmitter_free(instance->transmitter);
        instance->transmitter = NULL;
    }

    subghz_environment_free(instance->environment);
    instance->environment = NULL;

    furry_thread_free(instance->thread);

    free(instance);
}

uint64_t subbrute_worker_get_step(SubBruteWorker* instance) {
    return instance->step;
}

bool subbrute_worker_set_step(SubBruteWorker* instance, uint64_t step) {
    furry_assert(instance);
    if(!subbrute_worker_can_manual_transmit(instance)) {
        FURRY_LOG_W(TAG, "Cannot set step during running mode");
        return false;
    }

    instance->step = step;

    return true;
}

bool subbrute_worker_init_default_attack(
    SubBruteWorker* instance,
    SubBruteAttacks attack_type,
    uint64_t step,
    const SubBruteProtocol* protocol,
    uint8_t extra_repeats) {
    furry_assert(instance);

    if(instance->worker_running) {
        FURRY_LOG_W(TAG, "Init Worker when it's running");
        subbrute_worker_stop(instance);
    }

    instance->attack = attack_type;
    instance->frequency = protocol->frequency;
    instance->preset = protocol->preset;
    instance->file = protocol->file;
    instance->step = step;
    instance->bits = protocol->bits;
    instance->te = protocol->te;
    instance->repeat = protocol->repeat + extra_repeats;
    instance->load_index = 0;
    instance->file_key = 0;
    instance->two_bytes = false;

    instance->max_value =
        subbrute_protocol_calc_max_value(instance->attack, instance->bits, instance->two_bytes);

    instance->initiated = true;
    instance->state = SubBruteWorkerStateReady;
    subbrute_worker_send_callback(instance);
#ifdef FURRY_DEBUG
    FURRY_LOG_I(
        TAG,
        "subbrute_worker_init_default_attack: %s, bits: %d, preset: %s, file: %s, te: %ld, repeat: %d, max_value: %lld",
        subbrute_protocol_name(instance->attack),
        instance->bits,
        subbrute_protocol_preset(instance->preset),
        subbrute_protocol_file(instance->file),
        instance->te,
        instance->repeat,
        instance->max_value);
#endif

    return true;
}

bool subbrute_worker_init_file_attack(
    SubBruteWorker* instance,
    uint64_t step,
    uint8_t load_index,
    uint64_t file_key,
    SubBruteProtocol* protocol,
    uint8_t extra_repeats,
    bool two_bytes) {
    furry_assert(instance);

    if(instance->worker_running) {
        FURRY_LOG_W(TAG, "Init Worker when it's running");
        subbrute_worker_stop(instance);
    }

    instance->attack = SubBruteAttackLoadFile;
    instance->frequency = protocol->frequency;
    instance->preset = protocol->preset;
    instance->file = protocol->file;
    instance->step = step;
    instance->bits = protocol->bits;
    instance->te = protocol->te;
    instance->load_index = load_index;
    instance->repeat = protocol->repeat + extra_repeats;
    instance->file_key = file_key;
    instance->two_bytes = two_bytes;

    instance->max_value =
        subbrute_protocol_calc_max_value(instance->attack, instance->bits, instance->two_bytes);

    instance->initiated = true;
    instance->state = SubBruteWorkerStateReady;
    subbrute_worker_send_callback(instance);
#ifdef FURRY_DEBUG
    FURRY_LOG_I(
        TAG,
        "subbrute_worker_init_file_attack: %s, bits: %d, preset: %s, file: %s, te: %ld, repeat: %d, max_value: %lld, key: %llX",
        subbrute_protocol_name(instance->attack),
        instance->bits,
        subbrute_protocol_preset(instance->preset),
        subbrute_protocol_file(instance->file),
        instance->te,
        instance->repeat,
        instance->max_value,
        instance->file_key);
#endif

    return true;
}

bool subbrute_worker_start(SubBruteWorker* instance) {
    furry_assert(instance);

    if(!instance->initiated) {
        FURRY_LOG_W(TAG, "Worker not init!");
        return false;
    }

    if(instance->worker_running) {
        FURRY_LOG_W(TAG, "Worker is already running!");
        return false;
    }
    if(instance->state != SubBruteWorkerStateReady &&
       instance->state != SubBruteWorkerStateFinished) {
        FURRY_LOG_W(TAG, "Worker cannot start, invalid device state: %d", instance->state);
        return false;
    }

    instance->worker_running = true;
    furry_thread_start(instance->thread);

    return true;
}

void subbrute_worker_stop(SubBruteWorker* instance) {
    furry_assert(instance);

    if(!instance->worker_running) {
        return;
    }

    instance->worker_running = false;
    furry_thread_join(instance->thread);

    furry_hal_subghz_set_path(FurryHalSubGhzPathIsolate);
    furry_hal_subghz_sleep();
}

bool subbrute_worker_transmit_current_key(SubBruteWorker* instance, uint64_t step) {
    furry_assert(instance);

    if(!instance->initiated) {
        FURRY_LOG_W(TAG, "Worker not init!");
        return false;
    }
    if(instance->worker_running) {
        FURRY_LOG_W(TAG, "Worker in running state!");
        return false;
    }
    if(instance->state != SubBruteWorkerStateReady &&
       instance->state != SubBruteWorkerStateFinished) {
        FURRY_LOG_W(TAG, "Invalid state for running worker! State: %d", instance->state);
        return false;
    }

    uint32_t ticks = furry_get_tick();
    if((ticks - instance->last_time_tx_data) < SUBBRUTE_MANUAL_TRANSMIT_INTERVAL) {
#if FURRY_DEBUG
        FURRY_LOG_D(TAG, "Need to wait, current: %ld", ticks - instance->last_time_tx_data);
#endif
        return false;
    }

    instance->last_time_tx_data = ticks;
    instance->step = step;

    bool result;
    instance->protocol_name = subbrute_protocol_file(instance->file);
    FlipperFormat* flipper_format = flipper_format_string_alloc();
    Stream* stream = flipper_format_get_raw_stream(flipper_format);

    stream_clean(stream);

    if(instance->attack == SubBruteAttackLoadFile) {
        subbrute_protocol_file_payload(
            stream,
            step,
            instance->bits,
            instance->te,
            instance->repeat,
            instance->load_index,
            instance->file_key,
            instance->two_bytes);
    } else {
        subbrute_protocol_default_payload(
            stream, instance->file, step, instance->bits, instance->te, instance->repeat);
    }

    //    size_t written = stream_write_string(stream, payload);
    //    if(written <= 0) {
    //        FURRY_LOG_W(TAG, "Error creating packet! EXIT");
    //        result = false;
    //    } else {
    subbrute_worker_subghz_transmit(instance, flipper_format);

    result = true;
#if FURRY_DEBUG
    FURRY_LOG_D(TAG, "Manual transmit done");
#endif
    //    }

    flipper_format_free(flipper_format);
    //    furry_string_free(payload);

    return result;
}

bool subbrute_worker_is_running(SubBruteWorker* instance) {
    return instance->worker_running;
}

bool subbrute_worker_can_manual_transmit(SubBruteWorker* instance) {
    furry_assert(instance);

    if(!instance->initiated) {
        FURRY_LOG_W(TAG, "Worker not init!");
        return false;
    }

    return !instance->worker_running && instance->state != SubBruteWorkerStateIDLE &&
           instance->state != SubBruteWorkerStateTx &&
           ((furry_get_tick() - instance->last_time_tx_data) > SUBBRUTE_MANUAL_TRANSMIT_INTERVAL);
}

void subbrute_worker_set_callback(
    SubBruteWorker* instance,
    SubBruteWorkerCallback callback,
    void* context) {
    furry_assert(instance);

    instance->callback = callback;
    instance->context = context;
}

void subbrute_worker_subghz_transmit(SubBruteWorker* instance, FlipperFormat* flipper_format) {
    while(instance->transmit_mode) {
        furry_delay_ms(SUBBRUTE_TX_TIMEOUT);
    }
    instance->transmit_mode = true;
    if(instance->transmitter != NULL) {
        subghz_transmitter_free(instance->transmitter);
        instance->transmitter = NULL;
    }
    instance->transmitter =
        subghz_transmitter_alloc_init(instance->environment, instance->protocol_name);
    subghz_transmitter_deserialize(instance->transmitter, flipper_format);
    furry_hal_subghz_reset();
    furry_hal_subghz_load_preset(instance->preset);
    furry_hal_subghz_set_frequency_and_path(instance->frequency);
    furry_hal_subghz_start_async_tx(subghz_transmitter_yield, instance->transmitter);

    while(!furry_hal_subghz_is_async_tx_complete()) {
        furry_delay_ms(SUBBRUTE_TX_TIMEOUT);
    }
    furry_hal_subghz_stop_async_tx();

    furry_hal_subghz_set_path(FurryHalSubGhzPathIsolate);
    furry_hal_subghz_sleep();
    subghz_transmitter_free(instance->transmitter);
    instance->transmitter = NULL;

    instance->transmit_mode = false;
}

void subbrute_worker_send_callback(SubBruteWorker* instance) {
    if(instance->callback != NULL) {
        instance->callback(instance->context, instance->state);
    }
}

/**
 * Entrypoint for worker
 *
 * @param context SubBruteWorker*
 * @return 0 if ok
 */
int32_t subbrute_worker_thread(void* context) {
    furry_assert(context);
    SubBruteWorker* instance = (SubBruteWorker*)context;

    if(!instance->worker_running) {
        FURRY_LOG_W(TAG, "Worker is not set to running state!");
        return -1;
    }
    if(instance->state != SubBruteWorkerStateReady &&
       instance->state != SubBruteWorkerStateFinished) {
        FURRY_LOG_W(TAG, "Invalid state for running worker! State: %d", instance->state);
        return -2;
    }
#ifdef FURRY_DEBUG
    FURRY_LOG_I(TAG, "Worker start");
#endif

    SubBruteWorkerState local_state = instance->state = SubBruteWorkerStateTx;
    subbrute_worker_send_callback(instance);

    instance->protocol_name = subbrute_protocol_file(instance->file);

    FlipperFormat* flipper_format = flipper_format_string_alloc();
    Stream* stream = flipper_format_get_raw_stream(flipper_format);

    while(instance->worker_running) {
        stream_clean(stream);
        if(instance->attack == SubBruteAttackLoadFile) {
            subbrute_protocol_file_payload(
                stream,
                instance->step,
                instance->bits,
                instance->te,
                instance->repeat,
                instance->load_index,
                instance->file_key,
                instance->two_bytes);
        } else {
            subbrute_protocol_default_payload(
                stream,
                instance->file,
                instance->step,
                instance->bits,
                instance->te,
                instance->repeat);
        }
#ifdef FURRY_DEBUG
        //FURRY_LOG_I(TAG, "Payload: %s", furry_string_get_cstr(payload));
        //furry_delay_ms(SUBBRUTE_MANUAL_TRANSMIT_INTERVAL / 4);
#endif

        //        size_t written = stream_write_stream_write_string(stream, payload);
        //        if(written <= 0) {
        //            FURRY_LOG_W(TAG, "Error creating packet! BREAK");
        //            instance->worker_running = false;
        //            local_state = SubBruteWorkerStateIDLE;
        //            furry_string_free(payload);
        //            break;
        //        }

        subbrute_worker_subghz_transmit(instance, flipper_format);

        if(instance->step + 1 > instance->max_value) {
#ifdef FURRY_DEBUG
            FURRY_LOG_I(TAG, "Worker finished to end");
#endif
            local_state = SubBruteWorkerStateFinished;
            //            furry_string_free(payload);
            break;
        }
        instance->step++;

        //        furry_string_free(payload);
        furry_delay_ms(SUBBRUTE_TX_TIMEOUT);
    }

    flipper_format_free(flipper_format);

    instance->worker_running = false; // Because we have error states
    instance->state = local_state == SubBruteWorkerStateTx ? SubBruteWorkerStateReady :
                                                             local_state;
    subbrute_worker_send_callback(instance);

#ifdef FURRY_DEBUG
    FURRY_LOG_I(TAG, "Worker stop");
#endif
    return 0;
}
