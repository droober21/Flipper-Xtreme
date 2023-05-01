#include "dolphin/dolphin.h"
#include "dolphin/helpers/dolphin_state.h"
#include "dolphin_i.h"
#include "portmacro.h"
#include "projdefs.h"
#include <furry_hal.h>
#include <stdint.h>
#include <furry.h>
#include <xtreme.h>
#define DOLPHIN_LOCK_EVENT_FLAG (0x1)

#define TAG "Dolphin"
#define HOURS_IN_TICKS(x) ((x)*60 * 60 * 1000)

static void dolphin_update_clear_limits_timer_period(Dolphin* dolphin);

void dolphin_deed(Dolphin* dolphin, DolphinDeed deed) {
    furry_assert(dolphin);
    DolphinEvent event;
    event.type = DolphinEventTypeDeed;
    event.deed = deed;
    dolphin_event_send_async(dolphin, &event);
}

DolphinStats dolphin_stats(Dolphin* dolphin) {
    furry_assert(dolphin);

    DolphinStats stats;
    DolphinEvent event;

    event.type = DolphinEventTypeStats;
    event.stats = &stats;

    dolphin_event_send_wait(dolphin, &event);

    return stats;
}

void dolphin_flush(Dolphin* dolphin) {
    furry_assert(dolphin);

    DolphinEvent event;
    event.type = DolphinEventTypeFlush;

    dolphin_event_send_wait(dolphin, &event);
}

void dolphin_butthurt_timer_callback(TimerHandle_t xTimer) {
    Dolphin* dolphin = pvTimerGetTimerID(xTimer);
    furry_assert(dolphin);

    DolphinEvent event;
    event.type = DolphinEventTypeIncreaseButthurt;
    dolphin_event_send_async(dolphin, &event);
}

void dolphin_flush_timer_callback(TimerHandle_t xTimer) {
    Dolphin* dolphin = pvTimerGetTimerID(xTimer);
    furry_assert(dolphin);

    DolphinEvent event;
    event.type = DolphinEventTypeFlush;
    dolphin_event_send_async(dolphin, &event);
}

void dolphin_clear_limits_timer_callback(TimerHandle_t xTimer) {
    Dolphin* dolphin = pvTimerGetTimerID(xTimer);
    furry_assert(dolphin);

    xTimerChangePeriod(dolphin->clear_limits_timer, HOURS_IN_TICKS(24), portMAX_DELAY);

    DolphinEvent event;
    event.type = DolphinEventTypeClearLimits;
    dolphin_event_send_async(dolphin, &event);
}

Dolphin* dolphin_alloc() {
    Dolphin* dolphin = malloc(sizeof(Dolphin));

    dolphin->state = dolphin_state_alloc();
    dolphin->event_queue = furry_message_queue_alloc(8, sizeof(DolphinEvent));
    dolphin->pubsub = furry_pubsub_alloc();
    int32_t butthurt = XTREME_SETTINGS()->butthurt_timer;
    dolphin->butthurt_timer = xTimerCreate(
        NULL,
        (butthurt > 0) ? (butthurt * 1000) : -1,
        pdTRUE,
        dolphin,
        dolphin_butthurt_timer_callback);
    dolphin->flush_timer =
        xTimerCreate(NULL, 30 * 1000, pdFALSE, dolphin, dolphin_flush_timer_callback);
    dolphin->clear_limits_timer = xTimerCreate(
        NULL, HOURS_IN_TICKS(24), pdTRUE, dolphin, dolphin_clear_limits_timer_callback);

    return dolphin;
}

void dolphin_free(Dolphin* dolphin) {
    furry_assert(dolphin);

    dolphin_state_free(dolphin->state);
    furry_message_queue_free(dolphin->event_queue);

    free(dolphin);
}

void dolphin_event_send_async(Dolphin* dolphin, DolphinEvent* event) {
    furry_assert(dolphin);
    furry_assert(event);
    event->flag = NULL;
    furry_check(
        furry_message_queue_put(dolphin->event_queue, event, FurryWaitForever) == FurryStatusOk);
}

void dolphin_event_send_wait(Dolphin* dolphin, DolphinEvent* event) {
    furry_assert(dolphin);
    furry_assert(event);
    event->flag = furry_event_flag_alloc();
    furry_check(event->flag);
    furry_check(
        furry_message_queue_put(dolphin->event_queue, event, FurryWaitForever) == FurryStatusOk);
    furry_check(
        furry_event_flag_wait(
            event->flag, DOLPHIN_LOCK_EVENT_FLAG, FurryFlagWaitAny, FurryWaitForever) ==
        DOLPHIN_LOCK_EVENT_FLAG);
    furry_event_flag_free(event->flag);
}

void dolphin_event_release(Dolphin* dolphin, DolphinEvent* event) {
    UNUSED(dolphin);
    if(event->flag) {
        furry_event_flag_set(event->flag, DOLPHIN_LOCK_EVENT_FLAG);
    }
}

FurryPubSub* dolphin_get_pubsub(Dolphin* dolphin) {
    return dolphin->pubsub;
}

static void dolphin_update_clear_limits_timer_period(Dolphin* dolphin) {
    furry_assert(dolphin);
    TickType_t now_ticks = xTaskGetTickCount();
    TickType_t timer_expires_at = xTimerGetExpiryTime(dolphin->clear_limits_timer);

    if((timer_expires_at - now_ticks) > HOURS_IN_TICKS(0.1)) {
        FurryHalRtcDateTime date;
        furry_hal_rtc_get_datetime(&date);
        TickType_t now_time_in_ms = ((date.hour * 60 + date.minute) * 60 + date.second) * 1000;
        TickType_t time_to_clear_limits = 0;

        if(date.hour < 5) {
            time_to_clear_limits = HOURS_IN_TICKS(5) - now_time_in_ms;
        } else {
            time_to_clear_limits = HOURS_IN_TICKS(24 + 5) - now_time_in_ms;
        }

        xTimerChangePeriod(dolphin->clear_limits_timer, time_to_clear_limits, portMAX_DELAY);
    }
}

int32_t dolphin_srv(void* p) {
    UNUSED(p);

    if(!furry_hal_is_normal_boot()) {
        FURRY_LOG_W(TAG, "Skipping start in special boot mode");
        return 0;
    }

    Dolphin* dolphin = dolphin_alloc();
    furry_record_create(RECORD_DOLPHIN, dolphin);

    dolphin_state_load(dolphin->state);
    xTimerReset(dolphin->butthurt_timer, portMAX_DELAY);
    dolphin_update_clear_limits_timer_period(dolphin);
    xTimerReset(dolphin->clear_limits_timer, portMAX_DELAY);

    DolphinEvent event;
    while(1) {
        if(furry_message_queue_get(dolphin->event_queue, &event, HOURS_IN_TICKS(1)) ==
           FurryStatusOk) {
            if(event.type == DolphinEventTypeDeed) {
                dolphin_state_on_deed(dolphin->state, event.deed);
                DolphinPubsubEvent event = DolphinPubsubEventUpdate;
                furry_pubsub_publish(dolphin->pubsub, &event);
                xTimerReset(dolphin->butthurt_timer, portMAX_DELAY);
                xTimerReset(dolphin->flush_timer, portMAX_DELAY);
            } else if(event.type == DolphinEventTypeStats) {
                event.stats->icounter = dolphin->state->data.icounter;
                event.stats->butthurt = dolphin->state->data.butthurt;
                event.stats->timestamp = dolphin->state->data.timestamp;
                event.stats->level = dolphin_get_level(dolphin->state->data.icounter);
                event.stats->level_up_is_pending =
                    !dolphin_state_xp_to_levelup(dolphin->state->data.icounter);
            } else if(event.type == DolphinEventTypeFlush) {
                FURRY_LOG_I(TAG, "Flush stats");
                dolphin_state_save(dolphin->state);
            } else if(event.type == DolphinEventTypeClearLimits) {
                FURRY_LOG_I(TAG, "Clear limits");
                dolphin_state_clear_limits(dolphin->state);
                dolphin_state_save(dolphin->state);
            } else if(event.type == DolphinEventTypeIncreaseButthurt) {
                FURRY_LOG_I(TAG, "Increase butthurt");
                dolphin_state_butthurted(dolphin->state);
                dolphin_state_save(dolphin->state);
            }
            dolphin_event_release(dolphin, &event);
        } else {
            /* once per hour check rtc time is not changed */
            dolphin_update_clear_limits_timer_period(dolphin);
        }
    }

    dolphin_free(dolphin);

    return 0;
}

void dolphin_upgrade_level(Dolphin* dolphin) {
    dolphin_state_increase_level(dolphin->state);
    dolphin_flush(dolphin);
}
