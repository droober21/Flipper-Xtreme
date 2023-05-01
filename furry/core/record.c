#include "record.h"
#include "check.h"
#include "memmgr.h"
#include "mutex.h"
#include "event_flag.h"

#include <m-dict.h>
#include <toolbox/m_cstr_dup.h>

#define FURRY_RECORD_FLAG_READY (0x1)

typedef struct {
    FurryEventFlag* flags;
    void* data;
    size_t holders_count;
} FurryRecordData;

DICT_DEF2(FurryRecordDataDict, const char*, M_CSTR_DUP_OPLIST, FurryRecordData, M_POD_OPLIST)

typedef struct {
    FurryMutex* mutex;
    FurryRecordDataDict_t records;
} FurryRecord;

static FurryRecord* furry_record = NULL;

static FurryRecordData* furry_record_get(const char* name) {
    return FurryRecordDataDict_get(furry_record->records, name);
}

static void furry_record_put(const char* name, FurryRecordData* record_data) {
    FurryRecordDataDict_set_at(furry_record->records, name, *record_data);
}

static void furry_record_erase(const char* name, FurryRecordData* record_data) {
    furry_event_flag_free(record_data->flags);
    FurryRecordDataDict_erase(furry_record->records, name);
}

void furry_record_init() {
    furry_record = malloc(sizeof(FurryRecord));
    furry_record->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    furry_check(furry_record->mutex);
    FurryRecordDataDict_init(furry_record->records);
}

static FurryRecordData* furry_record_data_get_or_create(const char* name) {
    furry_assert(furry_record);
    FurryRecordData* record_data = furry_record_get(name);
    if(!record_data) {
        FurryRecordData new_record;
        new_record.flags = furry_event_flag_alloc();
        new_record.data = NULL;
        new_record.holders_count = 0;
        furry_record_put(name, &new_record);
        record_data = furry_record_get(name);
    }
    return record_data;
}

static void furry_record_lock() {
    furry_check(furry_mutex_acquire(furry_record->mutex, FurryWaitForever) == FurryStatusOk);
}

static void furry_record_unlock() {
    furry_check(furry_mutex_release(furry_record->mutex) == FurryStatusOk);
}

bool furry_record_exists(const char* name) {
    furry_assert(furry_record);
    furry_assert(name);

    bool ret = false;

    furry_record_lock();
    ret = (furry_record_get(name) != NULL);
    furry_record_unlock();

    return ret;
}

void furry_record_create(const char* name, void* data) {
    furry_assert(furry_record);

    furry_record_lock();

    // Get record data and fill it
    FurryRecordData* record_data = furry_record_data_get_or_create(name);
    furry_assert(record_data->data == NULL);
    record_data->data = data;
    furry_event_flag_set(record_data->flags, FURRY_RECORD_FLAG_READY);

    furry_record_unlock();
}

bool furry_record_destroy(const char* name) {
    furry_assert(furry_record);

    bool ret = false;

    furry_record_lock();

    FurryRecordData* record_data = furry_record_get(name);
    furry_assert(record_data);
    if(record_data->holders_count == 0) {
        furry_record_erase(name, record_data);
        ret = true;
    }

    furry_record_unlock();

    return ret;
}

void* furry_record_open(const char* name) {
    furry_assert(furry_record);

    furry_record_lock();

    FurryRecordData* record_data = furry_record_data_get_or_create(name);
    record_data->holders_count++;

    furry_record_unlock();

    // Wait for record to become ready
    furry_check(
        furry_event_flag_wait(
            record_data->flags,
            FURRY_RECORD_FLAG_READY,
            FurryFlagWaitAny | FurryFlagNoClear,
            FurryWaitForever) == FURRY_RECORD_FLAG_READY);

    return record_data->data;
}

void furry_record_close(const char* name) {
    furry_assert(furry_record);

    furry_record_lock();

    FurryRecordData* record_data = furry_record_get(name);
    furry_assert(record_data);
    record_data->holders_count--;

    furry_record_unlock();
}
