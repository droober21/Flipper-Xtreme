#include "../nfc_i.h"

#define NFC_SCENE_EMULATE_NFCV_LOG_SIZE_MAX (800)

enum {
    NfcSceneNfcVSniffStateWidget,
    NfcSceneNfcVSniffStateTextBox,
};

bool nfc_scene_nfcv_sniff_worker_callback(NfcWorkerEvent event, void* context) {
    UNUSED(event);
    furry_assert(context);
    Nfc* nfc = context;

    switch(event) {
    case NfcWorkerEventNfcVCommandExecuted:
        view_dispatcher_send_custom_event(nfc->view_dispatcher, NfcCustomEventUpdateLog);
        break;
    case NfcWorkerEventNfcVContentChanged:
        view_dispatcher_send_custom_event(nfc->view_dispatcher, NfcCustomEventSaveShadow);
        break;
    default:
        break;
    }
    return true;
}

void nfc_scene_nfcv_sniff_widget_callback(GuiButtonType result, InputType type, void* context) {
    furry_assert(context);
    Nfc* nfc = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc->view_dispatcher, result);
    }
}

void nfc_scene_nfcv_sniff_textbox_callback(void* context) {
    furry_assert(context);
    Nfc* nfc = context;
    view_dispatcher_send_custom_event(nfc->view_dispatcher, NfcCustomEventViewExit);
}

static void nfc_scene_nfcv_sniff_widget_config(Nfc* nfc, bool data_received) {
    Widget* widget = nfc->widget;
    widget_reset(widget);
    FurryString* info_str;
    info_str = furry_string_alloc();

    widget_add_icon_element(widget, 0, 3, &I_RFIDDolphinSend_97x61);
    widget_add_string_element(widget, 89, 32, AlignCenter, AlignTop, FontPrimary, "Listen NfcV");
    furry_string_trim(info_str);
    widget_add_text_box_element(
        widget, 56, 43, 70, 21, AlignCenter, AlignTop, furry_string_get_cstr(info_str), true);
    furry_string_free(info_str);
    if(data_received) {
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Log", nfc_scene_nfcv_sniff_widget_callback, nfc);
    }
}

void nfc_scene_nfcv_sniff_on_enter(void* context) {
    Nfc* nfc = context;

    // Setup Widget
    nfc_scene_nfcv_sniff_widget_config(nfc, false);
    // Setup TextBox
    TextBox* text_box = nfc->text_box;
    text_box_set_font(text_box, TextBoxFontHex);
    text_box_set_focus(text_box, TextBoxFocusEnd);
    text_box_set_text(text_box, "");
    furry_string_reset(nfc->text_box_store);

    // Set Widget state and view
    scene_manager_set_scene_state(
        nfc->scene_manager, NfcSceneNfcVSniff, NfcSceneNfcVSniffStateWidget);
    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
    // Start worker
    memset(&nfc->dev->dev_data.reader_data, 0, sizeof(NfcReaderRequestData));
    nfc_worker_start(
        nfc->worker,
        NfcWorkerStateNfcVSniff,
        &nfc->dev->dev_data,
        nfc_scene_nfcv_sniff_worker_callback,
        nfc);

    nfc_blink_emulate_start(nfc);
}

bool nfc_scene_nfcv_sniff_on_event(void* context, SceneManagerEvent event) {
    Nfc* nfc = context;
    NfcVData* nfcv_data = &nfc->dev->dev_data.nfcv_data;
    uint32_t state = scene_manager_get_scene_state(nfc->scene_manager, NfcSceneNfcVSniff);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCustomEventUpdateLog) {
            // Add data button to widget if data is received for the first time
            if(strlen(nfcv_data->last_command) > 0) {
                if(!furry_string_size(nfc->text_box_store)) {
                    nfc_scene_nfcv_sniff_widget_config(nfc, true);
                }
                /* use the last n bytes from the log so there's enough space for the new log entry */
                size_t maxSize =
                    NFC_SCENE_EMULATE_NFCV_LOG_SIZE_MAX - (strlen(nfcv_data->last_command) + 1);
                if(furry_string_size(nfc->text_box_store) >= maxSize) {
                    furry_string_right(nfc->text_box_store, (strlen(nfcv_data->last_command) + 1));
                }
                furry_string_cat_printf(nfc->text_box_store, "%s", nfcv_data->last_command);
                furry_string_push_back(nfc->text_box_store, '\n');
                text_box_set_text(nfc->text_box, furry_string_get_cstr(nfc->text_box_store));

                /* clear previously logged command */
                strcpy(nfcv_data->last_command, "");
            }
            consumed = true;
        } else if(event.event == NfcCustomEventSaveShadow) {
            if(furry_string_size(nfc->dev->load_path)) {
                nfc_device_save_shadow(nfc->dev, furry_string_get_cstr(nfc->dev->load_path));
            }
            consumed = true;
        } else if(event.event == GuiButtonTypeCenter && state == NfcSceneNfcVSniffStateWidget) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewTextBox);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneNfcVSniff, NfcSceneNfcVSniffStateTextBox);
            consumed = true;
        } else if(event.event == NfcCustomEventViewExit && state == NfcSceneNfcVSniffStateTextBox) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneNfcVSniff, NfcSceneNfcVSniffStateWidget);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(state == NfcSceneNfcVSniffStateTextBox) {
            view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewWidget);
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneNfcVSniff, NfcSceneNfcVSniffStateWidget);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_scene_nfcv_sniff_on_exit(void* context) {
    Nfc* nfc = context;

    // Stop worker
    nfc_worker_stop(nfc->worker);

    // Clear view
    widget_reset(nfc->widget);
    text_box_reset(nfc->text_box);
    furry_string_reset(nfc->text_box_store);

    nfc_blink_stop(nfc);
}
