#include "../subghz_i.h"
#include "../views/receiver.h"
#include <lib/subghz/protocols/raw.h>

#include <lib/subghz/subghz_file_encoder_worker.h>

#define TAG "SubGhzDecodeRaw"
#define SAMPLES_TO_READ_PER_TICK 400

// TODO:
// [X] Remember RAW file after decoding
// [X] Decode in tick events instead of on_enter
// [X] Make "Config" label optional in subghz_view_receiver_draw (../views/receiver.c)
// [X] Make "Scanning..." label optional in subghz_view_receiver_draw (../views/receiver.c)
// [X] Add Decoding logo
// [ ] Design nicer Decoding logo
// [X] Check progress in stream_buffer, instead of raw stream
// [X] Blink led while decoding
// [X] Stop rx blink (blue, fast) on history item view
// [X] Don't reparse file on back
// [X] Fix: RX animation+LED returning from decoded detail view
// [X] Find good value for SAMPLES_TO_READ_PER_TICK
// [X] Fix: read errors (slow flash) after aborting decode read

static void subghz_scene_receiver_update_statusbar(void* context) {
    SubGhz* subghz = context;
    FurryString* history_stat_str;
    history_stat_str = furry_string_alloc();
    if(!subghz_history_get_text_space_left(subghz->txrx->history, history_stat_str)) {
        FurryString* frequency_str;
        FurryString* modulation_str;

        frequency_str = furry_string_alloc();
        modulation_str = furry_string_alloc();

        subghz_get_frequency_modulation(subghz, frequency_str, modulation_str);

        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver,
            furry_string_get_cstr(frequency_str),
            furry_string_get_cstr(modulation_str),
            furry_string_get_cstr(history_stat_str));

        furry_string_free(frequency_str);
        furry_string_free(modulation_str);
    } else {
        subghz_view_receiver_add_data_statusbar(
            subghz->subghz_receiver, furry_string_get_cstr(history_stat_str), "", "");
    }
    furry_string_free(history_stat_str);
}

void subghz_scene_decode_raw_callback(SubGhzCustomEvent event, void* context) {
    furry_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void subghz_scene_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furry_assert(context);
    SubGhz* subghz = context;
    FurryString* item_name;
    FurryString* item_time;
    uint16_t idx = subghz_history_get_item(subghz->txrx->history);
    item_name = furry_string_alloc();
    item_time = furry_string_alloc();

    if(subghz_history_add_to_history(subghz->txrx->history, decoder_base, subghz->txrx->preset)) {
        furry_string_reset(item_name);
        furry_string_reset(item_time);

        subghz->state_notifications = SubGhzNotificationStateRxDone;

        subghz_history_get_text_item_menu(subghz->txrx->history, item_name, idx);
        subghz_history_get_time_item_menu(subghz->txrx->history, item_time, idx);
        subghz_view_receiver_add_item_to_menu(
            subghz->subghz_receiver,
            furry_string_get_cstr(item_name),
            furry_string_get_cstr(item_time),
            subghz_history_get_type_protocol(subghz->txrx->history, idx));

        subghz_scene_receiver_update_statusbar(subghz);
    }
    subghz_receiver_reset(receiver);
    furry_string_free(item_name);
    furry_string_free(item_time);
}

bool subghz_scene_decode_raw_start(SubGhz* subghz) {
    FurryString* file_name;
    file_name = furry_string_alloc();
    bool success = false;
    do {
        if(!flipper_format_rewind(subghz->txrx->fff_data)) {
            FURRY_LOG_E(TAG, "Rewind error");
            break;
        }

        if(!flipper_format_read_string(subghz->txrx->fff_data, "File_name", file_name)) {
            FURRY_LOG_E(TAG, "Missing File_name");
            break;
        }

        success = true;
    } while(false);

    if(success) {
        //FURRY_LOG_I(TAG, "Listening at \033[0;33m%s\033[0m.", furry_string_get_cstr(file_name));

        subghz->decode_raw_file_worker_encoder = subghz_file_encoder_worker_alloc();
        if(subghz_file_encoder_worker_start(
               subghz->decode_raw_file_worker_encoder, furry_string_get_cstr(file_name))) {
            //the worker needs a file in order to open and read part of the file
            furry_delay_ms(100);
        } else {
            success = false;
        }

        if(!success) {
            subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);
        }
    }

    furry_string_free(file_name);
    return success;
}

bool subghz_scene_decode_raw_next(SubGhz* subghz) {
    LevelDuration level_duration;

    for(uint32_t read = SAMPLES_TO_READ_PER_TICK; read > 0; --read) {
        level_duration =
            subghz_file_encoder_worker_get_level_duration(subghz->decode_raw_file_worker_encoder);
        if(!level_duration_is_reset(level_duration)) {
            bool level = level_duration_get_level(level_duration);
            uint32_t duration = level_duration_get_duration(level_duration);
            subghz_receiver_decode(subghz->txrx->receiver, level, duration);
        } else {
            subghz->decode_raw_state = SubGhzDecodeRawStateLoaded;
            subghz->state_notifications = SubGhzNotificationStateIDLE;

            subghz_view_receiver_add_data_progress(subghz->subghz_receiver, "Done!");
            return false; // No more samples available
        }
    }

    // Update progress info
    FurryString* progress_str;
    progress_str = furry_string_alloc();
    subghz_file_encoder_worker_get_text_progress(
        subghz->decode_raw_file_worker_encoder, progress_str);

    subghz_view_receiver_add_data_progress(
        subghz->subghz_receiver, furry_string_get_cstr(progress_str));

    furry_string_free(progress_str);

    return true; // More samples available
}

void subghz_scene_decode_raw_on_enter(void* context) {
    SubGhz* subghz = context;

    FurryString* item_name;
    FurryString* item_time;
    item_name = furry_string_alloc();
    item_time = furry_string_alloc();

    subghz_view_receiver_set_lock(subghz->subghz_receiver, subghz->lock);
    subghz_view_receiver_set_mode(subghz->subghz_receiver, SubGhzViewReceiverModeFile);
    subghz_view_receiver_set_callback(
        subghz->subghz_receiver, subghz_scene_decode_raw_callback, subghz);

    subghz_receiver_set_rx_callback(
        subghz->txrx->receiver, subghz_scene_add_to_history_callback, subghz);

    subghz_receiver_set_filter(subghz->txrx->receiver, SubGhzProtocolFlag_Decodable);

    if(subghz->decode_raw_state == SubGhzDecodeRawStateStart) {
        //Decode RAW to history
        subghz_history_reset(subghz->txrx->history);
        if(subghz_scene_decode_raw_start(subghz)) {
            subghz->decode_raw_state = SubGhzDecodeRawStateLoading;
            subghz->state_notifications = SubGhzNotificationStateRx;
        }
    } else {
        //Load history to receiver
        subghz_view_receiver_exit(subghz->subghz_receiver);
        for(uint8_t i = 0; i < subghz_history_get_item(subghz->txrx->history); i++) {
            furry_string_reset(item_name);
            furry_string_reset(item_time);
            subghz_history_get_text_item_menu(subghz->txrx->history, item_name, i);
            subghz_history_get_time_item_menu(subghz->txrx->history, item_time, i);
            subghz_view_receiver_add_item_to_menu(
                subghz->subghz_receiver,
                furry_string_get_cstr(item_name),
                furry_string_get_cstr(item_time),
                subghz_history_get_type_protocol(subghz->txrx->history, i));
        }
        furry_string_free(item_name);
        furry_string_free(item_time);
        subghz_view_receiver_set_idx_menu(subghz->subghz_receiver, subghz->txrx->idx_menu_chosen);
    }

    subghz_scene_receiver_update_statusbar(subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdReceiver);
}

bool subghz_scene_decode_raw_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubGhzCustomEventViewReceiverBack:
            subghz->decode_raw_state = SubGhzDecodeRawStateStart;
            subghz->txrx->idx_menu_chosen = 0;
            subghz->in_decoder_scene = false;
            subghz->in_decoder_scene_skip = false;

            subghz_receiver_set_rx_callback(subghz->txrx->receiver, NULL, subghz);

            if(subghz_file_encoder_worker_is_running(subghz->decode_raw_file_worker_encoder)) {
                subghz_file_encoder_worker_stop(subghz->decode_raw_file_worker_encoder);
            }
            subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);

            subghz->state_notifications = SubGhzNotificationStateIDLE;
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerNoSet);
            scene_manager_search_and_switch_to_previous_scene(
                subghz->scene_manager, SubGhzSceneMoreRAW);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOK:
            subghz->txrx->idx_menu_chosen =
                subghz_view_receiver_get_idx_menu(subghz->subghz_receiver);
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz->in_decoder_scene = true;
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneReceiverInfo);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverConfig:
            FURRY_LOG_W(TAG, "No config options");
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverOffDisplay:
            notification_message(subghz->notifications, &sequence_display_backlight_off);
            consumed = true;
            break;
        case SubGhzCustomEventViewReceiverUnlock:
            subghz->lock = SubGhzLockOff;
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        switch(subghz->state_notifications) {
        case SubGhzNotificationStateRx:
            notification_message(subghz->notifications, &sequence_blink_cyan_10);
            break;
        case SubGhzNotificationStateRxDone:
            notification_message(subghz->notifications, &subghz_sequence_rx);
            subghz->state_notifications = SubGhzNotificationStateRx;
            break;
        default:
            break;
        }

        switch(subghz->decode_raw_state) {
        case SubGhzDecodeRawStateLoading:
            subghz_scene_decode_raw_next(subghz);
            break;
        default:
            break;
        }
    }
    return consumed;
}

void subghz_scene_decode_raw_on_exit(void* context) {
    UNUSED(context);
}
