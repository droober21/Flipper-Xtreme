#include "ibtnfuzzer_scene_load_file.h"
#include "ibtnfuzzer_scene_entrypoint.h"

#define IBUTTON_FUZZER_APP_EXTENSION ".ibtn"
#define IBUTTON_FUZZER_APP_PATH_FOLDER "/ext/ibutton"

bool ibtnfuzzer_load(iBtnFuzzerState* context, const char* file_path) {
    bool result = false;
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);
    FurryString* temp_str;
    temp_str = furry_string_alloc();
    do {
        if(!flipper_format_file_open_existing(fff_data_file, file_path)) {
            FURRY_LOG_E(TAG, "Error open file %s", file_path);
            furry_string_reset(context->notification_msg);
            furry_string_set(context->notification_msg, "Error open file");
            break;
        }

        // FileType
        if(!flipper_format_read_string(fff_data_file, "Filetype", temp_str)) {
            FURRY_LOG_E(TAG, "Missing or incorrect Filetype");
            furry_string_reset(context->notification_msg);
            furry_string_set(context->notification_msg, "Missing or incorrect Filetypes");
            break;
        } else {
            FURRY_LOG_I(TAG, "Filetype: %s", furry_string_get_cstr(temp_str));
        }

        // Key type
        if(!flipper_format_read_string(fff_data_file, "Key type", temp_str)) {
            FURRY_LOG_E(TAG, "Missing or incorrect Key type");
            furry_string_reset(context->notification_msg);
            furry_string_set(context->notification_msg, "Missing or incorrect Key type");
            break;
        } else {
            FURRY_LOG_I(TAG, "Key type: %s", furry_string_get_cstr(temp_str));

            if(context->proto == DS1990) {
                if(strcmp(furry_string_get_cstr(temp_str), "Dallas") != 0) {
                    FURRY_LOG_E(TAG, "Unsupported Key type");
                    furry_string_reset(context->notification_msg);
                    furry_string_set(context->notification_msg, "Unsupported Key type");
                    break;
                }
            } else if(context->proto == Cyfral) {
                if(strcmp(furry_string_get_cstr(temp_str), "Cyfral") != 0) {
                    FURRY_LOG_E(TAG, "Unsupported Key type");
                    furry_string_reset(context->notification_msg);
                    furry_string_set(context->notification_msg, "Unsupported Key type");
                    break;
                }
            } else {
                if(strcmp(furry_string_get_cstr(temp_str), "Metakom") != 0) {
                    FURRY_LOG_E(TAG, "Unsupported Key type");
                    furry_string_reset(context->notification_msg);
                    furry_string_set(context->notification_msg, "Unsupported Key type");
                    break;
                }
            }
        }

        // Data
        if(!flipper_format_read_string(fff_data_file, "Data", context->data_str)) {
            FURRY_LOG_E(TAG, "Missing or incorrect Data");
            furry_string_reset(context->notification_msg);
            furry_string_set(context->notification_msg, "Missing or incorrect Key");
            break;
        } else {
            FURRY_LOG_I(TAG, "Key: %s", furry_string_get_cstr(context->data_str));

            if(context->proto == DS1990) {
                if(furry_string_size(context->data_str) != 23) {
                    FURRY_LOG_E(TAG, "Incorrect Key length");
                    furry_string_reset(context->notification_msg);
                    furry_string_set(context->notification_msg, "Incorrect Key length");
                    break;
                }
            } else if(context->proto == Cyfral) {
                if(furry_string_size(context->data_str) != 5) {
                    FURRY_LOG_E(TAG, "Incorrect Key length");
                    furry_string_reset(context->notification_msg);
                    furry_string_set(context->notification_msg, "Incorrect Key length");
                    break;
                }
            } else {
                if(furry_string_size(context->data_str) != 11) {
                    FURRY_LOG_E(TAG, "Incorrect Key length");
                    furry_string_reset(context->notification_msg);
                    furry_string_set(context->notification_msg, "Incorrect Key length");
                    break;
                }
            }

            // String to uint8_t
            for(uint8_t i = 0; i < 8; i++) {
                char temp_str2[3];
                temp_str2[0] = furry_string_get_cstr(context->data_str)[i * 3];
                temp_str2[1] = furry_string_get_cstr(context->data_str)[i * 3 + 1];
                temp_str2[2] = '\0';
                context->data[i] = (uint8_t)strtol(temp_str2, NULL, 16);
            }
        }

        result = true;
    } while(0);
    furry_string_free(temp_str);
    flipper_format_free(fff_data_file);
    if(result) {
        FURRY_LOG_I(TAG, "Loaded successfully");
        furry_string_reset(context->notification_msg);
        furry_string_set(context->notification_msg, "Source loaded.");
    }
    return result;
}

void ibtnfuzzer_scene_load_file_on_enter(iBtnFuzzerState* context) {
    if(ibtnfuzzer_load_protocol_from_file(context)) {
        context->current_scene = SceneSelectField;
    } else {
        ibtnfuzzer_scene_entrypoint_on_enter(context);
        context->current_scene = SceneEntryPoint;
    }
}

void ibtnfuzzer_scene_load_file_on_exit(iBtnFuzzerState* context) {
    UNUSED(context);
}

void ibtnfuzzer_scene_load_file_on_tick(iBtnFuzzerState* context) {
    UNUSED(context);
}

void ibtnfuzzer_scene_load_file_on_event(iBtnFuzzerEvent event, iBtnFuzzerState* context) {
    if(event.evt_type == EventTypeKey) {
        if(event.input_type == InputTypeShort) {
            switch(event.key) {
            case InputKeyDown:
            case InputKeyUp:
            case InputKeyLeft:
            case InputKeyRight:
            case InputKeyOk:
            case InputKeyBack:
                context->current_scene = SceneEntryPoint;
                break;
            default:
                break;
            }
        }
    }
}

void ibtnfuzzer_scene_load_file_on_draw(Canvas* canvas, iBtnFuzzerState* context) {
    UNUSED(context);
    UNUSED(canvas);
}

bool ibtnfuzzer_load_protocol_from_file(iBtnFuzzerState* context) {
    FurryString* user_file_path;
    user_file_path = furry_string_alloc();
    furry_string_set(user_file_path, IBUTTON_FUZZER_APP_PATH_FOLDER);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(
        &browser_options, IBUTTON_FUZZER_APP_EXTENSION, &I_ibutt_10px);
    browser_options.base_path = IBUTTON_FUZZER_APP_PATH_FOLDER;

    // Input events and views are managed by file_select
    bool res = dialog_file_browser_show(
        context->dialogs, user_file_path, user_file_path, &browser_options);

    if(res) {
        res = ibtnfuzzer_load(context, furry_string_get_cstr(user_file_path));
    }

    furry_string_free(user_file_path);

    return res;
}