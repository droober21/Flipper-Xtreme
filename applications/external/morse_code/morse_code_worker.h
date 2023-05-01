#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furry.h>

#define FREQUENCY 261.63f
#define SLEEP 10
#define DOT "."
#define LINE "-"
#define SPACE " "

typedef void (*MorseCodeWorkerCallback)(FurryString* buffer, void* context);

typedef struct MorseCodeWorker MorseCodeWorker;

MorseCodeWorker* morse_code_worker_alloc();

void morse_code_worker_free(MorseCodeWorker* instance);

void morse_code_worker_set_callback(
    MorseCodeWorker* instance,
    MorseCodeWorkerCallback callback,
    void* context);

void morse_code_worker_start(MorseCodeWorker* instance);

void morse_code_worker_stop(MorseCodeWorker* instance);

void morse_code_worker_play(MorseCodeWorker* instance, bool play);

void morse_code_worker_reset_text(MorseCodeWorker* instance);

void morse_code_worker_set_volume(MorseCodeWorker* instance, float level);

void morse_code_worker_set_dit_delta(MorseCodeWorker* instance, uint32_t delta);
