#include "../bad_kb_app.h"
#include "furry_hal_power.h"
#include "furry_hal_usb.h"
#include <storage/storage.h>

static bool bad_kb_layout_select(BadKbApp* bad_kb) {
    furry_assert(bad_kb);

    FurryString* predefined_path;
    predefined_path = furry_string_alloc();
    if(!furry_string_empty(bad_kb->keyboard_layout)) {
        furry_string_set(predefined_path, bad_kb->keyboard_layout);
    } else {
        furry_string_set(predefined_path, BAD_KB_APP_PATH_LAYOUT_FOLDER);
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(
        &browser_options, BAD_KB_APP_LAYOUT_EXTENSION, &I_keyboard_10px);
    browser_options.base_path = BAD_KB_APP_PATH_LAYOUT_FOLDER;
    browser_options.skip_assets = false;

    // Input events and views are managed by file_browser
    bool res = dialog_file_browser_show(
        bad_kb->dialogs, bad_kb->keyboard_layout, predefined_path, &browser_options);

    furry_string_free(predefined_path);
    return res;
}

void bad_kb_scene_config_layout_on_enter(void* context) {
    BadKbApp* bad_kb = context;

    if(bad_kb_layout_select(bad_kb)) {
        bad_kb_script_set_keyboard_layout(bad_kb->bad_kb_script, bad_kb->keyboard_layout);
    }
    scene_manager_previous_scene(bad_kb->scene_manager);
}

bool bad_kb_scene_config_layout_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // BadKbApp* bad_kb = context;
    return false;
}

void bad_kb_scene_config_layout_on_exit(void* context) {
    UNUSED(context);
    // BadKbApp* bad_kb = context;
}
