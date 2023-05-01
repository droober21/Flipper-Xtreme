#include <furry.h>
#include <furry_hal.h>
#include "notification.h"
#include "notification_messages.h"
#include "notification_app.h"

void notification_message(NotificationApp* app, const NotificationSequence* sequence) {
    NotificationAppMessage m = {
        .type = NotificationLayerMessage, .sequence = sequence, .back_event = NULL};
    furry_check(furry_message_queue_put(app->queue, &m, FurryWaitForever) == FurryStatusOk);
};

void notification_internal_message(NotificationApp* app, const NotificationSequence* sequence) {
    NotificationAppMessage m = {
        .type = InternalLayerMessage, .sequence = sequence, .back_event = NULL};
    furry_check(furry_message_queue_put(app->queue, &m, FurryWaitForever) == FurryStatusOk);
};

void notification_message_block(NotificationApp* app, const NotificationSequence* sequence) {
    NotificationAppMessage m = {
        .type = NotificationLayerMessage,
        .sequence = sequence,
        .back_event = furry_event_flag_alloc()};
    furry_check(furry_message_queue_put(app->queue, &m, FurryWaitForever) == FurryStatusOk);
    furry_event_flag_wait(
        m.back_event, NOTIFICATION_EVENT_COMPLETE, FurryFlagWaitAny, FurryWaitForever);
    furry_event_flag_free(m.back_event);
};

void notification_internal_message_block(
    NotificationApp* app,
    const NotificationSequence* sequence) {
    NotificationAppMessage m = {
        .type = InternalLayerMessage, .sequence = sequence, .back_event = furry_event_flag_alloc()};
    furry_check(furry_message_queue_put(app->queue, &m, FurryWaitForever) == FurryStatusOk);
    furry_event_flag_wait(
        m.back_event, NOTIFICATION_EVENT_COMPLETE, FurryFlagWaitAny, FurryWaitForever);
    furry_event_flag_free(m.back_event);
};
