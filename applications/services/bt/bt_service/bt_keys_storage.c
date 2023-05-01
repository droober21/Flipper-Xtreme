#include "bt_keys_storage.h"

#include <furry.h>
#include <furry_hal_bt.h>
#include <lib/toolbox/saved_struct.h>
#include <storage/storage.h>

#define BT_KEYS_STORAGE_VERSION (0)
#define BT_KEYS_STORAGE_MAGIC (0x18)

#define TAG "BtKeyStorage"

struct BtKeysStorage {
    uint8_t* nvm_sram_buff;
    uint16_t nvm_sram_buff_size;
    FurryString* file_path;
};

bool bt_keys_storage_delete(BtKeysStorage* instance) {
    furry_assert(instance);

    bool delete_succeed = false;
    bool bt_is_active = furry_hal_bt_is_active();

    furry_hal_bt_stop_advertising();
    delete_succeed = furry_hal_bt_clear_white_list();
    if(bt_is_active) {
        furry_hal_bt_start_advertising();
    }

    return delete_succeed;
}

BtKeysStorage* bt_keys_storage_alloc(const char* keys_storage_path) {
    furry_assert(keys_storage_path);

    BtKeysStorage* instance = malloc(sizeof(BtKeysStorage));
    // Set default nvm ram parameters
    furry_hal_bt_get_key_storage_buff(&instance->nvm_sram_buff, &instance->nvm_sram_buff_size);
    // Set key storage file
    instance->file_path = furry_string_alloc();
    furry_string_set_str(instance->file_path, keys_storage_path);

    return instance;
}

void bt_keys_storage_free(BtKeysStorage* instance) {
    furry_assert(instance);

    furry_string_free(instance->file_path);
    free(instance);
}

void bt_keys_storage_set_file_path(BtKeysStorage* instance, const char* path) {
    furry_assert(instance);
    furry_assert(path);

    furry_string_set_str(instance->file_path, path);
}

void bt_keys_storage_set_ram_params(BtKeysStorage* instance, uint8_t* buff, uint16_t size) {
    furry_assert(instance);
    furry_assert(buff);

    instance->nvm_sram_buff = buff;
    instance->nvm_sram_buff_size = size;
}

bool bt_keys_storage_load(BtKeysStorage* instance) {
    furry_assert(instance);

    bool loaded = false;
    do {
        // Get payload size
        size_t payload_size = 0;
        if(!saved_struct_get_payload_size(
               furry_string_get_cstr(instance->file_path),
               BT_KEYS_STORAGE_MAGIC,
               BT_KEYS_STORAGE_VERSION,
               &payload_size)) {
            FURRY_LOG_E(TAG, "Failed to read payload size");
            break;
        }

        if(payload_size > instance->nvm_sram_buff_size) {
            FURRY_LOG_E(TAG, "Saved data doesn't fit ram buffer");
            break;
        }

        // Load saved data to ram
        furry_hal_bt_nvm_sram_sem_acquire();
        bool data_loaded = saved_struct_load(
            furry_string_get_cstr(instance->file_path),
            instance->nvm_sram_buff,
            payload_size,
            BT_KEYS_STORAGE_MAGIC,
            BT_KEYS_STORAGE_VERSION);
        furry_hal_bt_nvm_sram_sem_release();
        if(!data_loaded) {
            FURRY_LOG_E(TAG, "Failed to load struct");
            break;
        }

        loaded = true;
    } while(false);

    return loaded;
}

bool bt_keys_storage_update(BtKeysStorage* instance, uint8_t* start_addr, uint32_t size) {
    furry_assert(instance);
    furry_assert(start_addr);

    bool updated = false;

    FURRY_LOG_I(
        TAG,
        "Base address: %p. Start update address: %p. Size changed: %lu",
        (void*)instance->nvm_sram_buff,
        start_addr,
        size);

    do {
        size_t new_size = start_addr - instance->nvm_sram_buff + size;
        if(new_size > instance->nvm_sram_buff_size) {
            FURRY_LOG_E(TAG, "NVM RAM buffer overflow");
            break;
        }

        furry_hal_bt_nvm_sram_sem_acquire();
        bool data_updated = saved_struct_save(
            furry_string_get_cstr(instance->file_path),
            instance->nvm_sram_buff,
            new_size,
            BT_KEYS_STORAGE_MAGIC,
            BT_KEYS_STORAGE_VERSION);
        furry_hal_bt_nvm_sram_sem_release();
        if(!data_updated) {
            FURRY_LOG_E(TAG, "Failed to update key storage");
            break;
        }
        updated = true;
    } while(false);

    return updated;
}
