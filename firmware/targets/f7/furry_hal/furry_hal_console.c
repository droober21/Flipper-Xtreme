#include <furry_hal_console.h>
#include <furry_hal_uart.h>

#include <stdbool.h>
#include <stm32wbxx_ll_gpio.h>
#include <stm32wbxx_ll_usart.h>

#include <utilities_conf.h>

#include <furry.h>

#define TAG "FurryHalConsole"

#ifdef HEAP_PRINT_DEBUG
#define CONSOLE_BAUDRATE 1843200
#else
#define CONSOLE_BAUDRATE 230400
#endif

typedef struct {
    bool alive;
    FurryHalConsoleTxCallback tx_callback;
    void* tx_callback_context;
} FurryHalConsole;

FurryHalConsole furry_hal_console = {
    .alive = false,
    .tx_callback = NULL,
    .tx_callback_context = NULL,
};

void furry_hal_console_init() {
    furry_hal_uart_init(FurryHalUartIdUSART1, CONSOLE_BAUDRATE);
    furry_hal_console.alive = true;
}

void furry_hal_console_enable() {
    furry_hal_uart_set_irq_cb(FurryHalUartIdUSART1, NULL, NULL);
    while(!LL_USART_IsActiveFlag_TC(USART1))
        ;
    furry_hal_uart_set_br(FurryHalUartIdUSART1, CONSOLE_BAUDRATE);
    furry_hal_console.alive = true;
}

void furry_hal_console_disable() {
    while(!LL_USART_IsActiveFlag_TC(USART1))
        ;
    furry_hal_console.alive = false;
}

void furry_hal_console_set_tx_callback(FurryHalConsoleTxCallback callback, void* context) {
    FURRY_CRITICAL_ENTER();
    furry_hal_console.tx_callback = callback;
    furry_hal_console.tx_callback_context = context;
    FURRY_CRITICAL_EXIT();
}

void furry_hal_console_tx(const uint8_t* buffer, size_t buffer_size) {
    if(!furry_hal_console.alive) return;

    FURRY_CRITICAL_ENTER();
    // Transmit data

    if(furry_hal_console.tx_callback) {
        furry_hal_console.tx_callback(buffer, buffer_size, furry_hal_console.tx_callback_context);
    }

    furry_hal_uart_tx(FurryHalUartIdUSART1, (uint8_t*)buffer, buffer_size);
    // Wait for TC flag to be raised for last char
    while(!LL_USART_IsActiveFlag_TC(USART1))
        ;
    FURRY_CRITICAL_EXIT();
}

void furry_hal_console_tx_with_new_line(const uint8_t* buffer, size_t buffer_size) {
    if(!furry_hal_console.alive) return;

    FURRY_CRITICAL_ENTER();
    // Transmit data
    furry_hal_uart_tx(FurryHalUartIdUSART1, (uint8_t*)buffer, buffer_size);
    // Transmit new line symbols
    furry_hal_uart_tx(FurryHalUartIdUSART1, (uint8_t*)"\r\n", 2);
    // Wait for TC flag to be raised for last char
    while(!LL_USART_IsActiveFlag_TC(USART1))
        ;
    FURRY_CRITICAL_EXIT();
}

void furry_hal_console_printf(const char format[], ...) {
    FurryString* string;
    va_list args;
    va_start(args, format);
    string = furry_string_alloc_vprintf(format, args);
    va_end(args);
    furry_hal_console_tx((const uint8_t*)furry_string_get_cstr(string), furry_string_size(string));
    furry_string_free(string);
}

void furry_hal_console_puts(const char* data) {
    furry_hal_console_tx((const uint8_t*)data, strlen(data));
}
