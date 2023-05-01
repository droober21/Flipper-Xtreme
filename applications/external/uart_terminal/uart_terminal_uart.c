#include "uart_terminal_app_i.h"
#include "uart_terminal_uart.h"

//#define UART_CH (FurryHalUartIdUSART1)
//#define BAUDRATE (115200)

struct UART_TerminalUart {
    UART_TerminalApp* app;
    FurryThread* rx_thread;
    FurryStreamBuffer* rx_stream;
    uint8_t rx_buf[RX_BUF_SIZE + 1];
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context);
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

void uart_terminal_uart_set_handle_rx_data_cb(
    UART_TerminalUart* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context)) {
    furry_assert(uart);
    uart->handle_rx_data_cb = handle_rx_data_cb;
}

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

void uart_terminal_uart_on_irq_cb(UartIrqEvent ev, uint8_t data, void* context) {
    UART_TerminalUart* uart = (UART_TerminalUart*)context;

    if(ev == UartIrqEventRXNE) {
        furry_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furry_thread_flags_set(furry_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

static int32_t uart_worker(void* context) {
    UART_TerminalUart* uart = (void*)context;

    uart->rx_stream = furry_stream_buffer_alloc(RX_BUF_SIZE, 1);

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

void uart_terminal_uart_tx(uint8_t* data, size_t len) {
    furry_hal_uart_tx(UART_CH, data, len);
}

UART_TerminalUart* uart_terminal_uart_init(UART_TerminalApp* app) {
    UART_TerminalUart* uart = malloc(sizeof(UART_TerminalUart));

    furry_hal_console_disable();
    if(app->BAUDRATE == 0) {
        app->BAUDRATE = 115200;
    }
    furry_hal_uart_set_br(UART_CH, app->BAUDRATE);
    furry_hal_uart_set_irq_cb(UART_CH, uart_terminal_uart_on_irq_cb, uart);

    uart->app = app;
    uart->rx_thread = furry_thread_alloc();
    furry_thread_set_name(uart->rx_thread, "UART_TerminalUartRxThread");
    furry_thread_set_stack_size(uart->rx_thread, 1024);
    furry_thread_set_context(uart->rx_thread, uart);
    furry_thread_set_callback(uart->rx_thread, uart_worker);

    furry_thread_start(uart->rx_thread);
    return uart;
}

void uart_terminal_uart_free(UART_TerminalUart* uart) {
    furry_assert(uart);

    furry_thread_flags_set(furry_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furry_thread_join(uart->rx_thread);
    furry_thread_free(uart->rx_thread);

    furry_hal_uart_set_irq_cb(UART_CH, NULL, NULL);
    furry_hal_console_enable();

    free(uart);
}