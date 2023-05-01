#include "saved_struct.h"
#include <furry.h>
#include <stdint.h>
#include <storage/storage.h>

#define TAG "SavedStruct"

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t checksum;
    uint8_t flags;
    uint32_t timestamp;
} SavedStructHeader;

bool saved_struct_save(const char* path, void* data, size_t size, uint8_t magic, uint8_t version) {
    furry_assert(path);
    furry_assert(data);
    furry_assert(size);
    SavedStructHeader header;

    FURRY_LOG_I(TAG, "Saving \"%s\"", path);

    // Store
    Storage* storage = furry_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool result = true;
    bool saved = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!saved) {
        FURRY_LOG_E(
            TAG, "Open failed \"%s\". Error: \'%s\'", path, storage_file_get_error_desc(file));
        result = false;
    }

    if(result) {
        // Calculate checksum
        uint8_t checksum = 0;
        uint8_t* source = data;
        for(size_t i = 0; i < size; i++) {
            checksum += source[i];
        }
        // Set header
        header.magic = magic;
        header.version = version;
        header.checksum = checksum;
        header.flags = 0;
        header.timestamp = 0;

        uint16_t bytes_count = storage_file_write(file, &header, sizeof(header));
        bytes_count += storage_file_write(file, data, size);

        if(bytes_count != (size + sizeof(header))) {
            FURRY_LOG_E(
                TAG, "Write failed \"%s\". Error: \'%s\'", path, storage_file_get_error_desc(file));
            result = false;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furry_record_close(RECORD_STORAGE);
    return result;
}

bool saved_struct_load(const char* path, void* data, size_t size, uint8_t magic, uint8_t version) {
    FURRY_LOG_I(TAG, "Loading \"%s\"", path);

    SavedStructHeader header;

    uint8_t* data_read = malloc(size);
    Storage* storage = furry_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool result = true;
    bool loaded = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!loaded) {
        FURRY_LOG_E(
            TAG, "Failed to read \"%s\". Error: %s", path, storage_file_get_error_desc(file));
        result = false;
    }

    if(result) {
        uint16_t bytes_count = storage_file_read(file, &header, sizeof(SavedStructHeader));
        bytes_count += storage_file_read(file, data_read, size);

        if(bytes_count != (sizeof(SavedStructHeader) + size)) {
            FURRY_LOG_E(TAG, "Size mismatch of file \"%s\"", path);
            result = false;
        }
    }

    if(result && (header.magic != magic || header.version != version)) {
        FURRY_LOG_E(
            TAG,
            "Magic(%d != %d) or Version(%d != %d) mismatch of file \"%s\"",
            header.magic,
            magic,
            header.version,
            version,
            path);
        result = false;
    }

    if(result) {
        uint8_t checksum = 0;
        const uint8_t* source = (const uint8_t*)data_read;
        for(size_t i = 0; i < size; i++) {
            checksum += source[i];
        }

        if(header.checksum != checksum) {
            FURRY_LOG_E(
                TAG, "Checksum(%d != %d) mismatch of file \"%s\"", header.checksum, checksum, path);
            result = false;
        }
    }

    if(result) {
        memcpy(data, data_read, size);
    }

    storage_file_close(file);
    storage_file_free(file);
    furry_record_close(RECORD_STORAGE);
    free(data_read);

    return result;
}

bool saved_struct_get_payload_size(
    const char* path,
    uint8_t magic,
    uint8_t version,
    size_t* payload_size) {
    furry_assert(path);
    furry_assert(payload_size);

    SavedStructHeader header;
    Storage* storage = furry_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool result = false;
    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURRY_LOG_E(
                TAG, "Failed to read \"%s\". Error: %s", path, storage_file_get_error_desc(file));
            break;
        }

        uint16_t bytes_count = storage_file_read(file, &header, sizeof(SavedStructHeader));
        if(bytes_count != sizeof(SavedStructHeader)) {
            FURRY_LOG_E(TAG, "Failed to read header");
            break;
        }

        if((header.magic != magic) || (header.version != version)) {
            FURRY_LOG_E(
                TAG,
                "Magic(%d != %d) or Version(%d != %d) mismatch of file \"%s\"",
                header.magic,
                magic,
                header.version,
                version,
                path);
            break;
        }

        uint64_t file_size = storage_file_size(file);
        *payload_size = file_size - sizeof(SavedStructHeader);

        result = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furry_record_close(RECORD_STORAGE);

    return result;
}
