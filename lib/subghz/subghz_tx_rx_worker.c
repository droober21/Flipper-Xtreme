#include "subghz_tx_rx_worker.h"

#include <furry.h>

#define TAG "SubGhzTxRxWorker"

#define SUBGHZ_TXRX_WORKER_BUF_SIZE 2048
//you can not set more than 62 because it will not fit into the FIFO CC1101
#define SUBGHZ_TXRX_WORKER_MAX_TXRX_SIZE 60

#define SUBGHZ_TXRX_WORKER_TIMEOUT_READ_WRITE_BUF 40

struct SubGhzTxRxWorker {
    FurryThread* thread;
    FurryStreamBuffer* stream_tx;
    FurryStreamBuffer* stream_rx;

    volatile bool worker_running;
    volatile bool worker_stopping;

    SubGhzTxRxWorkerStatus status;

    uint32_t frequency;

    SubGhzTxRxWorkerCallbackHaveRead callback_have_read;
    void* context_have_read;
};

bool subghz_tx_rx_worker_write(SubGhzTxRxWorker* instance, uint8_t* data, size_t size) {
    furry_assert(instance);
    bool ret = false;
    size_t stream_tx_free_byte = furry_stream_buffer_spaces_available(instance->stream_tx);
    if(size && (stream_tx_free_byte >= size)) {
        if(furry_stream_buffer_send(
               instance->stream_tx, data, size, SUBGHZ_TXRX_WORKER_TIMEOUT_READ_WRITE_BUF) ==
           size) {
            ret = true;
        }
    }
    return ret;
}

size_t subghz_tx_rx_worker_available(SubGhzTxRxWorker* instance) {
    furry_assert(instance);
    return furry_stream_buffer_bytes_available(instance->stream_rx);
}

size_t subghz_tx_rx_worker_read(SubGhzTxRxWorker* instance, uint8_t* data, size_t size) {
    furry_assert(instance);
    return furry_stream_buffer_receive(instance->stream_rx, data, size, 0);
}

void subghz_tx_rx_worker_set_callback_have_read(
    SubGhzTxRxWorker* instance,
    SubGhzTxRxWorkerCallbackHaveRead callback,
    void* context) {
    furry_assert(instance);
    furry_assert(callback);
    furry_assert(context);
    instance->callback_have_read = callback;
    instance->context_have_read = context;
}

bool subghz_tx_rx_worker_rx(SubGhzTxRxWorker* instance, uint8_t* data, uint8_t* size) {
    uint8_t timeout = 100;
    bool ret = false;
    if(instance->status != SubGhzTxRxWorkerStatusRx) {
        furry_hal_subghz_rx();
        instance->status = SubGhzTxRxWorkerStatusRx;
        furry_delay_tick(1);
    }
    //waiting for reception to complete
    while(furry_hal_gpio_read(furry_hal_subghz.cc1101_g0_pin)) {
        furry_delay_tick(1);
        if(!--timeout) {
            FURRY_LOG_W(TAG, "RX cc1101_g0 timeout");
            furry_hal_subghz_flush_rx();
            furry_hal_subghz_rx();
            break;
        }
    }

    if(furry_hal_subghz_rx_pipe_not_empty()) {
        FURRY_LOG_I(
            TAG,
            "RSSI: %03.1fdbm LQI: %d",
            (double)furry_hal_subghz_get_rssi(),
            furry_hal_subghz_get_lqi());
        if(furry_hal_subghz_is_rx_data_crc_valid()) {
            furry_hal_subghz_read_packet(data, size);
            ret = true;
        }
        furry_hal_subghz_flush_rx();
        furry_hal_subghz_rx();
    }
    return ret;
}

void subghz_tx_rx_worker_tx(SubGhzTxRxWorker* instance, uint8_t* data, size_t size) {
    uint8_t timeout = 200;
    if(instance->status != SubGhzTxRxWorkerStatusIDLE) {
        furry_hal_subghz_idle();
    }
    furry_hal_subghz_write_packet(data, size);
    furry_hal_subghz_tx(); //start send
    instance->status = SubGhzTxRxWorkerStatusTx;
    while(!furry_hal_gpio_read(
        furry_hal_subghz.cc1101_g0_pin)) { // Wait for GDO0 to be set -> sync transmitted
        furry_delay_tick(1);
        if(!--timeout) {
            FURRY_LOG_W(TAG, "TX !cc1101_g0 timeout");
            break;
        }
    }
    while(furry_hal_gpio_read(
        furry_hal_subghz.cc1101_g0_pin)) { // Wait for GDO0 to be cleared -> end of packet
        furry_delay_tick(1);
        if(!--timeout) {
            FURRY_LOG_W(TAG, "TX cc1101_g0 timeout");
            break;
        }
    }
    furry_hal_subghz_idle();
    instance->status = SubGhzTxRxWorkerStatusIDLE;
}
/** Worker thread
 * 
 * @param context 
 * @return exit code 
 */
static int32_t subghz_tx_rx_worker_thread(void* context) {
    SubGhzTxRxWorker* instance = context;
    FURRY_LOG_I(TAG, "Worker start");

    furry_hal_subghz_reset();
    furry_hal_subghz_idle();
    furry_hal_subghz_load_preset(FurryHalSubGhzPresetGFSK9_99KbAsync);
    //furry_hal_subghz_load_preset(FurryHalSubGhzPresetMSK99_97KbAsync);
    furry_hal_gpio_init(furry_hal_subghz.cc1101_g0_pin, GpioModeInput, GpioPullNo, GpioSpeedLow);

    furry_hal_subghz_set_frequency_and_path(instance->frequency);
    furry_hal_subghz_flush_rx();

    uint8_t data[SUBGHZ_TXRX_WORKER_MAX_TXRX_SIZE + 1] = {0};
    size_t size_tx = 0;
    uint8_t size_rx[1] = {0};
    uint8_t timeout_tx = 0;
    bool callback_rx = false;

    while(instance->worker_running) {
        //transmit
        size_tx = furry_stream_buffer_bytes_available(instance->stream_tx);
        if(size_tx > 0 && !timeout_tx) {
            timeout_tx = 10; //20ms
            if(size_tx > SUBGHZ_TXRX_WORKER_MAX_TXRX_SIZE) {
                furry_stream_buffer_receive(
                    instance->stream_tx,
                    &data,
                    SUBGHZ_TXRX_WORKER_MAX_TXRX_SIZE,
                    SUBGHZ_TXRX_WORKER_TIMEOUT_READ_WRITE_BUF);
                subghz_tx_rx_worker_tx(instance, data, SUBGHZ_TXRX_WORKER_MAX_TXRX_SIZE);
            } else {
                //todo checking that he managed to write all the data to the TX buffer
                furry_stream_buffer_receive(
                    instance->stream_tx, &data, size_tx, SUBGHZ_TXRX_WORKER_TIMEOUT_READ_WRITE_BUF);
                subghz_tx_rx_worker_tx(instance, data, size_tx);
            }
        } else {
            //receive
            if(subghz_tx_rx_worker_rx(instance, data, size_rx)) {
                if(furry_stream_buffer_spaces_available(instance->stream_rx) >= size_rx[0]) {
                    if(instance->callback_have_read &&
                       furry_stream_buffer_bytes_available(instance->stream_rx) == 0) {
                        callback_rx = true;
                    }
                    //todo checking that he managed to write all the data to the RX buffer
                    furry_stream_buffer_send(
                        instance->stream_rx,
                        &data,
                        size_rx[0],
                        SUBGHZ_TXRX_WORKER_TIMEOUT_READ_WRITE_BUF);
                    if(callback_rx) {
                        instance->callback_have_read(instance->context_have_read);
                        callback_rx = false;
                    }
                } else {
                    //todo RX buffer overflow
                }
            }
        }

        if(timeout_tx) timeout_tx--;
        furry_delay_tick(1);
    }

    furry_hal_subghz_set_path(FurryHalSubGhzPathIsolate);
    furry_hal_subghz_sleep();

    FURRY_LOG_I(TAG, "Worker stop");
    return 0;
}

SubGhzTxRxWorker* subghz_tx_rx_worker_alloc() {
    SubGhzTxRxWorker* instance = malloc(sizeof(SubGhzTxRxWorker));

    instance->thread =
        furry_thread_alloc_ex("SubGhzTxRxWorker", 2048, subghz_tx_rx_worker_thread, instance);
    instance->stream_tx =
        furry_stream_buffer_alloc(sizeof(uint8_t) * SUBGHZ_TXRX_WORKER_BUF_SIZE, sizeof(uint8_t));
    instance->stream_rx =
        furry_stream_buffer_alloc(sizeof(uint8_t) * SUBGHZ_TXRX_WORKER_BUF_SIZE, sizeof(uint8_t));

    instance->status = SubGhzTxRxWorkerStatusIDLE;
    instance->worker_stopping = true;

    return instance;
}

void subghz_tx_rx_worker_free(SubGhzTxRxWorker* instance) {
    furry_assert(instance);
    furry_assert(!instance->worker_running);
    furry_stream_buffer_free(instance->stream_tx);
    furry_stream_buffer_free(instance->stream_rx);
    furry_thread_free(instance->thread);

    free(instance);
}

bool subghz_tx_rx_worker_start(SubGhzTxRxWorker* instance, uint32_t frequency) {
    furry_assert(instance);
    furry_assert(!instance->worker_running);
    bool res = false;
    furry_stream_buffer_reset(instance->stream_tx);
    furry_stream_buffer_reset(instance->stream_rx);

    instance->worker_running = true;

    if(furry_hal_subghz_is_tx_allowed(frequency)) {
        instance->frequency = frequency;
        res = true;
    }

    furry_thread_start(instance->thread);

    return res;
}

void subghz_tx_rx_worker_stop(SubGhzTxRxWorker* instance) {
    furry_assert(instance);
    furry_assert(instance->worker_running);

    instance->worker_running = false;

    furry_thread_join(instance->thread);
}

bool subghz_tx_rx_worker_is_running(SubGhzTxRxWorker* instance) {
    furry_assert(instance);
    return instance->worker_running;
}
