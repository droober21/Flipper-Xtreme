#include <storage/storage.h>
#include "util.h"

const char* CONFIG_FILE_PATH = EXT_PATH(".blackjack.settings");

void save_settings(Settings settings) {
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FURRY_LOG_D(APP_NAME, "Saving config");
    if(flipper_format_file_open_existing(file, CONFIG_FILE_PATH)) {
        FURRY_LOG_D(
            APP_NAME, "Saving %s: %ld", CONF_ANIMATION_DURATION, settings.animation_duration);
        flipper_format_update_uint32(
            file, CONF_ANIMATION_DURATION, &(settings.animation_duration), 1);

        FURRY_LOG_D(APP_NAME, "Saving %s: %ld", CONF_MESSAGE_DURATION, settings.message_duration);
        flipper_format_update_uint32(file, CONF_MESSAGE_DURATION, &(settings.message_duration), 1);

        FURRY_LOG_D(APP_NAME, "Saving %s: %ld", CONF_STARTING_MONEY, settings.starting_money);
        flipper_format_update_uint32(file, CONF_STARTING_MONEY, &(settings.starting_money), 1);

        FURRY_LOG_D(APP_NAME, "Saving %s: %ld", CONF_ROUND_PRICE, settings.round_price);
        flipper_format_update_uint32(file, CONF_ROUND_PRICE, &(settings.round_price), 1);

        FURRY_LOG_D(APP_NAME, "Saving %s: %i", CONF_SOUND_EFFECTS, settings.sound_effects ? 1 : 0);
        flipper_format_update_bool(file, CONF_SOUND_EFFECTS, &(settings.sound_effects), 1);
        FURRY_LOG_D(APP_NAME, "Config saved");
    } else {
        FURRY_LOG_E(APP_NAME, "Save error");
    }
    flipper_format_file_close(file);
    flipper_format_free(file);
    furry_record_close(RECORD_STORAGE);
}

void save_settings_file(FlipperFormat* file, Settings* settings) {
    flipper_format_write_header_cstr(file, CONFIG_FILE_HEADER, CONFIG_FILE_VERSION);
    flipper_format_write_comment_cstr(file, "Card animation duration in ms");
    flipper_format_write_uint32(file, CONF_ANIMATION_DURATION, &(settings->animation_duration), 1);
    flipper_format_write_comment_cstr(file, "Popup message duration in ms");
    flipper_format_write_uint32(file, CONF_MESSAGE_DURATION, &(settings->message_duration), 1);
    flipper_format_write_comment_cstr(file, "Player's starting money");
    flipper_format_write_uint32(file, CONF_STARTING_MONEY, &(settings->starting_money), 1);
    flipper_format_write_comment_cstr(file, "Round price");
    flipper_format_write_uint32(file, CONF_ROUND_PRICE, &(settings->round_price), 1);
    flipper_format_write_comment_cstr(file, "Enable sound effects");
    flipper_format_write_bool(file, CONF_SOUND_EFFECTS, &(settings->sound_effects), 1);
}

Settings load_settings() {
    Settings settings;

    FURRY_LOG_D(APP_NAME, "Loading default settings");
    settings.animation_duration = 800;
    settings.message_duration = 1500;
    settings.starting_money = 200;
    settings.round_price = 10;
    settings.sound_effects = true;

    FURRY_LOG_D(APP_NAME, "Opening storage");
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FURRY_LOG_D(APP_NAME, "Allocating file");
    FlipperFormat* file = flipper_format_file_alloc(storage);

    FURRY_LOG_D(APP_NAME, "Allocating string");
    FurryString* string_value;
    string_value = furry_string_alloc();

    if(storage_common_stat(storage, CONFIG_FILE_PATH, NULL) != FSE_OK) {
        FURRY_LOG_D(APP_NAME, "Config file %s not found, creating new one...", CONFIG_FILE_PATH);
        if(!flipper_format_file_open_new(file, CONFIG_FILE_PATH)) {
            FURRY_LOG_E(APP_NAME, "Error creating new file %s", CONFIG_FILE_PATH);
            flipper_format_file_close(file);
        } else {
            save_settings_file(file, &settings);
        }
    } else {
        if(!flipper_format_file_open_existing(file, CONFIG_FILE_PATH)) {
            FURRY_LOG_E(APP_NAME, "Error opening existing file %s", CONFIG_FILE_PATH);
            flipper_format_file_close(file);
        } else {
            uint32_t value;
            bool valueBool;
            FURRY_LOG_D(APP_NAME, "Checking version");
            if(!flipper_format_read_header(file, string_value, &value)) {
                FURRY_LOG_E(APP_NAME, "Config file mismatch");
            } else {
                FURRY_LOG_D(APP_NAME, "Loading %s", CONF_ANIMATION_DURATION);
                if(flipper_format_read_uint32(file, CONF_ANIMATION_DURATION, &value, 1)) {
                    settings.animation_duration = value;
                    FURRY_LOG_D(APP_NAME, "Loaded %s: %ld", CONF_ANIMATION_DURATION, value);
                }
                FURRY_LOG_D(APP_NAME, "Loading %s", CONF_MESSAGE_DURATION);
                if(flipper_format_read_uint32(file, CONF_MESSAGE_DURATION, &value, 1)) {
                    settings.message_duration = value;
                    FURRY_LOG_D(APP_NAME, "Loaded %s: %ld", CONF_MESSAGE_DURATION, value);
                }
                FURRY_LOG_D(APP_NAME, "Loading %s", CONF_STARTING_MONEY);
                if(flipper_format_read_uint32(file, CONF_STARTING_MONEY, &value, 1)) {
                    settings.starting_money = value;
                    FURRY_LOG_D(APP_NAME, "Loaded %s: %ld", CONF_STARTING_MONEY, value);
                }
                FURRY_LOG_D(APP_NAME, "Loading %s", CONF_ROUND_PRICE);
                if(flipper_format_read_uint32(file, CONF_ROUND_PRICE, &value, 1)) {
                    settings.round_price = value;
                    FURRY_LOG_D(APP_NAME, "Loaded %s: %ld", CONF_ROUND_PRICE, value);
                }
                FURRY_LOG_D(APP_NAME, "Loading %s", CONF_SOUND_EFFECTS);
                if(flipper_format_read_bool(file, CONF_SOUND_EFFECTS, &valueBool, 1)) {
                    settings.sound_effects = valueBool;
                    FURRY_LOG_D(APP_NAME, "Loaded %s: %i", CONF_ROUND_PRICE, valueBool ? 1 : 0);
                }
            }
            flipper_format_file_close(file);
        }
    }

    furry_string_free(string_value);
    //        flipper_format_file_close(file);
    flipper_format_free(file);
    furry_record_close(RECORD_STORAGE);
    return settings;
}