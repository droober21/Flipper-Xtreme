#include "bt_i.h"

bool bt_set_profile(Bt* bt, BtProfile profile) {
    furry_assert(bt);

    // Send message
    bool result = false;
    BtMessage message = {
        .type = BtMessageTypeSetProfile, .data.profile = profile, .result = &result};
    furry_check(
        furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
    // Wait for unlock
    furry_event_flag_wait(bt->api_event, BT_API_UNLOCK_EVENT, FurryFlagWaitAny, FurryWaitForever);

    return result;
}

void bt_disconnect(Bt* bt) {
    furry_assert(bt);

    // Send message
    BtMessage message = {.type = BtMessageTypeDisconnect};
    furry_check(
        furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
    // Wait for unlock
    furry_event_flag_wait(bt->api_event, BT_API_UNLOCK_EVENT, FurryFlagWaitAny, FurryWaitForever);
}

void bt_set_status_changed_callback(Bt* bt, BtStatusChangedCallback callback, void* context) {
    furry_assert(bt);

    bt->status_changed_cb = callback;
    bt->status_changed_ctx = context;
}

void bt_forget_bonded_devices(Bt* bt) {
    furry_assert(bt);
    BtMessage message = {.type = BtMessageTypeForgetBondedDevices};
    furry_check(
        furry_message_queue_put(bt->message_queue, &message, FurryWaitForever) == FurryStatusOk);
}

void bt_keys_storage_set_storage_path(Bt* bt, const char* keys_storage_path) {
    furry_assert(bt);
    furry_assert(bt->keys_storage);
    furry_assert(keys_storage_path);

    Storage* storage = furry_record_open(RECORD_STORAGE);
    FurryString* path = furry_string_alloc_set(keys_storage_path);
    storage_common_resolve_path_and_ensure_app_directory(storage, path);

    bt_keys_storage_set_file_path(bt->keys_storage, furry_string_get_cstr(path));

    furry_string_free(path);
    furry_record_close(RECORD_STORAGE);
}

void bt_keys_storage_set_default_path(Bt* bt) {
    furry_assert(bt);
    furry_assert(bt->keys_storage);

    bt_keys_storage_set_file_path(bt->keys_storage, BT_KEYS_STORAGE_PATH);
}
