#pragma once

#include <furry_hal_spi_config.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Early initialize SPI HAL */
void furry_hal_spi_config_init_early();

/** Early deinitialize SPI HAL */
void furry_hal_spi_config_deinit_early();

/** Initialize SPI HAL */
void furry_hal_spi_config_init();

/** Initialize SPI DMA HAL */
void furry_hal_spi_dma_init();

/** Initialize SPI Bus
 *
 * @param      handle  pointer to FurryHalSpiBus instance
 */
void furry_hal_spi_bus_init(FurryHalSpiBus* bus);

/** Deinitialize SPI Bus
 *
 * @param      handle  pointer to FurryHalSpiBus instance
 */
void furry_hal_spi_bus_deinit(FurryHalSpiBus* bus);

/** Initialize SPI Bus Handle
 *
 * @param      handle  pointer to FurryHalSpiBusHandle instance
 */
void furry_hal_spi_bus_handle_init(FurryHalSpiBusHandle* handle);

/** Deinitialize SPI Bus Handle
 *
 * @param      handle  pointer to FurryHalSpiBusHandle instance
 */
void furry_hal_spi_bus_handle_deinit(FurryHalSpiBusHandle* handle);

/** Acquire SPI bus
 *
 * @warning blocking, calls `furry_crash` on programming error, CS transition is up to handler event routine
 *
 * @param      handle  pointer to FurryHalSpiBusHandle instance
 */
void furry_hal_spi_acquire(FurryHalSpiBusHandle* handle);

/** Release SPI bus
 *
 * @warning calls `furry_crash` on programming error, CS transition is up to handler event routine
 * 
 * @param      handle  pointer to FurryHalSpiBusHandle instance
 */
void furry_hal_spi_release(FurryHalSpiBusHandle* handle);

/** SPI Receive
 *
 * @param      handle   pointer to FurryHalSpiBusHandle instance
 * @param      buffer   receive buffer
 * @param      size     transaction size (buffer size)
 * @param      timeout  operation timeout in ms
 *
 * @return     true on sucess
 */
bool furry_hal_spi_bus_rx(
    FurryHalSpiBusHandle* handle,
    uint8_t* buffer,
    size_t size,
    uint32_t timeout);

/** SPI Transmit
 *
 * @param      handle   pointer to FurryHalSpiBusHandle instance
 * @param      buffer   transmit buffer
 * @param      size     transaction size (buffer size)
 * @param      timeout  operation timeout in ms
 *
 * @return     true on success
 */
bool furry_hal_spi_bus_tx(
    FurryHalSpiBusHandle* handle,
    const uint8_t* buffer,
    size_t size,
    uint32_t timeout);

/** SPI Transmit and Receive
 *
 * @param      handle     pointer to FurryHalSpiBusHandle instance
 * @param      tx_buffer  pointer to tx buffer
 * @param      rx_buffer  pointer to rx buffer
 * @param      size       transaction size (buffer size)
 * @param      timeout    operation timeout in ms
 *
 * @return     true on success
 */
bool furry_hal_spi_bus_trx(
    FurryHalSpiBusHandle* handle,
    const uint8_t* tx_buffer,
    uint8_t* rx_buffer,
    size_t size,
    uint32_t timeout);

/** SPI Transmit and Receive with DMA
 *
 * @param      handle     pointer to FurryHalSpiBusHandle instance
 * @param      tx_buffer  pointer to tx buffer
 * @param      rx_buffer  pointer to rx buffer
 * @param      size       transaction size (buffer size)
 * @param      timeout_ms operation timeout in ms
 *
 * @return     true on success
 */
bool furry_hal_spi_bus_trx_dma(
    FurryHalSpiBusHandle* handle,
    uint8_t* tx_buffer,
    uint8_t* rx_buffer,
    size_t size,
    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
