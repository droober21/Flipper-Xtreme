#include "storage_glue.h"
#include <furry_hal.h>

/****************** storage file ******************/

void storage_file_init(StorageFile* obj) {
    obj->file = NULL;
    obj->file_data = NULL;
    obj->path = furry_string_alloc();
}

void storage_file_init_set(StorageFile* obj, const StorageFile* src) {
    obj->file = src->file;
    obj->file_data = src->file_data;
    obj->path = furry_string_alloc_set(src->path);
}

void storage_file_set(StorageFile* obj, const StorageFile* src) { //-V524
    obj->file = src->file;
    obj->file_data = src->file_data;
    furry_string_set(obj->path, src->path);
}

void storage_file_clear(StorageFile* obj) {
    furry_string_free(obj->path);
}

/****************** storage data ******************/

void storage_data_init(StorageData* storage) {
    storage->data = NULL;
    storage->status = StorageStatusNotReady;
    StorageFileList_init(storage->files);
}

StorageStatus storage_data_status(StorageData* storage) {
    return storage->status;
}

const char* storage_data_status_text(StorageData* storage) {
    const char* result = "unknown";
    switch(storage->status) {
    case StorageStatusOK:
        result = "ok";
        break;
    case StorageStatusNotReady:
        result = "not ready";
        break;
    case StorageStatusNotMounted:
        result = "not mounted";
        break;
    case StorageStatusNoFS:
        result = "no filesystem";
        break;
    case StorageStatusNotAccessible:
        result = "not accessible";
        break;
    case StorageStatusErrorInternal:
        result = "internal";
        break;
    }

    return result;
}

void storage_data_timestamp(StorageData* storage) {
    storage->timestamp = furry_hal_rtc_get_timestamp();
}

uint32_t storage_data_get_timestamp(StorageData* storage) {
    return storage->timestamp;
}

/****************** storage glue ******************/

static StorageFile* storage_get_file(const File* file, StorageData* storage) {
    StorageFile* storage_file_ref = NULL;

    StorageFileList_it_t it;
    for(StorageFileList_it(it, storage->files); !StorageFileList_end_p(it);
        StorageFileList_next(it)) {
        StorageFile* storage_file = StorageFileList_ref(it);

        if(storage_file->file->file_id == file->file_id) {
            storage_file_ref = storage_file;
            break;
        }
    }

    return storage_file_ref;
}

bool storage_has_file(const File* file, StorageData* storage) {
    return storage_get_file(file, storage) != NULL;
}

bool storage_path_already_open(FurryString* path, StorageData* storage) {
    bool open = false;

    StorageFileList_it_t it;

    for(StorageFileList_it(it, storage->files); !StorageFileList_end_p(it);
        StorageFileList_next(it)) {
        const StorageFile* storage_file = StorageFileList_cref(it);

        if(furry_string_cmp(storage_file->path, path) == 0) {
            open = true;
            break;
        }
    }

    return open;
}

void storage_set_storage_file_data(const File* file, void* file_data, StorageData* storage) {
    StorageFile* storage_file_ref = storage_get_file(file, storage);
    furry_check(storage_file_ref != NULL);
    storage_file_ref->file_data = file_data;
}

void* storage_get_storage_file_data(const File* file, StorageData* storage) {
    StorageFile* storage_file_ref = storage_get_file(file, storage);
    furry_check(storage_file_ref != NULL);
    return storage_file_ref->file_data;
}

void storage_push_storage_file(File* file, FurryString* path, StorageData* storage) {
    StorageFile* storage_file = StorageFileList_push_new(storage->files);
    file->file_id = (uint32_t)storage_file;
    storage_file->file = file;
    furry_string_set(storage_file->path, path);
}

bool storage_pop_storage_file(File* file, StorageData* storage) {
    StorageFileList_it_t it;
    bool result = false;

    for(StorageFileList_it(it, storage->files); !StorageFileList_end_p(it);
        StorageFileList_next(it)) {
        if(StorageFileList_cref(it)->file->file_id == file->file_id) {
            result = true;
            break;
        }
    }

    if(result) {
        StorageFileList_remove(storage->files, it);
    }

    return result;
}
