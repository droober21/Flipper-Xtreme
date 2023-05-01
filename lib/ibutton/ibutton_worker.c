#include "ibutton_worker_i.h"
#include "ibutton_protocols.h"

#include <core/check.h>

typedef enum {
    iButtonMessageEnd,
    iButtonMessageStop,
    iButtonMessageRead,
    iButtonMessageWriteBlank,
    iButtonMessageWriteCopy,
    iButtonMessageEmulate,
    iButtonMessageNotifyEmulate,
} iButtonMessageType;

typedef struct {
    iButtonMessageType type;
    union {
        iButtonKey* key;
    } data;
} iButtonMessage;

static int32_t ibutton_worker_thread(void* thread_context);

iButtonWorker* ibutton_worker_alloc(iButtonProtocols* protocols) {
    iButtonWorker* worker = malloc(sizeof(iButtonWorker));

    worker->protocols = protocols;
    worker->messages = furry_message_queue_alloc(1, sizeof(iButtonMessage));

    worker->mode_index = iButtonWorkerModeIdle;
    worker->thread = furry_thread_alloc_ex("iButtonWorker", 2048, ibutton_worker_thread, worker);

    return worker;
}

void ibutton_worker_read_set_callback(
    iButtonWorker* worker,
    iButtonWorkerReadCallback callback,
    void* context) {
    furry_check(worker->mode_index == iButtonWorkerModeIdle);
    worker->read_cb = callback;
    worker->cb_ctx = context;
}

void ibutton_worker_write_set_callback(
    iButtonWorker* worker,
    iButtonWorkerWriteCallback callback,
    void* context) {
    furry_check(worker->mode_index == iButtonWorkerModeIdle);
    worker->write_cb = callback;
    worker->cb_ctx = context;
}

void ibutton_worker_emulate_set_callback(
    iButtonWorker* worker,
    iButtonWorkerEmulateCallback callback,
    void* context) {
    furry_check(worker->mode_index == iButtonWorkerModeIdle);
    worker->emulate_cb = callback;
    worker->cb_ctx = context;
}

void ibutton_worker_read_start(iButtonWorker* worker, iButtonKey* key) {
    iButtonMessage message = {.type = iButtonMessageRead, .data.key = key};
    furry_check(
        furry_message_queue_put(worker->messages, &message, FurryWaitForever) == FurryStatusOk);
}

void ibutton_worker_write_blank_start(iButtonWorker* worker, iButtonKey* key) {
    iButtonMessage message = {.type = iButtonMessageWriteBlank, .data.key = key};
    furry_check(
        furry_message_queue_put(worker->messages, &message, FurryWaitForever) == FurryStatusOk);
}

void ibutton_worker_write_copy_start(iButtonWorker* worker, iButtonKey* key) {
    iButtonMessage message = {.type = iButtonMessageWriteCopy, .data.key = key};
    furry_check(
        furry_message_queue_put(worker->messages, &message, FurryWaitForever) == FurryStatusOk);
}

void ibutton_worker_emulate_start(iButtonWorker* worker, iButtonKey* key) {
    iButtonMessage message = {.type = iButtonMessageEmulate, .data.key = key};
    furry_check(
        furry_message_queue_put(worker->messages, &message, FurryWaitForever) == FurryStatusOk);
}

void ibutton_worker_stop(iButtonWorker* worker) {
    iButtonMessage message = {.type = iButtonMessageStop};
    furry_check(
        furry_message_queue_put(worker->messages, &message, FurryWaitForever) == FurryStatusOk);
}

void ibutton_worker_free(iButtonWorker* worker) {
    furry_message_queue_free(worker->messages);
    furry_thread_free(worker->thread);
    free(worker);
}

void ibutton_worker_start_thread(iButtonWorker* worker) {
    furry_thread_start(worker->thread);
}

void ibutton_worker_stop_thread(iButtonWorker* worker) {
    iButtonMessage message = {.type = iButtonMessageEnd};
    furry_check(
        furry_message_queue_put(worker->messages, &message, FurryWaitForever) == FurryStatusOk);
    furry_thread_join(worker->thread);
}

void ibutton_worker_switch_mode(iButtonWorker* worker, iButtonWorkerMode mode) {
    ibutton_worker_modes[worker->mode_index].stop(worker);
    worker->mode_index = mode;
    ibutton_worker_modes[worker->mode_index].start(worker);
}

void ibutton_worker_notify_emulate(iButtonWorker* worker) {
    iButtonMessage message = {.type = iButtonMessageNotifyEmulate};
    // we're running in an interrupt context, so we can't wait
    // and we can drop message if queue is full, that's ok for that message
    furry_message_queue_put(worker->messages, &message, 0);
}

void ibutton_worker_set_key_p(iButtonWorker* worker, iButtonKey* key) {
    worker->key = key;
}

static int32_t ibutton_worker_thread(void* thread_context) {
    iButtonWorker* worker = thread_context;
    bool running = true;
    iButtonMessage message;
    FurryStatus status;

    ibutton_worker_modes[worker->mode_index].start(worker);

    while(running) {
        status = furry_message_queue_get(
            worker->messages, &message, ibutton_worker_modes[worker->mode_index].quant);
        if(status == FurryStatusOk) {
            switch(message.type) {
            case iButtonMessageEnd:
                ibutton_worker_switch_mode(worker, iButtonWorkerModeIdle);
                ibutton_worker_set_key_p(worker, NULL);
                running = false;
                break;
            case iButtonMessageStop:
                ibutton_worker_switch_mode(worker, iButtonWorkerModeIdle);
                ibutton_worker_set_key_p(worker, NULL);
                break;
            case iButtonMessageRead:
                ibutton_worker_set_key_p(worker, message.data.key);
                ibutton_worker_switch_mode(worker, iButtonWorkerModeRead);
                break;
            case iButtonMessageWriteBlank:
                ibutton_worker_set_key_p(worker, message.data.key);
                ibutton_worker_switch_mode(worker, iButtonWorkerModeWriteBlank);
                break;
            case iButtonMessageWriteCopy:
                ibutton_worker_set_key_p(worker, message.data.key);
                ibutton_worker_switch_mode(worker, iButtonWorkerModeWriteCopy);
                break;
            case iButtonMessageEmulate:
                ibutton_worker_set_key_p(worker, message.data.key);
                ibutton_worker_switch_mode(worker, iButtonWorkerModeEmulate);
                break;
            case iButtonMessageNotifyEmulate:
                if(worker->emulate_cb) {
                    worker->emulate_cb(worker->cb_ctx, true);
                }
                break;
            }
        } else if(status == FurryStatusErrorTimeout) {
            ibutton_worker_modes[worker->mode_index].tick(worker);
        } else {
            furry_crash("iButton worker error");
        }
    }

    ibutton_worker_modes[worker->mode_index].stop(worker);

    return 0;
}
