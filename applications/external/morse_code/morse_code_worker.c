#include "morse_code_worker.h"
#include <furry_hal.h>
#include <lib/flipper_format/flipper_format.h>

#define TAG "MorseCodeWorker"

#define MORSE_CODE_VERSION 0

//A-Z0-1
const char morse_array[36][6] = {".-",    "-...",  "-.-.",  "-..",   ".",     "..-.",
                                 "--.",   "....",  "..",    ".---",  "-.-",   ".-..",
                                 "--",    "-.",    "---",   ".--.",  "--.-",  ".-.",
                                 "...",   "-",     "..-",   "...-",  ".--",   "-..-",
                                 "-.--",  "--..",  ".----", "..---", "...--", "....-",
                                 ".....", "-....", "--...", "---..", "----.", "-----"};
const char symbol_array[36] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
                               'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                               'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

struct MorseCodeWorker {
    FurryThread* thread;
    MorseCodeWorkerCallback callback;
    void* callback_context;
    bool is_running;
    bool play;
    float volume;
    uint32_t dit_delta;
    FurryString* buffer;
    FurryString* words;
};

void morse_code_worker_fill_buffer(MorseCodeWorker* instance, uint32_t duration) {
    FURRY_LOG_D("MorseCode: Duration", "%ld", duration);
    if(duration <= instance->dit_delta)
        furry_string_push_back(instance->buffer, *DOT);
    else if(duration <= (instance->dit_delta * 3))
        furry_string_push_back(instance->buffer, *LINE);
    else
        furry_string_reset(instance->buffer);
    if(furry_string_size(instance->buffer) > 5) furry_string_reset(instance->buffer);
    FURRY_LOG_D("MorseCode: Buffer", "%s", furry_string_get_cstr(instance->buffer));
}

void morse_code_worker_fill_letter(MorseCodeWorker* instance) {
    if(furry_string_size(instance->words) > 63) furry_string_reset(instance->words);
    for(size_t i = 0; i < sizeof(morse_array); i++) {
        if(furry_string_cmp_str(instance->buffer, morse_array[i]) == 0) {
            furry_string_push_back(instance->words, symbol_array[i]);
            break;
        }
    }
    furry_string_reset(instance->buffer);
    FURRY_LOG_D("MorseCode: Words", "%s", furry_string_get_cstr(instance->words));
}

static int32_t morse_code_worker_thread_callback(void* context) {
    furry_assert(context);
    MorseCodeWorker* instance = context;
    bool was_playing = false;
    uint32_t start_tick = 0;
    uint32_t end_tick = 0;
    bool pushed = true;
    bool spaced = true;
    while(instance->is_running) {
        furry_delay_ms(SLEEP);

        if(instance->play) {
            if(!was_playing) {
                start_tick = furry_get_tick();
                if(furry_hal_speaker_acquire(1000)) {
                    furry_hal_speaker_start(FREQUENCY, instance->volume);
                }
                was_playing = true;
            }
        } else {
            if(was_playing) {
                pushed = false;
                spaced = false;
                if(furry_hal_speaker_is_mine()) {
                    furry_hal_speaker_stop();
                    furry_hal_speaker_release();
                }
                end_tick = furry_get_tick();
                was_playing = false;
                morse_code_worker_fill_buffer(instance, end_tick - start_tick);
                start_tick = 0;
            }
        }
        if(!pushed) {
            if(end_tick + (instance->dit_delta * 3) < furry_get_tick()) {
                //NEW LETTER
                if(!furry_string_empty(instance->buffer)) {
                    morse_code_worker_fill_letter(instance);
                    if(instance->callback)
                        instance->callback(instance->words, instance->callback_context);
                } else {
                    spaced = true;
                }
                pushed = true;
            }
        }
        if(!spaced) {
            if(end_tick + (instance->dit_delta * 7) < furry_get_tick()) {
                //NEW WORD
                furry_string_push_back(instance->words, *SPACE);
                if(instance->callback)
                    instance->callback(instance->words, instance->callback_context);
                spaced = true;
            }
        }
    }
    return 0;
}

MorseCodeWorker* morse_code_worker_alloc() {
    MorseCodeWorker* instance = malloc(sizeof(MorseCodeWorker));
    instance->thread = furry_thread_alloc();
    furry_thread_set_name(instance->thread, "MorseCodeWorker");
    furry_thread_set_stack_size(instance->thread, 1024);
    furry_thread_set_context(instance->thread, instance);
    furry_thread_set_callback(instance->thread, morse_code_worker_thread_callback);
    instance->play = false;
    instance->volume = 1.0f;
    instance->dit_delta = 150;
    instance->buffer = furry_string_alloc_set_str("");
    instance->words = furry_string_alloc_set_str("");
    return instance;
}

void morse_code_worker_free(MorseCodeWorker* instance) {
    furry_assert(instance);
    furry_string_free(instance->buffer);
    furry_string_free(instance->words);
    furry_thread_free(instance->thread);
    free(instance);
}

void morse_code_worker_set_callback(
    MorseCodeWorker* instance,
    MorseCodeWorkerCallback callback,
    void* context) {
    furry_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void morse_code_worker_play(MorseCodeWorker* instance, bool play) {
    furry_assert(instance);
    instance->play = play;
}

void morse_code_worker_set_volume(MorseCodeWorker* instance, float level) {
    furry_assert(instance);
    instance->volume = level;
}

void morse_code_worker_set_dit_delta(MorseCodeWorker* instance, uint32_t delta) {
    furry_assert(instance);
    instance->dit_delta = delta;
}

void morse_code_worker_reset_text(MorseCodeWorker* instance) {
    furry_assert(instance);
    furry_string_reset(instance->buffer);
    furry_string_reset(instance->words);
}

void morse_code_worker_start(MorseCodeWorker* instance) {
    furry_assert(instance);
    furry_assert(instance->is_running == false);
    instance->is_running = true;
    furry_thread_start(instance->thread);
    FURRY_LOG_D("MorseCode: Start", "is Running");
}

void morse_code_worker_stop(MorseCodeWorker* instance) {
    furry_assert(instance);
    furry_assert(instance->is_running == true);
    instance->play = false;
    instance->is_running = false;
    furry_thread_join(instance->thread);
    FURRY_LOG_D("MorseCode: Stop", "Stop");
}
