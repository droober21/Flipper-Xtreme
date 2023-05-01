#include <furry.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

// Define log tag
#define TAG "example_apps_assets"

static void example_apps_data_print_file_content(Storage* storage, const char* path) {
    Stream* stream = file_stream_alloc(storage);
    FurryString* line = furry_string_alloc();

    FURRY_LOG_I(TAG, "----------------------------------------");
    FURRY_LOG_I(TAG, "File \"%s\" content:", path);
    if(file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line)) {
            furry_string_replace_all(line, "\r", "");
            furry_string_replace_all(line, "\n", "");
            FURRY_LOG_I(TAG, "%s", furry_string_get_cstr(line));
        }
    } else {
        FURRY_LOG_E(TAG, "Failed to open file");
    }
    FURRY_LOG_I(TAG, "----------------------------------------");

    furry_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
}

// Application entry point
int32_t example_apps_assets_main(void* p) {
    // Mark argument as unused
    UNUSED(p);

    // Open storage
    Storage* storage = furry_record_open(RECORD_STORAGE);

    example_apps_data_print_file_content(storage, APP_ASSETS_PATH("test_asset.txt"));
    example_apps_data_print_file_content(storage, APP_ASSETS_PATH("poems/a jelly-fish.txt"));
    example_apps_data_print_file_content(storage, APP_ASSETS_PATH("poems/theme in yellow.txt"));
    example_apps_data_print_file_content(storage, APP_ASSETS_PATH("poems/my shadow.txt"));

    // Close storage
    furry_record_close(RECORD_STORAGE);

    return 0;
}
