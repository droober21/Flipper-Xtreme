#include "nfc_debug_log.h"

#include <storage/storage.h>
#include <stream/buffered_file_stream.h>

#define TAG "NfcDebugLog"

#define NFC_DEBUG_PCAP_FILENAME EXT_PATH("nfc/debug.txt")

struct NfcDebugLog {
    Stream* file_stream;
    FurryString* data_str;
};

NfcDebugLog* nfc_debug_log_alloc() {
    NfcDebugLog* instance = malloc(sizeof(NfcDebugLog));

    Storage* storage = furry_record_open(RECORD_STORAGE);
    instance->file_stream = buffered_file_stream_alloc(storage);

    if(!buffered_file_stream_open(
           instance->file_stream, NFC_DEBUG_PCAP_FILENAME, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        buffered_file_stream_close(instance->file_stream);
        stream_free(instance->file_stream);
        instance->file_stream = NULL;
    }

    if(!instance->file_stream) {
        free(instance);
        instance = NULL;
    } else {
        instance->data_str = furry_string_alloc();
    }
    furry_record_close(RECORD_STORAGE);

    return instance;
}

void nfc_debug_log_free(NfcDebugLog* instance) {
    furry_assert(instance);
    furry_assert(instance->file_stream);
    furry_assert(instance->data_str);

    buffered_file_stream_close(instance->file_stream);
    stream_free(instance->file_stream);
    furry_string_free(instance->data_str);

    free(instance);
}

void nfc_debug_log_process_data(
    NfcDebugLog* instance,
    uint8_t* data,
    uint16_t len,
    bool reader_to_tag,
    bool crc_dropped) {
    furry_assert(instance);
    furry_assert(instance->file_stream);
    furry_assert(instance->data_str);
    furry_assert(data);
    UNUSED(crc_dropped);

    furry_string_printf(instance->data_str, "%lu %c:", furry_get_tick(), reader_to_tag ? 'R' : 'T');
    uint16_t data_len = len;
    for(size_t i = 0; i < data_len; i++) {
        furry_string_cat_printf(instance->data_str, " %02x", data[i]);
    }
    furry_string_push_back(instance->data_str, '\n');

    stream_write_string(instance->file_stream, instance->data_str);
}
