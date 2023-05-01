#pragma once

#include <furry.h>

#include "nfc_magic_worker.h"

struct NfcMagicWorker {
    FurryThread* thread;

    NfcDeviceData* dev_data;

    NfcMagicWorkerCallback callback;
    void* context;

    NfcMagicWorkerState state;
};

int32_t nfc_magic_worker_task(void* context);

void nfc_magic_worker_check(NfcMagicWorker* nfc_magic_worker);

void nfc_magic_worker_write(NfcMagicWorker* nfc_magic_worker);

void nfc_magic_worker_wipe(NfcMagicWorker* nfc_magic_worker);
