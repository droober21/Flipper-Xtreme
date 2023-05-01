#include <furry.h>
#include "wifi_marauder_validators.h"
#include <storage/storage.h>

struct ValidatorIsFile {
    char* app_path_folder;
    const char* app_extension;
    char* current_name;
};

bool validator_is_file_callback(const char* text, FurryString* error, void* context) {
    furry_assert(context);
    ValidatorIsFile* instance = context;

    if(instance->current_name != NULL) {
        if(strcmp(instance->current_name, text) == 0) {
            return true;
        }
    }

    bool ret = true;
    FurryString* path = furry_string_alloc_printf(
        "%s/%s%s", instance->app_path_folder, text, instance->app_extension);
    Storage* storage = furry_record_open(RECORD_STORAGE);
    if(storage_common_stat(storage, furry_string_get_cstr(path), NULL) == FSE_OK) {
        ret = false;
        furry_string_printf(error, "This name\nexists!\nChoose\nanother one.");
    } else {
        ret = true;
    }
    furry_string_free(path);
    furry_record_close(RECORD_STORAGE);

    return ret;
}

ValidatorIsFile* validator_is_file_alloc_init(
    const char* app_path_folder,
    const char* app_extension,
    const char* current_name) {
    ValidatorIsFile* instance = malloc(sizeof(ValidatorIsFile));

    instance->app_path_folder = strdup(app_path_folder);
    instance->app_extension = app_extension;
    if(current_name != NULL) {
        instance->current_name = strdup(current_name);
    }

    return instance;
}

void validator_is_file_free(ValidatorIsFile* instance) {
    furry_assert(instance);
    free(instance->app_path_folder);
    free(instance->current_name);
    free(instance);
}
