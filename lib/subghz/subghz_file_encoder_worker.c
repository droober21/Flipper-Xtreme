#include "subghz_file_encoder_worker.h"

#include <toolbox/stream/stream.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>

#define TAG "SubGhzFileEncoderWorker"

#define SUBGHZ_FILE_ENCODER_LOAD 512

struct SubGhzFileEncoderWorker {
    FurryThread* thread;
    FurryStreamBuffer* stream;

    Storage* storage;
    FlipperFormat* flipper_format;

    volatile bool worker_running;
    volatile bool worker_stopping;
    bool level;
    bool is_storage_slow;
    FurryString* str_data;
    FurryString* file_path;

    SubGhzFileEncoderWorkerCallbackEnd callback_end;
    void* context_end;
};

void subghz_file_encoder_worker_callback_end(
    SubGhzFileEncoderWorker* instance,
    SubGhzFileEncoderWorkerCallbackEnd callback_end,
    void* context_end) {
    furry_assert(instance);
    furry_assert(callback_end);
    instance->callback_end = callback_end;
    instance->context_end = context_end;
}

void subghz_file_encoder_worker_add_level_duration(
    SubGhzFileEncoderWorker* instance,
    int32_t duration) {
    bool res = true;
    if(duration < 0 && !instance->level) {
        res = false;
    } else if(duration > 0 && instance->level) {
        res = false;
    }

    if(res) {
        instance->level = !instance->level;
        furry_stream_buffer_send(instance->stream, &duration, sizeof(int32_t), 100);
    } else {
        FURRY_LOG_E(TAG, "Invalid level in the stream");
    }
}

bool subghz_file_encoder_worker_data_parse(SubGhzFileEncoderWorker* instance, const char* strStart) {
    char* str1;
    int32_t temp_ds = 0;
    bool res = false;
    // Line sample: "RAW_Data: -1, 2, -2..."

    // Look for a key in the line
    str1 = strstr(strStart, "RAW_Data: ");

    if(str1 != NULL) {
        // Skip key
        str1 = strchr(str1, ' ');

        // Check that there is still an element in the line
        while(strchr(str1, ' ') != NULL) {
            str1 = strchr(str1, ' ');

            // Skip space
            str1 += 1;
            //
            temp_ds = atoi(str1);
            if((temp_ds < -1000000) || (temp_ds > 1000000)) {
                if(temp_ds > 0) {
                    subghz_file_encoder_worker_add_level_duration(instance, (int32_t)100);
                } else {
                    subghz_file_encoder_worker_add_level_duration(instance, (int32_t)-100);
                }
                //FURRY_LOG_I("PARSE", "Number overflow - %d", atoi(str1));
            } else {
                subghz_file_encoder_worker_add_level_duration(instance, temp_ds);
            }
        }
        res = true;
    }
    return res;
}

void subghz_file_encoder_worker_get_text_progress(
    SubGhzFileEncoderWorker* instance,
    FurryString* output) {
    UNUSED(output);
    Stream* stream = flipper_format_get_raw_stream(instance->flipper_format);
    size_t total_size = stream_size(stream);
    size_t current_offset = stream_tell(stream);
    size_t buffer_avail = furry_stream_buffer_bytes_available(instance->stream);

    furry_string_printf(output, "%03u%%", 100 * (current_offset - buffer_avail) / total_size);
}

LevelDuration subghz_file_encoder_worker_get_level_duration(void* context) {
    furry_assert(context);
    SubGhzFileEncoderWorker* instance = context;
    int32_t duration;
    int ret = furry_stream_buffer_receive(instance->stream, &duration, sizeof(int32_t), 0);
    if(ret == sizeof(int32_t)) {
        LevelDuration level_duration = {.level = LEVEL_DURATION_RESET};
        if(duration < 0) {
            level_duration = level_duration_make(false, -duration);
        } else if(duration > 0) {
            level_duration = level_duration_make(true, duration);
        } else if(duration == 0) { //-V547
            level_duration = level_duration_reset();
            FURRY_LOG_I(TAG, "Stop transmission");
            instance->worker_stopping = true;
        }
        return level_duration;
    } else {
        instance->is_storage_slow = true;
        return level_duration_wait();
    }
}

/** Worker thread
 * 
 * @param context 
 * @return exit code 
 */
static int32_t subghz_file_encoder_worker_thread(void* context) {
    SubGhzFileEncoderWorker* instance = context;
    FURRY_LOG_I(TAG, "Worker start");
    bool res = false;
    instance->is_storage_slow = false;
    Stream* stream = flipper_format_get_raw_stream(instance->flipper_format);
    do {
        if(!flipper_format_file_open_existing(
               instance->flipper_format, furry_string_get_cstr(instance->file_path))) {
            FURRY_LOG_E(
                TAG,
                "Unable to open file for read: %s",
                furry_string_get_cstr(instance->file_path));
            break;
        }
        if(!flipper_format_read_string(instance->flipper_format, "Protocol", instance->str_data)) {
            FURRY_LOG_E(TAG, "Missing Protocol");
            break;
        }

        //skip the end of the previous line "\n"
        stream_seek(stream, 1, StreamOffsetFromCurrent);
        res = true;
        instance->worker_stopping = false;
        FURRY_LOG_I(TAG, "Start transmission");
    } while(0);

    while(res && instance->worker_running) {
        size_t stream_free_byte = furry_stream_buffer_spaces_available(instance->stream);
        if((stream_free_byte / sizeof(int32_t)) >= SUBGHZ_FILE_ENCODER_LOAD) {
            if(stream_read_line(stream, instance->str_data)) {
                furry_string_trim(instance->str_data);
                if(!subghz_file_encoder_worker_data_parse(
                       instance, furry_string_get_cstr(instance->str_data))) {
                    subghz_file_encoder_worker_add_level_duration(instance, LEVEL_DURATION_RESET);
                    break;
                }
            } else {
                subghz_file_encoder_worker_add_level_duration(instance, LEVEL_DURATION_RESET);
                break;
            }
        } else {
            furry_delay_ms(1);
        }
    }
    //waiting for the end of the transfer
    if(instance->is_storage_slow) {
        FURRY_LOG_E(TAG, "Storage is slow");
    }
    FURRY_LOG_I(TAG, "End read file");
    while(!furry_hal_subghz_is_async_tx_complete() && instance->worker_running) {
        furry_delay_ms(5);
    }
    FURRY_LOG_I(TAG, "End transmission");
    while(instance->worker_running) {
        if(instance->worker_stopping) {
            if(instance->callback_end) instance->callback_end(instance->context_end);
        }
        furry_delay_ms(50);
    }
    flipper_format_file_close(instance->flipper_format);

    FURRY_LOG_I(TAG, "Worker stop");
    return 0;
}

SubGhzFileEncoderWorker* subghz_file_encoder_worker_alloc() {
    SubGhzFileEncoderWorker* instance = malloc(sizeof(SubGhzFileEncoderWorker));

    instance->thread =
        furry_thread_alloc_ex("SubGhzFEWorker", 2048, subghz_file_encoder_worker_thread, instance);
    instance->stream = furry_stream_buffer_alloc(sizeof(int32_t) * 2048, sizeof(int32_t));

    instance->storage = furry_record_open(RECORD_STORAGE);
    instance->flipper_format = flipper_format_file_alloc(instance->storage);

    instance->str_data = furry_string_alloc();
    instance->file_path = furry_string_alloc();
    instance->level = false;
    instance->worker_stopping = true;

    return instance;
}

void subghz_file_encoder_worker_free(SubGhzFileEncoderWorker* instance) {
    furry_assert(instance);

    furry_stream_buffer_free(instance->stream);
    furry_thread_free(instance->thread);

    furry_string_free(instance->str_data);
    furry_string_free(instance->file_path);

    flipper_format_free(instance->flipper_format);
    furry_record_close(RECORD_STORAGE);

    free(instance);
}

bool subghz_file_encoder_worker_start(SubGhzFileEncoderWorker* instance, const char* file_path) {
    furry_assert(instance);
    furry_assert(!instance->worker_running);

    furry_stream_buffer_reset(instance->stream);
    furry_string_set(instance->file_path, file_path);
    instance->worker_running = true;
    furry_thread_start(instance->thread);

    return true;
}

void subghz_file_encoder_worker_stop(SubGhzFileEncoderWorker* instance) {
    furry_assert(instance);
    furry_assert(instance->worker_running);

    instance->worker_running = false;
    furry_thread_join(instance->thread);
}

bool subghz_file_encoder_worker_is_running(SubGhzFileEncoderWorker* instance) {
    furry_assert(instance);
    return instance->worker_running;
}
