#include "xtreme.h"
#include "private.h"
#include <assets_icons.h>
#include <storage/storage.h>
#include <core/dangerous_defines.h>

#define TAG "XtremeAssets"

#define ICONS_FMT XTREME_ASSETS_PATH "/%s/Icons/%s"

XtremeAssets xtreme_assets = {
    .A_Levelup_128x64 = &A_Levelup_128x64,
    .I_BLE_Pairing_128x64 = &I_BLE_Pairing_128x64,
    .I_DolphinCommon_56x48 = &I_DolphinCommon_56x48,
    .I_DolphinMafia_115x62 = &I_DolphinMafia_115x62,
    .I_DolphinNice_96x59 = &I_DolphinNice_96x59,
    .I_DolphinWait_61x59 = &I_DolphinWait_61x59,
    .I_iButtonDolphinVerySuccess_108x52 = &I_iButtonDolphinVerySuccess_108x52,
    .I_DolphinReadingSuccess_59x63 = &I_DolphinReadingSuccess_59x63,
    .I_Lockscreen = &I_Lockscreen,
    .I_WarningDolphin_45x42 = &I_WarningDolphin_45x42,
    .I_NFC_dolphin_emulation_47x61 = &I_NFC_dolphin_emulation_47x61,
    .I_passport_bad_46x49 = &I_passport_bad_46x49,
    .I_passport_DB = &I_passport_DB,
    .I_passport_happy_46x49 = &I_passport_happy_46x49,
    .I_passport_okay_46x49 = &I_passport_okay_46x49,
    .I_RFIDDolphinReceive_97x61 = &I_RFIDDolphinReceive_97x61,
    .I_RFIDDolphinSend_97x61 = &I_RFIDDolphinSend_97x61,
    .I_RFIDDolphinSuccess_108x57 = &I_RFIDDolphinSuccess_108x57,
    .I_Cry_dolph_55x52 = &I_Cry_dolph_55x52,
    .I_Fishing_123x52 = &I_Fishing_123x52,
    .I_Scanning_123x52 = &I_Scanning_123x52,
    .I_Auth_62x31 = &I_Auth_62x31,
    .I_Connect_me_62x31 = &I_Connect_me_62x31,
    .I_Connected_62x31 = &I_Connected_62x31,
    .I_Error_62x31 = &I_Error_62x31,
};

void anim(const Icon** replace, const char* name, FurryString* path, File* file) {
    do {
        furry_string_printf(path, ICONS_FMT "/meta", XTREME_SETTINGS()->asset_pack, name);
        if(!storage_file_open(file, furry_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING))
            break;
        int32_t width, height, frame_rate, frame_count;
        storage_file_read(file, &width, 4);
        storage_file_read(file, &height, 4);
        storage_file_read(file, &frame_rate, 4);
        storage_file_read(file, &frame_count, 4);
        storage_file_close(file);

        Icon* icon = malloc(sizeof(Icon));
        FURRY_CONST_ASSIGN(icon->width, width);
        FURRY_CONST_ASSIGN(icon->height, height);
        FURRY_CONST_ASSIGN(icon->frame_rate, frame_rate);
        FURRY_CONST_ASSIGN(icon->frame_count, frame_count);
        icon->frames = malloc(sizeof(const uint8_t*) * icon->frame_count);
        const char* pack = XTREME_SETTINGS()->asset_pack;

        bool ok = true;
        for(int i = 0; i < icon->frame_count; ++i) {
            FURRY_CONST_ASSIGN_PTR(icon->frames[i], 0);
            if(ok) {
                ok = false;
                furry_string_printf(path, ICONS_FMT "/frame_%02d.bm", pack, name, i);
                do {
                    if(!storage_file_open(
                           file, furry_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING))
                        break;

                    uint64_t size = storage_file_size(file);
                    FURRY_CONST_ASSIGN_PTR(icon->frames[i], malloc(size));
                    if(storage_file_read(file, (void*)icon->frames[i], size) == size) ok = true;
                    storage_file_close(file);
                } while(0);
            }
        }
        if(!ok) {
            for(int i = 0; i < icon->frame_count; ++i) {
                if(icon->frames[i]) {
                    free((void*)icon->frames[i]);
                }
            }
            free((void*)icon->frames);
            free(icon);

            break;
        }

        *replace = icon;
    } while(false);
}

void icon(const Icon** replace, const char* name, FurryString* path, File* file) {
    furry_string_printf(path, ICONS_FMT ".bmx", XTREME_SETTINGS()->asset_pack, name);
    if(storage_file_open(file, furry_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file) - 8;
        int32_t width, height;
        storage_file_read(file, &width, 4);
        storage_file_read(file, &height, 4);

        Icon* icon = malloc(sizeof(Icon));
        FURRY_CONST_ASSIGN(icon->frame_count, 1);
        FURRY_CONST_ASSIGN(icon->frame_rate, 0);
        FURRY_CONST_ASSIGN(icon->width, width);
        FURRY_CONST_ASSIGN(icon->height, height);
        icon->frames = malloc(sizeof(const uint8_t*));
        FURRY_CONST_ASSIGN_PTR(icon->frames[0], malloc(size));
        storage_file_read(file, (void*)icon->frames[0], size);
        *replace = icon;

        storage_file_close(file);
    }
}

void swap(XtremeAssets* x, FurryString* p, File* f) {
    anim(&x->A_Levelup_128x64, "Animations/Levelup_128x64", p, f);
    icon(&x->I_BLE_Pairing_128x64, "BLE/BLE_Pairing_128x64", p, f);
    icon(&x->I_DolphinCommon_56x48, "Dolphin/DolphinCommon_56x48", p, f);
    icon(&x->I_DolphinMafia_115x62, "iButton/DolphinMafia_115x62", p, f);
    icon(&x->I_DolphinNice_96x59, "iButton/DolphinNice_96x59", p, f);
    icon(&x->I_DolphinWait_61x59, "iButton/DolphinWait_61x59", p, f);
    icon(&x->I_iButtonDolphinVerySuccess_108x52, "iButton/iButtonDolphinVerySuccess_108x52", p, f);
    icon(&x->I_DolphinReadingSuccess_59x63, "Infrared/DolphinReadingSuccess_59x63", p, f);
    icon(&x->I_Lockscreen, "Interface/Lockscreen", p, f);
    icon(&x->I_WarningDolphin_45x42, "Interface/WarningDolphin_45x42", p, f);
    icon(&x->I_NFC_dolphin_emulation_47x61, "NFC/NFC_dolphin_emulation_47x61", p, f);
    icon(&x->I_passport_bad_46x49, "Passport/passport_bad_46x49", p, f);
    icon(&x->I_passport_DB, "Passport/passport_DB", p, f);
    icon(&x->I_passport_happy_46x49, "Passport/passport_happy_46x49", p, f);
    icon(&x->I_passport_okay_46x49, "Passport/passport_okay_46x49", p, f);
    icon(&x->I_RFIDDolphinReceive_97x61, "RFID/RFIDDolphinReceive_97x61", p, f);
    icon(&x->I_RFIDDolphinSend_97x61, "RFID/RFIDDolphinSend_97x61", p, f);
    icon(&x->I_RFIDDolphinSuccess_108x57, "RFID/RFIDDolphinSuccess_108x57", p, f);
    icon(&x->I_Cry_dolph_55x52, "Settings/Cry_dolph_55x52", p, f);
    icon(&x->I_Fishing_123x52, "SubGhz/Fishing_123x52", p, f);
    icon(&x->I_Scanning_123x52, "SubGhz/Scanning_123x52", p, f);
    icon(&x->I_Auth_62x31, "U2F/Auth_62x31", p, f);
    icon(&x->I_Connect_me_62x31, "U2F/Connect_me_62x31", p, f);
    icon(&x->I_Connected_62x31, "U2F/Connected_62x31", p, f);
    icon(&x->I_Error_62x31, "U2F/Error_62x31", p, f);
}

void XTREME_ASSETS_LOAD() {
    XtremeSettings* xtreme_settings = XTREME_SETTINGS();
    if(xtreme_settings->asset_pack[0] == '\0') return;
    xtreme_assets.is_nsfw = strncmp(xtreme_settings->asset_pack, "NSFW", strlen("NSFW")) == 0;

    Storage* storage = furry_record_open(RECORD_STORAGE);
    int32_t timeout = 5000;
    while(timeout > 0) {
        if(storage_sd_status(storage) == FSE_OK) break;
        furry_delay_ms(250);
        timeout -= 250;
    }

    FileInfo info;
    FurryString* path = furry_string_alloc();
    furry_string_printf(path, XTREME_ASSETS_PATH "/%s", xtreme_settings->asset_pack);
    if(storage_common_stat(storage, furry_string_get_cstr(path), &info) == FSE_OK &&
       info.flags & FSF_DIRECTORY) {
        File* file = storage_file_alloc(storage);
        swap(&xtreme_assets, path, file);
        storage_file_free(file);
    }
    furry_string_free(path);
    furry_record_close(RECORD_STORAGE);
}

XtremeAssets* XTREME_ASSETS() {
    return &xtreme_assets;
}
