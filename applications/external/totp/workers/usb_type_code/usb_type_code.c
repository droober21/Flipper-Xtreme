#include "usb_type_code.h"
#include <furry_hal_usb.h>
#include <furry_hal_usb_hid.h>
#include <furry/core/thread.h>
#include <furry/core/kernel.h>
#include <furry/core/check.h>
#include "../../services/convert/convert.h"
#include "../../types/token_info.h"
#include "../type_code_common.h"

struct TotpUsbTypeCodeWorkerContext {
    char* code_buffer;
    uint8_t code_buffer_size;
    uint8_t flags;
    FurryThread* thread;
    FurryMutex* code_buffer_sync;
    FurryHalUsbInterface* usb_mode_prev;
};

static void totp_type_code_worker_restore_usb_mode(TotpUsbTypeCodeWorkerContext* context) {
    if(context->usb_mode_prev != NULL) {
        furry_hal_usb_set_config(context->usb_mode_prev, NULL);
        context->usb_mode_prev = NULL;
    }
}

static inline bool totp_type_code_worker_stop_requested() {
    return furry_thread_flags_get() & TotpUsbTypeCodeWorkerEventStop;
}

static void totp_type_code_worker_type_code(TotpUsbTypeCodeWorkerContext* context) {
    context->usb_mode_prev = furry_hal_usb_get_config();
    furry_hal_usb_unlock();
    furry_check(furry_hal_usb_set_config(&usb_hid, NULL) == true);
    uint8_t i = 0;
    do {
        furry_delay_ms(500);
        i++;
    } while(!furry_hal_hid_is_connected() && i < 100 && !totp_type_code_worker_stop_requested());

    if(furry_hal_hid_is_connected() &&
       furry_mutex_acquire(context->code_buffer_sync, 500) == FurryStatusOk) {
        totp_type_code_worker_execute_automation(
            &furry_hal_hid_kb_press,
            &furry_hal_hid_kb_release,
            context->code_buffer,
            context->code_buffer_size,
            context->flags);
        furry_mutex_release(context->code_buffer_sync);

        furry_delay_ms(100);
    }

    totp_type_code_worker_restore_usb_mode(context);
}

static int32_t totp_type_code_worker_callback(void* context) {
    furry_check(context);
    FurryMutex* context_mutex = furry_mutex_alloc(FurryMutexTypeNormal);

    while(true) {
        uint32_t flags = furry_thread_flags_wait(
            TotpUsbTypeCodeWorkerEventStop | TotpUsbTypeCodeWorkerEventType,
            FurryFlagWaitAny,
            FurryWaitForever);
        furry_check((flags & FurryFlagError) == 0); //-V562
        if(flags & TotpUsbTypeCodeWorkerEventStop) break;

        if(furry_mutex_acquire(context_mutex, FurryWaitForever) == FurryStatusOk) {
            if(flags & TotpUsbTypeCodeWorkerEventType) {
                totp_type_code_worker_type_code(context);
            }

            furry_mutex_release(context_mutex);
        }
    }

    furry_mutex_free(context_mutex);

    return 0;
}

TotpUsbTypeCodeWorkerContext* totp_usb_type_code_worker_start(
    char* code_buffer,
    uint8_t code_buffer_size,
    FurryMutex* code_buffer_sync) {
    TotpUsbTypeCodeWorkerContext* context = malloc(sizeof(TotpUsbTypeCodeWorkerContext));
    furry_check(context != NULL);
    context->code_buffer = code_buffer;
    context->code_buffer_size = code_buffer_size;
    context->code_buffer_sync = code_buffer_sync;
    context->thread = furry_thread_alloc();
    context->usb_mode_prev = NULL;
    furry_thread_set_name(context->thread, "TOTPUsbHidWorker");
    furry_thread_set_stack_size(context->thread, 1024);
    furry_thread_set_context(context->thread, context);
    furry_thread_set_callback(context->thread, totp_type_code_worker_callback);
    furry_thread_start(context->thread);
    return context;
}

void totp_usb_type_code_worker_stop(TotpUsbTypeCodeWorkerContext* context) {
    furry_check(context != NULL);
    furry_thread_flags_set(furry_thread_get_id(context->thread), TotpUsbTypeCodeWorkerEventStop);
    furry_thread_join(context->thread);
    furry_thread_free(context->thread);
    totp_type_code_worker_restore_usb_mode(context);
    free(context);
}

void totp_usb_type_code_worker_notify(
    TotpUsbTypeCodeWorkerContext* context,
    TotpUsbTypeCodeWorkerEvent event,
    uint8_t flags) {
    furry_check(context != NULL);
    context->flags = flags;
    furry_thread_flags_set(furry_thread_get_id(context->thread), event);
}