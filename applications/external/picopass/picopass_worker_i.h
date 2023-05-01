#pragma once

#include "picopass_worker.h"
#include "picopass_i.h"

#include <furry.h>
#include <lib/toolbox/stream/file_stream.h>

#include <furry_hal.h>

#include <stdlib.h>
#include <rfal_rf.h>

#include <platform.h>

struct PicopassWorker {
    FurryThread* thread;
    Storage* storage;
    Stream* dict_stream;

    PicopassDeviceData* dev_data;
    PicopassWorkerCallback callback;
    void* context;

    PicopassWorkerState state;
};

void picopass_worker_change_state(PicopassWorker* picopass_worker, PicopassWorkerState state);

int32_t picopass_worker_task(void* context);

void picopass_worker_detect(PicopassWorker* picopass_worker);
void picopass_worker_write(PicopassWorker* picopass_worker);
void picopass_worker_write_key(PicopassWorker* picopass_worker);
