#include "icon_animation_i.h"
#include "icon_i.h"

#include <furry.h>

IconAnimation* icon_animation_alloc(const Icon* icon) {
    furry_assert(icon);
    IconAnimation* instance = malloc(sizeof(IconAnimation));
    instance->icon = icon;
    instance->timer =
        furry_timer_alloc(icon_animation_timer_callback, FurryTimerTypePeriodic, instance);
    return instance;
}

void icon_animation_free(IconAnimation* instance) {
    furry_assert(instance);
    icon_animation_stop(instance);
    while(xTimerIsTimerActive(instance->timer) == pdTRUE) furry_delay_tick(1);
    furry_timer_free(instance->timer);
    free(instance);
}

void icon_animation_set_update_callback(
    IconAnimation* instance,
    IconAnimationCallback callback,
    void* context) {
    furry_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

const uint8_t* icon_animation_get_data(const IconAnimation* instance) {
    return instance->icon->frames[instance->frame];
}

void icon_animation_next_frame(IconAnimation* instance) {
    furry_assert(instance);
    instance->frame = (instance->frame + 1) % instance->icon->frame_count;
}

void icon_animation_timer_callback(void* context) {
    furry_assert(context);

    IconAnimation* instance = context;

    if(!instance->animating) return;

    icon_animation_next_frame(instance);
    if(instance->callback) {
        instance->callback(instance, instance->callback_context);
    }
}

uint8_t icon_animation_get_width(const IconAnimation* instance) {
    furry_assert(instance);
    return instance->icon->width;
}

uint8_t icon_animation_get_height(const IconAnimation* instance) {
    furry_assert(instance);
    return instance->icon->height;
}

void icon_animation_start(IconAnimation* instance) {
    furry_assert(instance);
    if(!instance->animating) {
        instance->animating = true;
        furry_assert(instance->icon->frame_rate);
        furry_check(
            xTimerChangePeriod(
                instance->timer,
                (furry_kernel_get_tick_frequency() / instance->icon->frame_rate),
                portMAX_DELAY) == pdPASS);
    }
}

void icon_animation_stop(IconAnimation* instance) {
    furry_assert(instance);
    if(instance->animating) {
        instance->animating = false;
        furry_check(xTimerStop(instance->timer, portMAX_DELAY) == pdPASS);
        instance->frame = 0;
    }
}

bool icon_animation_is_last_frame(const IconAnimation* instance) {
    furry_assert(instance);
    return instance->icon->frame_count - instance->frame <= 1;
}
