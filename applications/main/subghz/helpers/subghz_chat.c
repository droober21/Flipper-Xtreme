#include "subghz_chat.h"
#include <lib/subghz/subghz_tx_rx_worker.h>

#define TAG "SubGhzChat"
#define SUBGHZ_CHAT_WORKER_TIMEOUT_BETWEEN_MESSAGES 500

struct SubGhzChatWorker {
    FurryThread* thread;
    SubGhzTxRxWorker* subghz_txrx;

    volatile bool worker_running;
    volatile bool worker_stopping;
    FurryMessageQueue* event_queue;
    uint32_t last_time_rx_data;

    Cli* cli;
};

/** Worker thread
 * 
 * @param context 
 * @return exit code 
 */
static int32_t subghz_chat_worker_thread(void* context) {
    SubGhzChatWorker* instance = context;
    FURRY_LOG_I(TAG, "Worker start");
    char c;
    SubGhzChatEvent event;
    event.event = SubGhzChatEventUserEntrance;
    furry_message_queue_put(instance->event_queue, &event, 0);
    while(instance->worker_running) {
        if(cli_read_timeout(instance->cli, (uint8_t*)&c, 1, 1000) == 1) {
            event.event = SubGhzChatEventInputData;
            event.c = c;
            furry_message_queue_put(instance->event_queue, &event, FurryWaitForever);
        }
    }

    FURRY_LOG_I(TAG, "Worker stop");
    return 0;
}

static void subghz_chat_worker_update_rx_event_chat(void* context) {
    furry_assert(context);
    SubGhzChatWorker* instance = context;
    SubGhzChatEvent event;
    if((furry_get_tick() - instance->last_time_rx_data) >
       SUBGHZ_CHAT_WORKER_TIMEOUT_BETWEEN_MESSAGES) {
        event.event = SubGhzChatEventNewMessage;
        furry_message_queue_put(instance->event_queue, &event, FurryWaitForever);
    }
    instance->last_time_rx_data = furry_get_tick();
    event.event = SubGhzChatEventRXData;
    furry_message_queue_put(instance->event_queue, &event, FurryWaitForever);
}

SubGhzChatWorker* subghz_chat_worker_alloc(Cli* cli) {
    SubGhzChatWorker* instance = malloc(sizeof(SubGhzChatWorker));

    instance->cli = cli;

    instance->thread =
        furry_thread_alloc_ex("SubGhzChat", 2048, subghz_chat_worker_thread, instance);
    instance->subghz_txrx = subghz_tx_rx_worker_alloc();
    instance->event_queue = furry_message_queue_alloc(80, sizeof(SubGhzChatEvent));
    return instance;
}

void subghz_chat_worker_free(SubGhzChatWorker* instance) {
    furry_assert(instance);
    furry_assert(!instance->worker_running);
    furry_message_queue_free(instance->event_queue);
    subghz_tx_rx_worker_free(instance->subghz_txrx);
    furry_thread_free(instance->thread);

    free(instance);
}

bool subghz_chat_worker_start(SubGhzChatWorker* instance, uint32_t frequency) {
    furry_assert(instance);
    furry_assert(!instance->worker_running);
    bool res = false;

    if(subghz_tx_rx_worker_start(instance->subghz_txrx, frequency)) {
        furry_message_queue_reset(instance->event_queue);
        subghz_tx_rx_worker_set_callback_have_read(
            instance->subghz_txrx, subghz_chat_worker_update_rx_event_chat, instance);

        instance->worker_running = true;
        instance->last_time_rx_data = 0;

        furry_thread_start(instance->thread);

        res = true;
    }
    return res;
}

void subghz_chat_worker_stop(SubGhzChatWorker* instance) {
    furry_assert(instance);
    furry_assert(instance->worker_running);
    if(subghz_tx_rx_worker_is_running(instance->subghz_txrx)) {
        subghz_tx_rx_worker_stop(instance->subghz_txrx);
    }

    instance->worker_running = false;

    furry_thread_join(instance->thread);
}

bool subghz_chat_worker_is_running(SubGhzChatWorker* instance) {
    furry_assert(instance);
    return instance->worker_running;
}

SubGhzChatEvent subghz_chat_worker_get_event_chat(SubGhzChatWorker* instance) {
    furry_assert(instance);
    SubGhzChatEvent event;
    if(furry_message_queue_get(instance->event_queue, &event, FurryWaitForever) == FurryStatusOk) {
        return event;
    } else {
        event.event = SubGhzChatEventNoEvent;
        return event;
    }
}

void subghz_chat_worker_put_event_chat(SubGhzChatWorker* instance, SubGhzChatEvent* event) {
    furry_assert(instance);
    furry_message_queue_put(instance->event_queue, event, FurryWaitForever);
}

size_t subghz_chat_worker_available(SubGhzChatWorker* instance) {
    furry_assert(instance);
    return subghz_tx_rx_worker_available(instance->subghz_txrx);
}

size_t subghz_chat_worker_read(SubGhzChatWorker* instance, uint8_t* data, size_t size) {
    furry_assert(instance);
    return subghz_tx_rx_worker_read(instance->subghz_txrx, data, size);
}

bool subghz_chat_worker_write(SubGhzChatWorker* instance, uint8_t* data, size_t size) {
    furry_assert(instance);
    return subghz_tx_rx_worker_write(instance->subghz_txrx, data, size);
}
