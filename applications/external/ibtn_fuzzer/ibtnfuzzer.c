#include "ibtnfuzzer.h"

#include "scene/ibtnfuzzer_scene_entrypoint.h"
#include "scene/ibtnfuzzer_scene_load_file.h"
#include "scene/ibtnfuzzer_scene_select_field.h"
#include "scene/ibtnfuzzer_scene_run_attack.h"
#include "scene/ibtnfuzzer_scene_load_custom_uids.h"
#include <dolphin/dolphin.h>

#define IBTNFUZZER_APP_FOLDER "/ext/ibtnfuzzer"

static void ibtnfuzzer_draw_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    iBtnFuzzerState* ibtnfuzzer_state = ctx;
    furry_mutex_acquire(ibtnfuzzer_state->mutex, FurryWaitForever);

    // Draw correct Canvas
    switch(ibtnfuzzer_state->current_scene) {
    case NoneScene:
    case SceneEntryPoint:
        ibtnfuzzer_scene_entrypoint_on_draw(canvas, ibtnfuzzer_state);
        break;
    case SceneSelectFile:
        ibtnfuzzer_scene_load_file_on_draw(canvas, ibtnfuzzer_state);
        break;
    case SceneSelectField:
        ibtnfuzzer_scene_select_field_on_draw(canvas, ibtnfuzzer_state);
        break;
    case SceneAttack:
        ibtnfuzzer_scene_run_attack_on_draw(canvas, ibtnfuzzer_state);
        break;
    case SceneLoadCustomUids:
        ibtnfuzzer_scene_load_custom_uids_on_draw(canvas, ibtnfuzzer_state);
        break;
    }

    furry_mutex_release(ibtnfuzzer_state->mutex);
}

void ibtnfuzzer_input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    iBtnFuzzerEvent event = {
        .evt_type = EventTypeKey, .key = input_event->key, .input_type = input_event->type};
    furry_message_queue_put(event_queue, &event, 25);
}

static void ibtnfuzzer_timer_callback(FurryMessageQueue* event_queue) {
    furry_assert(event_queue);
    iBtnFuzzerEvent event = {
        .evt_type = EventTypeTick, .key = InputKeyUp, .input_type = InputTypeRelease};
    furry_message_queue_put(event_queue, &event, 25);
}

iBtnFuzzerState* ibtnfuzzer_alloc() {
    iBtnFuzzerState* ibtnfuzzer = malloc(sizeof(iBtnFuzzerState));
    ibtnfuzzer->notification_msg = furry_string_alloc();
    ibtnfuzzer->attack_name = furry_string_alloc();
    ibtnfuzzer->proto_name = furry_string_alloc();
    ibtnfuzzer->data_str = furry_string_alloc();

    ibtnfuzzer->main_menu_items[0] = furry_string_alloc_set("Default Values");
    ibtnfuzzer->main_menu_items[1] = furry_string_alloc_set("Load File");
    ibtnfuzzer->main_menu_items[2] = furry_string_alloc_set("Load UIDs from file");

    ibtnfuzzer->main_menu_proto_items[0] = furry_string_alloc_set("DS1990");
    ibtnfuzzer->main_menu_proto_items[1] = furry_string_alloc_set("Metakom");
    ibtnfuzzer->main_menu_proto_items[2] = furry_string_alloc_set("Cyfral");

    ibtnfuzzer->previous_scene = NoneScene;
    ibtnfuzzer->current_scene = SceneEntryPoint;
    ibtnfuzzer->is_running = true;
    ibtnfuzzer->is_attacking = false;
    ibtnfuzzer->key_index = 0;
    ibtnfuzzer->menu_index = 0;
    ibtnfuzzer->menu_proto_index = 0;

    ibtnfuzzer->attack = iBtnFuzzerAttackDefaultValues;
    ibtnfuzzer->notify = furry_record_open(RECORD_NOTIFICATION);

    ibtnfuzzer->data[0] = 0x00;
    ibtnfuzzer->data[1] = 0x00;
    ibtnfuzzer->data[2] = 0x00;
    ibtnfuzzer->data[3] = 0x00;
    ibtnfuzzer->data[4] = 0x00;
    ibtnfuzzer->data[5] = 0x00;
    ibtnfuzzer->data[6] = 0x00;
    ibtnfuzzer->data[7] = 0x00;

    ibtnfuzzer->payload[0] = 0x00;
    ibtnfuzzer->payload[1] = 0x00;
    ibtnfuzzer->payload[2] = 0x00;
    ibtnfuzzer->payload[3] = 0x00;
    ibtnfuzzer->payload[4] = 0x00;
    ibtnfuzzer->payload[5] = 0x00;
    ibtnfuzzer->payload[6] = 0x00;
    ibtnfuzzer->payload[7] = 0x00;

    //Dialog
    ibtnfuzzer->dialogs = furry_record_open(RECORD_DIALOGS);

    return ibtnfuzzer;
}

void ibtnfuzzer_free(iBtnFuzzerState* ibtnfuzzer) {
    //Dialog
    furry_record_close(RECORD_DIALOGS);
    notification_message(ibtnfuzzer->notify, &sequence_blink_stop);

    // Strings
    furry_string_free(ibtnfuzzer->notification_msg);
    furry_string_free(ibtnfuzzer->attack_name);
    furry_string_free(ibtnfuzzer->proto_name);
    furry_string_free(ibtnfuzzer->data_str);

    for(uint32_t i = 0; i < 3; i++) {
        furry_string_free(ibtnfuzzer->main_menu_items[i]);
    }

    for(uint32_t i = 0; i < 3; i++) {
        furry_string_free(ibtnfuzzer->main_menu_proto_items[i]);
    }

    free(ibtnfuzzer->data);
    free(ibtnfuzzer->payload);

    // The rest
    free(ibtnfuzzer);
}

// ENTRYPOINT
int32_t ibtnfuzzer_start(void* p) {
    UNUSED(p);
    // Input
    FURRY_LOG_I(TAG, "Initializing input");
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(iBtnFuzzerEvent));
    iBtnFuzzerState* ibtnfuzzer_state = ibtnfuzzer_alloc();

    DOLPHIN_DEED(DolphinDeedPluginStart);
    ibtnfuzzer_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!ibtnfuzzer_state->mutex) {
        FURRY_LOG_E(TAG, "cannot create mutex\r\n");
        furry_message_queue_free(event_queue);
        furry_record_close(RECORD_NOTIFICATION);
        ibtnfuzzer_free(ibtnfuzzer_state);
        return 255;
    }

    Storage* storage = furry_record_open(RECORD_STORAGE);
    if(!storage_simply_mkdir(storage, IBTNFUZZER_APP_FOLDER)) {
        FURRY_LOG_E(TAG, "Could not create folder %s", IBTNFUZZER_APP_FOLDER);
    }
    furry_record_close(RECORD_STORAGE);

    // Configure view port
    FURRY_LOG_I(TAG, "Initializing viewport");
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, ibtnfuzzer_draw_callback, ibtnfuzzer_state);
    view_port_input_callback_set(view_port, ibtnfuzzer_input_callback, event_queue);

    // Configure timer
    FURRY_LOG_I(TAG, "Initializing timer");
    FurryTimer* timer =
        furry_timer_alloc(ibtnfuzzer_timer_callback, FurryTimerTypePeriodic, event_queue);
    furry_timer_start(timer, furry_kernel_get_tick_frequency() / 10); // 10 times per second

    // Register view port in GUI
    FURRY_LOG_I(TAG, "Initializing gui");
    Gui* gui = (Gui*)furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Init values
    iBtnFuzzerEvent event;
    while(ibtnfuzzer_state->is_running) {
        // Get next event
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 25);
        //furry_mutex_acquire(ibtnfuzzer_state->mutex, FurryWaitForever);
        if(event_status == FurryStatusOk) {
            if(event.evt_type == EventTypeKey) {
                //Handle event key
                switch(ibtnfuzzer_state->current_scene) {
                case NoneScene:
                case SceneEntryPoint:
                    ibtnfuzzer_scene_entrypoint_on_event(event, ibtnfuzzer_state);
                    break;
                case SceneSelectFile:
                    ibtnfuzzer_scene_load_file_on_event(event, ibtnfuzzer_state);
                    break;
                case SceneSelectField:
                    ibtnfuzzer_scene_select_field_on_event(event, ibtnfuzzer_state);
                    break;
                case SceneAttack:
                    ibtnfuzzer_scene_run_attack_on_event(event, ibtnfuzzer_state);
                    break;
                case SceneLoadCustomUids:
                    ibtnfuzzer_scene_load_custom_uids_on_event(event, ibtnfuzzer_state);
                    break;
                }

            } else if(event.evt_type == EventTypeTick) {
                //Handle event tick
                if(ibtnfuzzer_state->current_scene != ibtnfuzzer_state->previous_scene) {
                    // Trigger Exit Scene
                    switch(ibtnfuzzer_state->previous_scene) {
                    case SceneEntryPoint:
                        ibtnfuzzer_scene_entrypoint_on_exit(ibtnfuzzer_state);
                        break;
                    case SceneSelectFile:
                        ibtnfuzzer_scene_load_file_on_exit(ibtnfuzzer_state);
                        break;
                    case SceneSelectField:
                        ibtnfuzzer_scene_select_field_on_exit(ibtnfuzzer_state);
                        break;
                    case SceneAttack:
                        ibtnfuzzer_scene_run_attack_on_exit(ibtnfuzzer_state);
                        break;
                    case SceneLoadCustomUids:
                        ibtnfuzzer_scene_load_custom_uids_on_exit(ibtnfuzzer_state);
                        break;
                    case NoneScene:
                        break;
                    }

                    // Trigger Entry Scene
                    switch(ibtnfuzzer_state->current_scene) {
                    case NoneScene:
                    case SceneEntryPoint:
                        ibtnfuzzer_scene_entrypoint_on_enter(ibtnfuzzer_state);
                        break;
                    case SceneSelectFile:
                        ibtnfuzzer_scene_load_file_on_enter(ibtnfuzzer_state);
                        break;
                    case SceneSelectField:
                        ibtnfuzzer_scene_select_field_on_enter(ibtnfuzzer_state);
                        break;
                    case SceneAttack:
                        ibtnfuzzer_scene_run_attack_on_enter(ibtnfuzzer_state);
                        break;
                    case SceneLoadCustomUids:
                        ibtnfuzzer_scene_load_custom_uids_on_enter(ibtnfuzzer_state);
                        break;
                    }
                    ibtnfuzzer_state->previous_scene = ibtnfuzzer_state->current_scene;
                }

                // Trigger Tick Scene
                switch(ibtnfuzzer_state->current_scene) {
                case NoneScene:
                case SceneEntryPoint:
                    ibtnfuzzer_scene_entrypoint_on_tick(ibtnfuzzer_state);
                    break;
                case SceneSelectFile:
                    ibtnfuzzer_scene_load_file_on_tick(ibtnfuzzer_state);
                    break;
                case SceneSelectField:
                    ibtnfuzzer_scene_select_field_on_tick(ibtnfuzzer_state);
                    break;
                case SceneAttack:
                    ibtnfuzzer_scene_run_attack_on_tick(ibtnfuzzer_state);
                    break;
                case SceneLoadCustomUids:
                    ibtnfuzzer_scene_load_custom_uids_on_tick(ibtnfuzzer_state);
                    break;
                }
                view_port_update(view_port);
            }
        }
        //furry_mutex_release(ibtnfuzzer_state->mutex);
    }

    // Cleanup
    furry_timer_stop(timer);
    furry_timer_free(timer);

    FURRY_LOG_I(TAG, "Cleaning up");
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_record_close(RECORD_GUI);
    furry_record_close(RECORD_NOTIFICATION);
    furry_mutex_free(ibtnfuzzer_state->mutex);
    ibtnfuzzer_free(ibtnfuzzer_state);

    return 0;
}
