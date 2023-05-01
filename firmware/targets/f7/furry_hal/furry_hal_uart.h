/**
 * @file furry_hal_uart.h
 * @version 1.0
 * @date 2021-11-19
 * 
 * UART HAL api interface
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UART channels
 */
typedef enum {
    FurryHalUartIdUSART1,
    FurryHalUartIdLPUART1,
} FurryHalUartId;

/**
 * UART events
 */
typedef enum {
    UartIrqEventRXNE,
} UartIrqEvent;

/**
 * Init UART
 * Configures GPIO to UART function, configures UART hardware, enables UART hardware
 * @param channel UART channel
 * @param baud baudrate
 */
void furry_hal_uart_init(FurryHalUartId channel, uint32_t baud);

/**
 * Deinit UART
 * Configures GPIO to analog, clears callback and callback context, disables UART hardware
 * @param channel UART channel
 */
void furry_hal_uart_deinit(FurryHalUartId channel);

/**
 * Suspend UART operation
 * Disables UART hardware, settings and callbacks are preserved
 * @param channel UART channel
 */
void furry_hal_uart_suspend(FurryHalUartId channel);

/**
 * Resume UART operation
 * Resumes UART hardware from suspended state
 * @param channel UART channel
 */
void furry_hal_uart_resume(FurryHalUartId channel);

/**
 * Changes UART baudrate
 * @param channel UART channel
 * @param baud baudrate
 */
void furry_hal_uart_set_br(FurryHalUartId channel, uint32_t baud);

/**
 * Transmits data
 * @param channel UART channel
 * @param buffer data
 * @param buffer_size data size (in bytes)
 */
void furry_hal_uart_tx(FurryHalUartId channel, uint8_t* buffer, size_t buffer_size);

/**
 * Sets UART event callback
 * @param channel UART channel
 * @param callback callback pointer
 * @param context callback context
 */
void furry_hal_uart_set_irq_cb(
    FurryHalUartId channel,
    void (*callback)(UartIrqEvent event, uint8_t data, void* context),
    void* context);

#ifdef __cplusplus
}
#endif
