#include "../lfrfid_i.h"

#define TAG "ADC"

typedef struct {
    FurryString* string_file_name;
    bool error;
} LfRfidEmulateRawState;

static void lfrfid_raw_emulate_callback(LFRFIDWorkerEmulateRawResult result, void* context) {
    LfRfid* app = context;

    if(result == LFRFIDWorkerEmulateRawFileError) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LfRfidEventReadError);
    } else if(result == LFRFIDWorkerEmulateRawOverrun) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LfRfidEventReadOverrun);
    }
}

void lfrfid_scene_raw_emulate_on_enter(void* context) {
    LfRfid* app = context;
    Popup* popup = app->popup;

    LfRfidEmulateRawState* state = malloc(sizeof(LfRfidEmulateRawState));
    scene_manager_set_scene_state(app->scene_manager, LfRfidSceneRawEmulate, (uint32_t)state);
    state->string_file_name = furry_string_alloc();

    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
    view_dispatcher_switch_to_view(app->view_dispatcher, LfRfidViewPopup);
    lfrfid_worker_start_thread(app->lfworker);
    lfrfid_make_app_folder(app);

    furry_string_printf(
        state->string_file_name,
        "%s/%s%s",
        LFRFID_SD_FOLDER,
        furry_string_get_cstr(app->file_name),
        LFRFID_APP_RAW_ASK_EXTENSION);
    FURRY_LOG_D(TAG, "raw_emulate->file_name=%s", furry_string_get_cstr(state->string_file_name));
    popup_set_header(popup, "Emulating\nRAW RFID\nASK", 89, 30, AlignCenter, AlignTop);
    lfrfid_worker_emulate_raw_start(
        app->lfworker,
        furry_string_get_cstr(state->string_file_name),
        lfrfid_raw_emulate_callback,
        app);

    notification_message(app->notifications, &sequence_blink_start_cyan);

    state->error = false;
}

bool lfrfid_scene_raw_emulate_on_event(void* context, SceneManagerEvent event) {
    LfRfid* app = context;
    Popup* popup = app->popup;
    LfRfidEmulateRawState* state = (LfRfidEmulateRawState*)scene_manager_get_scene_state(
        app->scene_manager, LfRfidSceneRawEmulate);
    bool consumed = false;

    furry_assert(state);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LfRfidEventReadError) {
            consumed = true;
            state->error = true;
            popup_set_header(
                popup, "Reading\nRAW RFID\nFile error", 89, 30, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_blink_start_red);
        }
    }

    return consumed;
}

void lfrfid_scene_raw_emulate_on_exit(void* context) {
    LfRfid* app = context;
    LfRfidEmulateRawState* state = (LfRfidEmulateRawState*)scene_manager_get_scene_state(
        app->scene_manager, LfRfidSceneRawEmulate);

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    lfrfid_worker_stop(app->lfworker);
    lfrfid_worker_stop_thread(app->lfworker);

    furry_string_free(state->string_file_name);
    free(state);
}
