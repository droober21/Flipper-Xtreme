#include "../xtreme_app.h"

enum FileBrowserResult {
    FileBrowserResultOk,
};

static bool xtreme_app_scene_interface_mainmenu_add_file_browser_callback(
    FurryString* file_path,
    void* context,
    uint8_t** icon_ptr,
    FurryString* item_name) {
    UNUSED(context);
    Storage* storage = furry_record_open(RECORD_STORAGE);
    bool success = fap_loader_load_name_and_icon(file_path, storage, icon_ptr, item_name);
    furry_record_close(RECORD_STORAGE);
    return success;
}

void xtreme_app_scene_interface_mainmenu_add_on_enter(void* context) {
    XtremeApp* app = context;
    FurryString* string = furry_string_alloc_set_str(EXT_PATH("apps"));

    const DialogsFileBrowserOptions browser_options = {
        .extension = ".fap",
        .skip_assets = true,
        .hide_ext = true,
        .item_loader_callback = xtreme_app_scene_interface_mainmenu_add_file_browser_callback,
        .item_loader_context = app,
        .base_path = EXT_PATH("apps"),
    };

    if(dialog_file_browser_show(app->dialogs, string, string, &browser_options)) {
        CharList_push_back(app->mainmenu_app_paths, strdup(furry_string_get_cstr(string)));
        Storage* storage = furry_record_open(RECORD_STORAGE);
        fap_loader_load_name_and_icon(string, storage, NULL, string);
        furry_record_close(RECORD_STORAGE);
        CharList_push_back(app->mainmenu_app_names, strdup(furry_string_get_cstr(string)));
        app->save_mainmenu_apps = true;
        app->require_reboot = true;
    }

    furry_string_free(string);

    view_dispatcher_send_custom_event(app->view_dispatcher, FileBrowserResultOk);
}

bool xtreme_app_scene_interface_mainmenu_add_on_event(void* context, SceneManagerEvent event) {
    XtremeApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case FileBrowserResultOk:
            scene_manager_previous_scene(app->scene_manager);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void xtreme_app_scene_interface_mainmenu_add_on_exit(void* context) {
    UNUSED(context);
}
