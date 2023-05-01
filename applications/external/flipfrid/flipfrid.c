#include "flipfrid.h"

#include "scene/flipfrid_scene_entrypoint.h"
#include "scene/flipfrid_scene_load_file.h"
#include "scene/flipfrid_scene_select_field.h"
#include "scene/flipfrid_scene_run_attack.h"
#include "scene/flipfrid_scene_load_custom_uids.h"
#include <dolphin/dolphin.h>

#define RFIDFUZZER_APP_FOLDER "/ext/lrfid/rfidfuzzer"

static void flipfrid_draw_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    FlipFridState* flipfrid_state = ctx;
    furry_mutex_acquire(flipfrid_state->mutex, FurryWaitForever);

    // Draw correct Canvas
    switch(flipfrid_state->current_scene) {
    case NoneScene:
    case SceneEntryPoint:
        flipfrid_scene_entrypoint_on_draw(canvas, flipfrid_state);
        break;
    case SceneSelectFile:
        flipfrid_scene_load_file_on_draw(canvas, flipfrid_state);
        break;
    case SceneSelectField:
        flipfrid_scene_select_field_on_draw(canvas, flipfrid_state);
        break;
    case SceneAttack:
        flipfrid_scene_run_attack_on_draw(canvas, flipfrid_state);
        break;
    case SceneLoadCustomUids:
        flipfrid_scene_load_custom_uids_on_draw(canvas, flipfrid_state);
        break;
    default:
        break;
    }

    furry_mutex_release(flipfrid_state->mutex);
}

void flipfrid_input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    FlipFridEvent event = {
        .evt_type = EventTypeKey, .key = input_event->key, .input_type = input_event->type};
    furry_message_queue_put(event_queue, &event, 25);
}

static void flipfrid_timer_callback(FurryMessageQueue* event_queue) {
    furry_assert(event_queue);
    FlipFridEvent event = {
        .evt_type = EventTypeTick, .key = InputKeyUp, .input_type = InputTypeRelease};
    furry_message_queue_put(event_queue, &event, 25);
}

FlipFridState* flipfrid_alloc() {
    FlipFridState* flipfrid = malloc(sizeof(FlipFridState));
    flipfrid->notification_msg = furry_string_alloc();
    flipfrid->attack_name = furry_string_alloc();
    flipfrid->proto_name = furry_string_alloc();
    flipfrid->data_str = furry_string_alloc();

    flipfrid->main_menu_items[0] = furry_string_alloc_set("Default Values");
    flipfrid->main_menu_items[1] = furry_string_alloc_set("BF Customer ID");
    flipfrid->main_menu_items[2] = furry_string_alloc_set("Load File");
    flipfrid->main_menu_items[3] = furry_string_alloc_set("Load UIDs from file");

    flipfrid->main_menu_proto_items[0] = furry_string_alloc_set("EM4100");
    flipfrid->main_menu_proto_items[1] = furry_string_alloc_set("HIDProx");
    flipfrid->main_menu_proto_items[2] = furry_string_alloc_set("PAC/Stanley");
    flipfrid->main_menu_proto_items[3] = furry_string_alloc_set("H10301");

    flipfrid->previous_scene = NoneScene;
    flipfrid->current_scene = SceneEntryPoint;
    flipfrid->is_running = true;
    flipfrid->is_attacking = false;
    flipfrid->key_index = 0;
    flipfrid->menu_index = 0;
    flipfrid->menu_proto_index = 0;

    flipfrid->attack = FlipFridAttackDefaultValues;
    flipfrid->notify = furry_record_open(RECORD_NOTIFICATION);

    flipfrid->data[0] = 0x00;
    flipfrid->data[1] = 0x00;
    flipfrid->data[2] = 0x00;
    flipfrid->data[3] = 0x00;
    flipfrid->data[4] = 0x00;
    flipfrid->data[5] = 0x00;

    flipfrid->payload[0] = 0x00;
    flipfrid->payload[1] = 0x00;
    flipfrid->payload[2] = 0x00;
    flipfrid->payload[3] = 0x00;
    flipfrid->payload[4] = 0x00;
    flipfrid->payload[5] = 0x00;

    //Dialog
    flipfrid->dialogs = furry_record_open(RECORD_DIALOGS);

    return flipfrid;
}

void flipfrid_free(FlipFridState* flipfrid) {
    //Dialog
    furry_record_close(RECORD_DIALOGS);
    notification_message(flipfrid->notify, &sequence_blink_stop);

    // Strings
    furry_string_free(flipfrid->notification_msg);
    furry_string_free(flipfrid->attack_name);
    furry_string_free(flipfrid->proto_name);
    furry_string_free(flipfrid->data_str);

    for(uint32_t i = 0; i < 4; i++) {
        furry_string_free(flipfrid->main_menu_items[i]);
    }

    for(uint32_t i = 0; i < 4; i++) {
        furry_string_free(flipfrid->main_menu_proto_items[i]);
    }

    free(flipfrid->data);
    free(flipfrid->payload);

    // The rest
    free(flipfrid);
}

// ENTRYPOINT
int32_t flipfrid_start(void* p) {
    UNUSED(p);
    // Input
    FURRY_LOG_I(TAG, "Initializing input");
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(FlipFridEvent));
    FlipFridState* flipfrid_state = flipfrid_alloc();

    DOLPHIN_DEED(DolphinDeedPluginStart);
    flipfrid_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!flipfrid_state->mutex) {
        FURRY_LOG_E(TAG, "cannot create mutex\r\n");
        furry_message_queue_free(event_queue);
        furry_record_close(RECORD_NOTIFICATION);
        flipfrid_free(flipfrid_state);
        return 255;
    }

    Storage* storage = furry_record_open(RECORD_STORAGE);
    if(!storage_simply_mkdir(storage, RFIDFUZZER_APP_FOLDER)) {
        FURRY_LOG_E(TAG, "Could not create folder %s", RFIDFUZZER_APP_FOLDER);
    }
    furry_record_close(RECORD_STORAGE);

    // Configure view port
    FURRY_LOG_I(TAG, "Initializing viewport");
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, flipfrid_draw_callback, flipfrid_state);
    view_port_input_callback_set(view_port, flipfrid_input_callback, event_queue);

    // Configure timer
    FURRY_LOG_I(TAG, "Initializing timer");
    FurryTimer* timer =
        furry_timer_alloc(flipfrid_timer_callback, FurryTimerTypePeriodic, event_queue);
    furry_timer_start(timer, furry_kernel_get_tick_frequency() / 10); // 10 times per second

    // Register view port in GUI
    FURRY_LOG_I(TAG, "Initializing gui");
    Gui* gui = (Gui*)furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Init values
    FlipFridEvent event;
    while(flipfrid_state->is_running) {
        // Get next event
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 25);
        if(event_status == FurryStatusOk) {
            if(event.evt_type == EventTypeKey) {
                //Handle event key
                switch(flipfrid_state->current_scene) {
                case NoneScene:
                case SceneEntryPoint:
                    flipfrid_scene_entrypoint_on_event(event, flipfrid_state);
                    break;
                case SceneSelectFile:
                    flipfrid_scene_load_file_on_event(event, flipfrid_state);
                    break;
                case SceneSelectField:
                    flipfrid_scene_select_field_on_event(event, flipfrid_state);
                    break;
                case SceneAttack:
                    flipfrid_scene_run_attack_on_event(event, flipfrid_state);
                    break;
                case SceneLoadCustomUids:
                    flipfrid_scene_load_custom_uids_on_event(event, flipfrid_state);
                    break;
                default:
                    break;
                }

            } else if(event.evt_type == EventTypeTick) {
                //Handle event tick
                if(flipfrid_state->current_scene != flipfrid_state->previous_scene) {
                    // Trigger Exit Scene
                    switch(flipfrid_state->previous_scene) {
                    case SceneEntryPoint:
                        flipfrid_scene_entrypoint_on_exit(flipfrid_state);
                        break;
                    case SceneSelectFile:
                        flipfrid_scene_load_file_on_exit(flipfrid_state);
                        break;
                    case SceneSelectField:
                        flipfrid_scene_select_field_on_exit(flipfrid_state);
                        break;
                    case SceneAttack:
                        flipfrid_scene_run_attack_on_exit(flipfrid_state);
                        break;
                    case SceneLoadCustomUids:
                        flipfrid_scene_load_custom_uids_on_exit(flipfrid_state);
                        break;
                    case NoneScene:
                        break;
                    default:
                        break;
                    }

                    // Trigger Entry Scene
                    switch(flipfrid_state->current_scene) {
                    case NoneScene:
                    case SceneEntryPoint:
                        flipfrid_scene_entrypoint_on_enter(flipfrid_state);
                        break;
                    case SceneSelectFile:
                        flipfrid_scene_load_file_on_enter(flipfrid_state);
                        break;
                    case SceneSelectField:
                        flipfrid_scene_select_field_on_enter(flipfrid_state);
                        break;
                    case SceneAttack:
                        flipfrid_scene_run_attack_on_enter(flipfrid_state);
                        break;
                    case SceneLoadCustomUids:
                        flipfrid_scene_load_custom_uids_on_enter(flipfrid_state);
                        break;
                    default:
                        break;
                    }
                    flipfrid_state->previous_scene = flipfrid_state->current_scene;
                }

                // Trigger Tick Scene
                switch(flipfrid_state->current_scene) {
                case NoneScene:
                case SceneEntryPoint:
                    flipfrid_scene_entrypoint_on_tick(flipfrid_state);
                    break;
                case SceneSelectFile:
                    flipfrid_scene_load_file_on_tick(flipfrid_state);
                    break;
                case SceneSelectField:
                    flipfrid_scene_select_field_on_tick(flipfrid_state);
                    break;
                case SceneAttack:
                    flipfrid_scene_run_attack_on_tick(flipfrid_state);
                    break;
                case SceneLoadCustomUids:
                    flipfrid_scene_load_custom_uids_on_tick(flipfrid_state);
                    break;
                default:
                    break;
                }
                view_port_update(view_port);
            }
        }
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
    furry_mutex_free(flipfrid_state->mutex);
    flipfrid_free(flipfrid_state);

    return 0;
}
