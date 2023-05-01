#include "wifi_marauder_app_i.h"
#include "wifi_marauder_uart.h"

#define UART_CH (FurryHalUartIdUSART1)
#define LP_UART_CH (FurryHalUartIdLPUART1)
#define BAUDRATE (115200)

struct WifiMarauderUart {
    WifiMarauderApp* app;
    FurryHalUartId channel;
    FurryThread* rx_thread;
    FurryStreamBuffer* rx_stream;
    uint8_t rx_buf[RX_BUF_SIZE + 1];
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context);
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

void wifi_marauder_uart_set_handle_rx_data_cb(
    WifiMarauderUart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context)) {
    furry_assert(uart);
    uart->handle_rx_data_cb = handle_rx_data_cb;
}

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

void wifi_marauder_uart_on_irq_cb(UartIrqEvent ev, uint8_t data, void* context) {
    WifiMarauderUart* uart = (WifiMarauderUart*)context;

    if(ev == UartIrqEventRXNE) {
        furry_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furry_thread_flags_set(furry_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

static int32_t uart_worker(void* context) {
    WifiMarauderUart* uart = (void*)context;

    while(1) {
        uint32_t events =
            furry_thread_flags_wait(WORKER_ALL_RX_EVENTS, FurryFlagWaitAny, FurryWaitForever);
        furry_check((events & FurryFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            size_t len = furry_stream_buffer_receive(uart->rx_stream, uart->rx_buf, RX_BUF_SIZE, 0);
            if(len > 0) {
                if(uart->handle_rx_data_cb) uart->handle_rx_data_cb(uart->rx_buf, len, uart->app);
            }
        }
    }

    furry_stream_buffer_free(uart->rx_stream);

    return 0;
}

void wifi_marauder_uart_tx(uint8_t* data, size_t len) {
    furry_hal_uart_tx(UART_CH, data, len);
}

void wifi_marauder_lp_uart_tx(uint8_t* data, size_t len) {
    furry_hal_uart_tx(LP_UART_CH, data, len);
}

WifiMarauderUart*
    wifi_marauder_uart_init(WifiMarauderApp* app, FurryHalUartId channel, const char* thread_name) {
    WifiMarauderUart* uart = malloc(sizeof(WifiMarauderUart));

    uart->app = app;
    uart->channel = channel;
    uart->rx_stream = furry_stream_buffer_alloc(RX_BUF_SIZE, 1);
    uart->rx_thread = furry_thread_alloc();
    furry_thread_set_name(uart->rx_thread, thread_name);
    furry_thread_set_stack_size(uart->rx_thread, 1024);
    furry_thread_set_context(uart->rx_thread, uart);
    furry_thread_set_callback(uart->rx_thread, uart_worker);
    furry_thread_start(uart->rx_thread);
    if(channel == FurryHalUartIdUSART1) {
        furry_hal_console_disable();
    } else if(channel == FurryHalUartIdLPUART1) {
        furry_hal_uart_init(channel, BAUDRATE);
    }
    furry_hal_uart_set_br(channel, BAUDRATE);
    furry_hal_uart_set_irq_cb(channel, wifi_marauder_uart_on_irq_cb, uart);

    return uart;
}

WifiMarauderUart* wifi_marauder_usart_init(WifiMarauderApp* app) {
    return wifi_marauder_uart_init(app, UART_CH, "WifiMarauderUartRxThread");
}

WifiMarauderUart* wifi_marauder_lp_uart_init(WifiMarauderApp* app) {
    return wifi_marauder_uart_init(app, LP_UART_CH, "WifiMarauderLPUartRxThread");
}

void wifi_marauder_uart_free(WifiMarauderUart* uart) {
    furry_assert(uart);

    furry_thread_flags_set(furry_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furry_thread_join(uart->rx_thread);
    furry_thread_free(uart->rx_thread);

    furry_hal_uart_set_irq_cb(uart->channel, NULL, NULL);
    furry_hal_console_enable();

    free(uart);
}