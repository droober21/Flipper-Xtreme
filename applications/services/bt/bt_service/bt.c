#include "bt_i.h"
#include "battery_service.h"
#include "bt_keys_storage.h"

#include <notification/notification_messages.h>
#include <gui/elements.h>
#include <assets_icons.h>
#include <xtreme.h>

#define TAG "BtSrv"

#define BT_RPC_EVENT_BUFF_SENT (1UL << 0)
#define BT_RPC_EVENT_DISCONNECTED (1UL << 1)
#define BT_RPC_EVENT_ALL (BT_RPC_EVENT_BUFF_SENT | BT_RPC_EVENT_DISCONNECTED)

static void bt_draw_statusbar_callback(Canvas* canvas, void* context) {
    furry_assert(context);

    Bt* bt = context;
    if(bt->status == BtStatusAdvertising) {
        canvas_draw_icon(canvas, 0, 0, &I_Bluetooth_Idle_5x8);
    } else if(bt->status == BtStatusConnected) {
        canvas_draw_icon(canvas, 0, 0, &I_Bluetooth_Connected_16x8);
    }
}

static ViewPort* bt_statusbar_view_port_alloc(Bt* bt) {
    ViewPort* statusbar_view_port = view_port_alloc();
    view_port_set_width(statusbar_view_port, 5);
    view_port_draw_callback_set(statusbar_view_port, bt_draw_statusbar_callback, bt);
    view_port_enabled_set(statusbar_view_port, false);
    return statusbar_view_port;
}

static void bt_pin_code_view_port_draw_callback(Canvas* canvas, void* context) {
    furry_assert(context);
    Bt* bt = context;
    char pin_code_info[24];
    canvas_draw_icon(canvas, 0, 0, XTREME_ASSETS()->I_BLE_Pairing_128x64);
    snprintf(pin_code_info, sizeof(pin_code_info), "Pairing code\n%06lu", bt->pin_code);
    elements_multiline_text_aligned(canvas, 64, 4, AlignCenter, AlignTop, pin_code_info);
    elements_button_left(canvas, "Quit");
}

static void bt_pin_code_view_port_input_callback(InputEvent* event, void* context) {
    furry_assert(context);
    Bt* bt = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft || event->key == InputKeyBack) {
            view_port_enabled_set(bt->pin_code_view_port, false);
        }
    }
}

static ViewPort* bt_pin_code_view_port_alloc(Bt* bt) {
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, bt_pin_code_view_port_draw_callback, bt);
    view_port_input_callback_set(view_port, bt_pin_code_view_port_input_callback, bt);
    view_port_enabled_set(view_port, false);
    return view_port;
}

static void bt_pin_code_show(Bt* bt, uint32_t pin_code) {
    furry_assert(bt);
    bt->pin_code = pin_code;
    notification_message(bt->notification, &sequence_display_backlight_on);
    if(bt->suppress_pin_screen) return;

    gui_view_port_send_to_front(bt->gui, bt->pin_code_view_port);
    view_port_enabled_set(bt->pin_code_view_port, true);
}

static void bt_pin_code_hide(Bt* bt) {
    bt->pin_code = 0;
    if(view_port_is_enabled(bt->pin_code_view_port)) {
        view_port_enabled_set(bt->pin_code_view_port, false);
    }
}

static bool bt_pin_code_verify_event_handler(Bt* bt, uint32_t pin) {
    furry_assert(bt);
    bt->pin_code = pin;
    notification_message(bt->notification, &sequence_display_backlight_on);
    if(bt->suppress_pin_screen) return true;

    FurryString* pin_str;
    dialog_message_set_icon(bt->dialog_message, XTREME_ASSETS()->I_BLE_Pairing_128x64, 0, 0);
    pin_str = furry_string_alloc_printf("Verify code\n%06lu", pin);
    dialog_message_set_text(
        bt->dialog_message, furry_string_get_cstr(pin_str), 64, 4, AlignCenter, AlignTop);
    dialog_message_set_buttons(bt->dialog_message, "Cancel", "OK", NULL);
    DialogMessageButton button = dialog_message_show(bt->dialogs, bt->dialog_message);
    furry_string_free(pin_str);

    return button == DialogMessageButtonCenter;
}

static void bt_battery_level_changed_callback(const void* _event, void* context) {
    furry_assert(_event);
    furry_assert(context);

    Bt* bt = context;
    BtMessage message = {};
    const PowerEvent* event = _event;
    if(event->type == PowerEventTypeBatteryLevelChanged) {
        message.type = BtMessageTypeUpdateBatteryLevel;
        message.data.battery_level = event->data.battery_level;
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
    } else if(
        event->type == PowerEventTypeStartCharging || event->type == PowerEventTypeFullyCharged ||
        event->type == PowerEventTypeStopCharging) {
        message.type = BtMessageTypeUpdatePowerState;
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
    }
}

Bt* bt_alloc() {
    Bt* bt = malloc(sizeof(Bt));
    // Init default maximum packet size
    bt->max_packet_size = FURRY_HAL_BT_SERIAL_PACKET_SIZE_MAX;
    bt->profile = BtProfileSerial;
    // Load settings
    if(!bt_settings_load(&bt->bt_settings)) {
        bt_settings_save(&bt->bt_settings);
    }
    // Keys storage
    Storage* storage = furry_record_open(RECORD_STORAGE);
    storage_common_copy(storage, BT_KEYS_STORAGE_OLD_PATH, BT_KEYS_STORAGE_PATH);
    storage_common_remove(storage, BT_KEYS_STORAGE_OLD_PATH);
    furry_record_close(RECORD_STORAGE);
    bt->keys_storage = bt_keys_storage_alloc(BT_KEYS_STORAGE_PATH);
    // Alloc queue
    bt->message_queue = furry_message_queue_alloc(8, sizeof(BtMessage));

    // Setup statusbar view port
    bt->statusbar_view_port = bt_statusbar_view_port_alloc(bt);
    // Pin code view port
    bt->pin_code_view_port = bt_pin_code_view_port_alloc(bt);
    // Notification
    bt->notification = furry_record_open(RECORD_NOTIFICATION);
    // Gui
    bt->gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(bt->gui, bt->statusbar_view_port, GuiLayerStatusBarLeft);
    gui_add_view_port(bt->gui, bt->pin_code_view_port, GuiLayerFullscreen);

    // Dialogs
    bt->dialogs = furry_record_open(RECORD_DIALOGS);
    bt->dialog_message = dialog_message_alloc();

    // Power
    bt->power = furry_record_open(RECORD_POWER);
    FurryPubSub* power_pubsub = power_get_pubsub(bt->power);
    furry_pubsub_subscribe(power_pubsub, bt_battery_level_changed_callback, bt);

    // RPC
    bt->rpc = furry_record_open(RECORD_RPC);
    bt->rpc_event = furry_event_flag_alloc();

    // API evnent
    bt->api_event = furry_event_flag_alloc();

    bt->pin = 0;

    return bt;
}

// Called from GAP thread from Serial service
static uint16_t bt_serial_event_callback(SerialServiceEvent event, void* context) {
    furry_assert(context);
    Bt* bt = context;
    uint16_t ret = 0;

    if(event.event == SerialServiceEventTypeDataReceived) {
        size_t bytes_processed =
            rpc_session_feed(bt->rpc_session, event.data.buffer, event.data.size, 1000);
        if(bytes_processed != event.data.size) {
            FURRY_LOG_E(
                TAG, "Only %zu of %u bytes processed by RPC", bytes_processed, event.data.size);
        }
        ret = rpc_session_get_available_size(bt->rpc_session);
    } else if(event.event == SerialServiceEventTypeDataSent) {
        furry_event_flag_set(bt->rpc_event, BT_RPC_EVENT_BUFF_SENT);
    } else if(event.event == SerialServiceEventTypesBleResetRequest) {
        FURRY_LOG_I(TAG, "BLE restart request received");
        BtMessage message = {.type = BtMessageTypeSetProfile, .data.profile = BtProfileSerial};
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
    }
    return ret;
}

// Called from RPC thread
static void bt_rpc_send_bytes_callback(void* context, uint8_t* bytes, size_t bytes_len) {
    furry_assert(context);
    Bt* bt = context;

    if(furry_event_flag_get(bt->rpc_event) & BT_RPC_EVENT_DISCONNECTED) {
        // Early stop from sending if we're already disconnected
        return;
    }
    furry_event_flag_clear(bt->rpc_event, BT_RPC_EVENT_ALL & (~BT_RPC_EVENT_DISCONNECTED));
    size_t bytes_sent = 0;
    while(bytes_sent < bytes_len) {
        size_t bytes_remain = bytes_len - bytes_sent;
        if(bytes_remain > bt->max_packet_size) {
            furry_hal_bt_serial_tx(&bytes[bytes_sent], bt->max_packet_size);
            bytes_sent += bt->max_packet_size;
        } else {
            furry_hal_bt_serial_tx(&bytes[bytes_sent], bytes_remain);
            bytes_sent += bytes_remain;
        }
        // We want BT_RPC_EVENT_DISCONNECTED to stick, so don't clear
        uint32_t event_flag = furry_event_flag_wait(
            bt->rpc_event, BT_RPC_EVENT_ALL, FurryFlagWaitAny | FurryFlagNoClear, FurryWaitForever);
        if(event_flag & BT_RPC_EVENT_DISCONNECTED) {
            break;
        } else {
            // If we didn't get BT_RPC_EVENT_DISCONNECTED, then clear everything else
            furry_event_flag_clear(bt->rpc_event, BT_RPC_EVENT_ALL & (~BT_RPC_EVENT_DISCONNECTED));
        }
    }
}

// Called from GAP thread
static bool bt_on_gap_event_callback(GapEvent event, void* context) {
    furry_assert(context);
    Bt* bt = context;
    bool ret = false;
    bt->pin = 0;

    if(event.type == GapEventTypeConnected) {
        // Update status bar
        bt->status = BtStatusConnected;
        BtMessage message = {.type = BtMessageTypeUpdateStatus};
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
        // Clear BT_RPC_EVENT_DISCONNECTED because it might be set from previous session
        furry_event_flag_clear(bt->rpc_event, BT_RPC_EVENT_DISCONNECTED);
        if(bt->profile == BtProfileSerial) {
            // Open RPC session
            bt->rpc_session = rpc_session_open(bt->rpc, RpcOwnerBle);
            if(bt->rpc_session) {
                FURRY_LOG_I(TAG, "Open RPC connection");
                rpc_session_set_send_bytes_callback(bt->rpc_session, bt_rpc_send_bytes_callback);
                rpc_session_set_buffer_is_empty_callback(
                    bt->rpc_session, furry_hal_bt_serial_notify_buffer_is_empty);
                rpc_session_set_context(bt->rpc_session, bt);
                furry_hal_bt_serial_set_event_callback(
                    RPC_BUFFER_SIZE, bt_serial_event_callback, bt);
                furry_hal_bt_serial_set_rpc_status(FurryHalBtSerialRpcStatusActive);
            } else {
                FURRY_LOG_W(TAG, "RPC is busy, failed to open new session");
            }
        }
        // Update battery level
        PowerInfo info;
        power_get_info(bt->power, &info);
        message.type = BtMessageTypeUpdateBatteryLevel;
        message.data.battery_level = info.charge;
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
        ret = true;
    } else if(event.type == GapEventTypeDisconnected) {
        if(bt->profile == BtProfileSerial && bt->rpc_session) {
            FURRY_LOG_I(TAG, "Close RPC connection");
            furry_hal_bt_serial_set_rpc_status(FurryHalBtSerialRpcStatusNotActive);
            furry_event_flag_set(bt->rpc_event, BT_RPC_EVENT_DISCONNECTED);
            rpc_session_close(bt->rpc_session);
            furry_hal_bt_serial_set_event_callback(0, NULL, NULL);
            bt->rpc_session = NULL;
        }
        ret = true;
    } else if(event.type == GapEventTypeStartAdvertising) {
        bt->status = BtStatusAdvertising;
        BtMessage message = {.type = BtMessageTypeUpdateStatus};
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
        ret = true;
    } else if(event.type == GapEventTypeStopAdvertising) {
        bt->status = BtStatusOff;
        BtMessage message = {.type = BtMessageTypeUpdateStatus};
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
        ret = true;
    } else if(event.type == GapEventTypePinCodeShow) {
        bt->pin = event.data.pin_code;
        BtMessage message = {
            .type = BtMessageTypePinCodeShow, .data.pin_code = event.data.pin_code};
        furry_check(
            furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
        ret = true;
    } else if(event.type == GapEventTypePinCodeVerify) {
        bt->pin = event.data.pin_code;
        ret = bt_pin_code_verify_event_handler(bt, event.data.pin_code);
    } else if(event.type == GapEventTypeUpdateMTU) {
        bt->max_packet_size = event.data.max_packet_size;
        ret = true;
    }
    return ret;
}

static void bt_on_key_storage_change_callback(uint8_t* addr, uint16_t size, void* context) {
    furry_assert(context);
    Bt* bt = context;
    BtMessage message = {
        .type = BtMessageTypeKeysStorageUpdated,
        .data.key_storage_data.start_address = addr,
        .data.key_storage_data.size = size};
    furry_check(
        furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
}

static void bt_statusbar_update(Bt* bt) {
    if(bt->status == BtStatusAdvertising) {
        view_port_set_width(bt->statusbar_view_port, icon_get_width(&I_Bluetooth_Idle_5x8));
        view_port_enabled_set(bt->statusbar_view_port, true);
    } else if(bt->status == BtStatusConnected) {
        view_port_set_width(bt->statusbar_view_port, icon_get_width(&I_Bluetooth_Connected_16x8));
        view_port_enabled_set(bt->statusbar_view_port, true);
    } else {
        view_port_enabled_set(bt->statusbar_view_port, false);
    }
}

static void bt_show_warning(Bt* bt, const char* text) {
    dialog_message_set_text(bt->dialog_message, text, 64, 28, AlignCenter, AlignCenter);
    dialog_message_set_buttons(bt->dialog_message, "Quit", NULL, NULL);
    dialog_message_show(bt->dialogs, bt->dialog_message);
}

static void bt_close_rpc_connection(Bt* bt) {
    if(bt->profile == BtProfileSerial && bt->rpc_session) {
        FURRY_LOG_I(TAG, "Close RPC connection");
        furry_event_flag_set(bt->rpc_event, BT_RPC_EVENT_DISCONNECTED);
        rpc_session_close(bt->rpc_session);
        furry_hal_bt_serial_set_event_callback(0, NULL, NULL);
        bt->rpc_session = NULL;
    }
}

static void bt_change_profile(Bt* bt, BtMessage* message) {
    if(furry_hal_bt_is_ble_gatt_gap_supported()) {
        bt_settings_load(&bt->bt_settings);
        bt_close_rpc_connection(bt);

        FurryHalBtProfile furry_profile;
        if(message->data.profile == BtProfileHidKeyboard) {
            furry_profile = FurryHalBtProfileHidKeyboard;
        } else {
            furry_profile = FurryHalBtProfileSerial;
        }

        bt_keys_storage_load(bt->keys_storage);

        if(furry_hal_bt_change_app(furry_profile, bt_on_gap_event_callback, bt)) {
            FURRY_LOG_I(TAG, "Bt App started");
            if(bt->bt_settings.enabled) {
                furry_hal_bt_start_advertising();
            }
            furry_hal_bt_set_key_storage_change_callback(bt_on_key_storage_change_callback, bt);
            bt->profile = message->data.profile;
            if(message->result) {
                *message->result = true;
            }
        } else {
            FURRY_LOG_E(TAG, "Failed to start Bt App");
            if(message->result) {
                *message->result = false;
            }
        }
    } else {
        bt_show_warning(bt, "Radio stack doesn't support this app");
        if(message->result) {
            *message->result = false;
        }
    }
    furry_event_flag_set(bt->api_event, BT_API_UNLOCK_EVENT);
}

static void bt_close_connection(Bt* bt) {
    bt_close_rpc_connection(bt);
    furry_hal_bt_stop_advertising();
    furry_event_flag_set(bt->api_event, BT_API_UNLOCK_EVENT);
}

static inline FurryHalBtProfile get_hal_bt_profile(BtProfile profile) {
    if(profile == BtProfileHidKeyboard) {
        return FurryHalBtProfileHidKeyboard;
    } else {
        return FurryHalBtProfileSerial;
    }
}

static void bt_restart(Bt* bt) {
    furry_hal_bt_change_app(get_hal_bt_profile(bt->profile), bt_on_gap_event_callback, bt);
    furry_hal_bt_start_advertising();
}

void bt_set_profile_adv_name(Bt* bt, const char* fmt, ...) {
    furry_assert(bt);
    furry_assert(fmt);

    char name[FURRY_HAL_BT_ADV_NAME_LENGTH];
    va_list args;
    va_start(args, fmt);
    vsnprintf(name, sizeof(name), fmt, args);
    va_end(args);
    furry_hal_bt_set_profile_adv_name(get_hal_bt_profile(bt->profile), name);

    bt_restart(bt);
}

const char* bt_get_profile_adv_name(Bt* bt) {
    furry_assert(bt);
    return furry_hal_bt_get_profile_adv_name(get_hal_bt_profile(bt->profile));
}

void bt_set_profile_mac_address(Bt* bt, const uint8_t mac[6]) {
    furry_assert(bt);
    furry_assert(mac);

    furry_hal_bt_set_profile_mac_addr(get_hal_bt_profile(bt->profile), mac);

    bt_restart(bt);
}

const uint8_t* bt_get_profile_mac_address(Bt* bt) {
    furry_assert(bt);
    return furry_hal_bt_get_profile_mac_addr(get_hal_bt_profile(bt->profile));
}

bool bt_remote_rssi(Bt* bt, uint8_t* rssi) {
    furry_assert(bt);

    uint8_t rssi_val;
    uint32_t since = furry_hal_bt_get_conn_rssi(&rssi_val);

    if(since == 0) return false;

    *rssi = rssi_val;

    return true;
}

void bt_set_profile_pairing_method(Bt* bt, GapPairing pairing_method) {
    furry_assert(bt);
    furry_hal_bt_set_profile_pairing_method(get_hal_bt_profile(bt->profile), pairing_method);
    bt_restart(bt);
}

GapPairing bt_get_profile_pairing_method(Bt* bt) {
    furry_assert(bt);
    return furry_hal_bt_get_profile_pairing_method(get_hal_bt_profile(bt->profile));
}

void bt_disable_peer_key_update(Bt* bt) {
    UNUSED(bt);
    furry_hal_bt_set_key_storage_change_callback(NULL, NULL);
}

void bt_enable_peer_key_update(Bt* bt) {
    furry_assert(bt);
    furry_hal_bt_set_key_storage_change_callback(bt_on_key_storage_change_callback, bt);
}

int32_t bt_srv(void* p) {
    UNUSED(p);
    Bt* bt = bt_alloc();

    if(!furry_hal_is_normal_boot()) {
        FURRY_LOG_W(TAG, "Skipping start in special boot mode");
        bl_igloo_wait_for_c2_start(FURRY_HAL_BT_C2_START_TIMEOUT);
        furry_record_create(RECORD_BT, bt);
        return 0;
    }

    // Load keys
    if(!bt_keys_storage_load(bt->keys_storage)) {
        FURRY_LOG_W(TAG, "Failed to load bonding keys");
    }

    // Start radio stack
    if(!furry_hal_bt_start_radio_stack()) {
        FURRY_LOG_E(TAG, "Radio stack start failed");
    }

    if(furry_hal_bt_is_ble_gatt_gap_supported()) {
        if(!furry_hal_bt_start_app(FurryHalBtProfileSerial, bt_on_gap_event_callback, bt)) {
            FURRY_LOG_E(TAG, "BLE App start failed");
        } else {
            if(bt->bt_settings.enabled) {
                furry_hal_bt_start_advertising();
            }
            furry_hal_bt_set_key_storage_change_callback(bt_on_key_storage_change_callback, bt);
        }
    } else {
        bt_show_warning(bt, "Unsupported radio stack");
        bt->status = BtStatusUnavailable;
    }

    furry_record_create(RECORD_BT, bt);

    BtMessage message;
    while(1) {
        furry_check(
            furry_message_queue_get(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
        if(message.type == BtMessageTypeUpdateStatus) {
            // Update view ports
            bt_statusbar_update(bt);
            bt_pin_code_hide(bt);
            if(bt->status_changed_cb) {
                bt->status_changed_cb(bt->status, bt->status_changed_ctx);
            }
        } else if(message.type == BtMessageTypeUpdateBatteryLevel) {
            // Update battery level
            furry_hal_bt_update_battery_level(message.data.battery_level);
        } else if(message.type == BtMessageTypeUpdatePowerState) {
            furry_hal_bt_update_power_state();
        } else if(message.type == BtMessageTypePinCodeShow) {
            // Display PIN code
            bt_pin_code_show(bt, message.data.pin_code);
        } else if(message.type == BtMessageTypeKeysStorageUpdated) {
            bt_keys_storage_update(
                bt->keys_storage,
                message.data.key_storage_data.start_address,
                message.data.key_storage_data.size);
        } else if(message.type == BtMessageTypeSetProfile) {
            bt_change_profile(bt, &message);
        } else if(message.type == BtMessageTypeDisconnect) {
            bt_close_connection(bt);
        } else if(message.type == BtMessageTypeForgetBondedDevices) {
            bt_keys_storage_delete(bt->keys_storage);
        }
    }
    return 0;
}
