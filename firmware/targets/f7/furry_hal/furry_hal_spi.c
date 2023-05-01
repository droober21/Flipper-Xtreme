#include <furry.h>
#include <furry_hal_spi.h>
#include <furry_hal_resources.h>
#include <furry_hal_power.h>
#include <furry_hal_interrupt.h>

#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_spi.h>
#include <stm32wbxx_ll_utils.h>
#include <stm32wbxx_ll_cortex.h>

#define TAG "FurryHalSpi"

#define SPI_DMA DMA2
#define SPI_DMA_RX_CHANNEL LL_DMA_CHANNEL_3
#define SPI_DMA_TX_CHANNEL LL_DMA_CHANNEL_4
#define SPI_DMA_RX_IRQ FurryHalInterruptIdDma2Ch3
#define SPI_DMA_TX_IRQ FurryHalInterruptIdDma2Ch4
#define SPI_DMA_RX_DEF SPI_DMA, SPI_DMA_RX_CHANNEL
#define SPI_DMA_TX_DEF SPI_DMA, SPI_DMA_TX_CHANNEL

// For simplicity, I assume that only one SPI DMA transaction can occur at a time.
static FurrySemaphore* spi_dma_lock = NULL;
static FurrySemaphore* spi_dma_completed = NULL;

void furry_hal_spi_dma_init() {
    spi_dma_lock = furry_semaphore_alloc(1, 1);
    spi_dma_completed = furry_semaphore_alloc(1, 1);
}

void furry_hal_spi_bus_init(FurryHalSpiBus* bus) {
    furry_assert(bus);
    bus->callback(bus, FurryHalSpiBusEventInit);
}

void furry_hal_spi_bus_deinit(FurryHalSpiBus* bus) {
    furry_assert(bus);
    bus->callback(bus, FurryHalSpiBusEventDeinit);
}

void furry_hal_spi_bus_handle_init(FurryHalSpiBusHandle* handle) {
    furry_assert(handle);
    handle->callback(handle, FurryHalSpiBusHandleEventInit);
}

void furry_hal_spi_bus_handle_deinit(FurryHalSpiBusHandle* handle) {
    furry_assert(handle);
    handle->callback(handle, FurryHalSpiBusHandleEventDeinit);
}

void furry_hal_spi_acquire(FurryHalSpiBusHandle* handle) {
    furry_assert(handle);

    furry_hal_power_insomnia_enter();

    handle->bus->callback(handle->bus, FurryHalSpiBusEventLock);
    handle->bus->callback(handle->bus, FurryHalSpiBusEventActivate);

    furry_assert(handle->bus->current_handle == NULL);

    handle->bus->current_handle = handle;
    handle->callback(handle, FurryHalSpiBusHandleEventActivate);
}

void furry_hal_spi_release(FurryHalSpiBusHandle* handle) {
    furry_assert(handle);
    furry_assert(handle->bus->current_handle == handle);

    // Handle event and unset handle
    handle->callback(handle, FurryHalSpiBusHandleEventDeactivate);
    handle->bus->current_handle = NULL;

    // Bus events
    handle->bus->callback(handle->bus, FurryHalSpiBusEventDeactivate);
    handle->bus->callback(handle->bus, FurryHalSpiBusEventUnlock);

    furry_hal_power_insomnia_exit();
}

static void furry_hal_spi_bus_end_txrx(FurryHalSpiBusHandle* handle, uint32_t timeout) {
    UNUSED(timeout); // FIXME
    while(LL_SPI_GetTxFIFOLevel(handle->bus->spi) != LL_SPI_TX_FIFO_EMPTY)
        ;
    while(LL_SPI_IsActiveFlag_BSY(handle->bus->spi))
        ;
    while(LL_SPI_GetRxFIFOLevel(handle->bus->spi) != LL_SPI_RX_FIFO_EMPTY) {
        LL_SPI_ReceiveData8(handle->bus->spi);
    }
}

bool furry_hal_spi_bus_rx(
    FurryHalSpiBusHandle* handle,
    uint8_t* buffer,
    size_t size,
    uint32_t timeout) {
    furry_assert(handle);
    furry_assert(handle->bus->current_handle == handle);
    furry_assert(buffer);
    furry_assert(size > 0);

    return furry_hal_spi_bus_trx(handle, buffer, buffer, size, timeout);
}

bool furry_hal_spi_bus_tx(
    FurryHalSpiBusHandle* handle,
    const uint8_t* buffer,
    size_t size,
    uint32_t timeout) {
    furry_assert(handle);
    furry_assert(handle->bus->current_handle == handle);
    furry_assert(buffer);
    furry_assert(size > 0);
    bool ret = true;

    while(size > 0) {
        if(LL_SPI_IsActiveFlag_TXE(handle->bus->spi)) {
            LL_SPI_TransmitData8(handle->bus->spi, *buffer);
            buffer++;
            size--;
        }
    }

    furry_hal_spi_bus_end_txrx(handle, timeout);
    LL_SPI_ClearFlag_OVR(handle->bus->spi);

    return ret;
}

bool furry_hal_spi_bus_trx(
    FurryHalSpiBusHandle* handle,
    const uint8_t* tx_buffer,
    uint8_t* rx_buffer,
    size_t size,
    uint32_t timeout) {
    furry_assert(handle);
    furry_assert(handle->bus->current_handle == handle);
    furry_assert(size > 0);

    bool ret = true;
    size_t tx_size = size;
    bool tx_allowed = true;

    while(size > 0) {
        if(tx_size > 0 && LL_SPI_IsActiveFlag_TXE(handle->bus->spi) && tx_allowed) {
            if(tx_buffer) {
                LL_SPI_TransmitData8(handle->bus->spi, *tx_buffer);
                tx_buffer++;
            } else {
                LL_SPI_TransmitData8(handle->bus->spi, 0xFF);
            }
            tx_size--;
            tx_allowed = false;
        }

        if(LL_SPI_IsActiveFlag_RXNE(handle->bus->spi)) {
            if(rx_buffer) {
                *rx_buffer = LL_SPI_ReceiveData8(handle->bus->spi);
                rx_buffer++;
            } else {
                LL_SPI_ReceiveData8(handle->bus->spi);
            }
            size--;
            tx_allowed = true;
        }
    }

    furry_hal_spi_bus_end_txrx(handle, timeout);

    return ret;
}

static void spi_dma_isr() {
#if SPI_DMA_RX_CHANNEL == LL_DMA_CHANNEL_3
    if(LL_DMA_IsActiveFlag_TC3(SPI_DMA) && LL_DMA_IsEnabledIT_TC(SPI_DMA_RX_DEF)) {
        LL_DMA_ClearFlag_TC3(SPI_DMA);
        furry_check(furry_semaphore_release(spi_dma_completed) == FurryStatusOk);
    }
#else
#error Update this code. Would you kindly?
#endif

#if SPI_DMA_TX_CHANNEL == LL_DMA_CHANNEL_4
    if(LL_DMA_IsActiveFlag_TC4(SPI_DMA) && LL_DMA_IsEnabledIT_TC(SPI_DMA_TX_DEF)) {
        LL_DMA_ClearFlag_TC4(SPI_DMA);
        furry_check(furry_semaphore_release(spi_dma_completed) == FurryStatusOk);
    }
#else
#error Update this code. Would you kindly?
#endif
}

bool furry_hal_spi_bus_trx_dma(
    FurryHalSpiBusHandle* handle,
    uint8_t* tx_buffer,
    uint8_t* rx_buffer,
    size_t size,
    uint32_t timeout_ms) {
    furry_assert(handle);
    furry_assert(handle->bus->current_handle == handle);
    furry_assert(size > 0);

    // If scheduler is not running, use blocking mode
    if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return furry_hal_spi_bus_trx(handle, tx_buffer, rx_buffer, size, timeout_ms);
    }

    // Lock DMA
    furry_check(furry_semaphore_acquire(spi_dma_lock, FurryWaitForever) == FurryStatusOk);

    const uint32_t dma_dummy_u32 = 0xFFFFFFFF;

    bool ret = true;
    SPI_TypeDef* spi = handle->bus->spi;
    uint32_t dma_rx_req;
    uint32_t dma_tx_req;

    if(spi == SPI1) {
        dma_rx_req = LL_DMAMUX_REQ_SPI1_RX;
        dma_tx_req = LL_DMAMUX_REQ_SPI1_TX;
    } else if(spi == SPI2) {
        dma_rx_req = LL_DMAMUX_REQ_SPI2_RX;
        dma_tx_req = LL_DMAMUX_REQ_SPI2_TX;
    } else {
        furry_crash(NULL);
    }

    if(rx_buffer == NULL) {
        // Only TX mode, do not use RX channel

        LL_DMA_InitTypeDef dma_config = {0};
        dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (spi->DR);
        dma_config.MemoryOrM2MDstAddress = (uint32_t)tx_buffer;
        dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
        dma_config.Mode = LL_DMA_MODE_NORMAL;
        dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
        dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
        dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
        dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
        dma_config.NbData = size;
        dma_config.PeriphRequest = dma_tx_req;
        dma_config.Priority = LL_DMA_PRIORITY_MEDIUM;
        LL_DMA_Init(SPI_DMA_TX_DEF, &dma_config);

#if SPI_DMA_TX_CHANNEL == LL_DMA_CHANNEL_4
        LL_DMA_ClearFlag_TC4(SPI_DMA);
#else
#error Update this code. Would you kindly?
#endif

        furry_hal_interrupt_set_isr(SPI_DMA_TX_IRQ, spi_dma_isr, NULL);

        bool dma_tx_was_enabled = LL_SPI_IsEnabledDMAReq_TX(spi);
        if(!dma_tx_was_enabled) {
            LL_SPI_EnableDMAReq_TX(spi);
        }

        // acquire semaphore before enabling DMA
        furry_check(furry_semaphore_acquire(spi_dma_completed, timeout_ms) == FurryStatusOk);

        LL_DMA_EnableIT_TC(SPI_DMA_TX_DEF);
        LL_DMA_EnableChannel(SPI_DMA_TX_DEF);

        // and wait for it to be released (DMA transfer complete)
        if(furry_semaphore_acquire(spi_dma_completed, timeout_ms) != FurryStatusOk) {
            ret = false;
            FURRY_LOG_E(TAG, "DMA timeout\r\n");
        }
        // release semaphore, because we are using it as a flag
        furry_semaphore_release(spi_dma_completed);

        LL_DMA_DisableIT_TC(SPI_DMA_TX_DEF);
        LL_DMA_DisableChannel(SPI_DMA_TX_DEF);
        if(!dma_tx_was_enabled) {
            LL_SPI_DisableDMAReq_TX(spi);
        }
        furry_hal_interrupt_set_isr(SPI_DMA_TX_IRQ, NULL, NULL);

        LL_DMA_DeInit(SPI_DMA_TX_DEF);
    } else {
        // TRX or RX mode, use both channels
        uint32_t tx_mem_increase_mode;

        if(tx_buffer == NULL) {
            // RX mode, use dummy data instead of TX buffer
            tx_buffer = (uint8_t*)&dma_dummy_u32;
            tx_mem_increase_mode = LL_DMA_PERIPH_NOINCREMENT;
        } else {
            tx_mem_increase_mode = LL_DMA_MEMORY_INCREMENT;
        }

        LL_DMA_InitTypeDef dma_config = {0};
        dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (spi->DR);
        dma_config.MemoryOrM2MDstAddress = (uint32_t)tx_buffer;
        dma_config.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
        dma_config.Mode = LL_DMA_MODE_NORMAL;
        dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
        dma_config.MemoryOrM2MDstIncMode = tx_mem_increase_mode;
        dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
        dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
        dma_config.NbData = size;
        dma_config.PeriphRequest = dma_tx_req;
        dma_config.Priority = LL_DMA_PRIORITY_MEDIUM;
        LL_DMA_Init(SPI_DMA_TX_DEF, &dma_config);

        dma_config.PeriphOrM2MSrcAddress = (uint32_t) & (spi->DR);
        dma_config.MemoryOrM2MDstAddress = (uint32_t)rx_buffer;
        dma_config.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
        dma_config.Mode = LL_DMA_MODE_NORMAL;
        dma_config.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
        dma_config.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
        dma_config.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
        dma_config.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
        dma_config.NbData = size;
        dma_config.PeriphRequest = dma_rx_req;
        dma_config.Priority = LL_DMA_PRIORITY_MEDIUM;
        LL_DMA_Init(SPI_DMA_RX_DEF, &dma_config);

#if SPI_DMA_RX_CHANNEL == LL_DMA_CHANNEL_3
        LL_DMA_ClearFlag_TC3(SPI_DMA);
#else
#error Update this code. Would you kindly?
#endif

        furry_hal_interrupt_set_isr(SPI_DMA_RX_IRQ, spi_dma_isr, NULL);

        bool dma_tx_was_enabled = LL_SPI_IsEnabledDMAReq_TX(spi);
        bool dma_rx_was_enabled = LL_SPI_IsEnabledDMAReq_RX(spi);

        if(!dma_tx_was_enabled) {
            LL_SPI_EnableDMAReq_TX(spi);
        }

        if(!dma_rx_was_enabled) {
            LL_SPI_EnableDMAReq_RX(spi);
        }

        // acquire semaphore before enabling DMA
        furry_check(furry_semaphore_acquire(spi_dma_completed, timeout_ms) == FurryStatusOk);

        LL_DMA_EnableIT_TC(SPI_DMA_RX_DEF);
        LL_DMA_EnableChannel(SPI_DMA_RX_DEF);
        LL_DMA_EnableChannel(SPI_DMA_TX_DEF);

        // and wait for it to be released (DMA transfer complete)
        if(furry_semaphore_acquire(spi_dma_completed, timeout_ms) != FurryStatusOk) {
            ret = false;
            FURRY_LOG_E(TAG, "DMA timeout\r\n");
        }
        // release semaphore, because we are using it as a flag
        furry_semaphore_release(spi_dma_completed);

        LL_DMA_DisableIT_TC(SPI_DMA_RX_DEF);

        LL_DMA_DisableChannel(SPI_DMA_TX_DEF);
        LL_DMA_DisableChannel(SPI_DMA_RX_DEF);

        if(!dma_tx_was_enabled) {
            LL_SPI_DisableDMAReq_TX(spi);
        }

        if(!dma_rx_was_enabled) {
            LL_SPI_DisableDMAReq_RX(spi);
        }

        furry_hal_interrupt_set_isr(SPI_DMA_RX_IRQ, NULL, NULL);

        LL_DMA_DeInit(SPI_DMA_TX_DEF);
        LL_DMA_DeInit(SPI_DMA_RX_DEF);
    }

    furry_hal_spi_bus_end_txrx(handle, timeout_ms);

    furry_check(furry_semaphore_release(spi_dma_lock) == FurryStatusOk);

    return ret;
}