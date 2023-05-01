#include <furry.h>
#include <furry_hal.h>
#include "stm32_sam.h"
// WOULD BE COOL IF SOMEONE MADE A TEXT ENTRY SCREEN TO HAVE IT READ WHAT IS ENTERED TO TEXT
STM32SAM voice;

extern "C" int32_t sam_app(void* p) {
    UNUSED(p);
    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
        voice.begin();
        voice.say(
            "All your base are belong to us. You have no chance to survive make your time. ha. ha. ha. GOOD BYE. ");
        furry_hal_speaker_release();
    }
    return 0;
}

extern "C" int32_t sam_app_yes(void* p) {
    UNUSED(p);
    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
        voice.begin();
        voice.say("Yes");
        furry_hal_speaker_release();
    }
    return 0;
}

extern "C" int32_t sam_app_no(void* p) {
    UNUSED(p);
    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
        voice.begin();
        voice.say("No");
        furry_hal_speaker_release();
    }
    return 0;
}

extern "C" int32_t sam_app_wtf(void* p) {
    UNUSED(p);
    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
        voice.begin();
        voice.say("What The Fuck");
        furry_hal_speaker_release();
    }
    return 0;
}
