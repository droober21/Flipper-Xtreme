#include <furry_hal_spi_config.h>
#include <furry_hal_resources.h>
#include <furry_hal_spi.h>
#include <furry.h>
#include <furry_hal_subghz.h>

#define TAG "FurryHalSpiConfig"

/* SPI Presets */

const LL_SPI_InitTypeDef furry_hal_spi_preset_2edge_low_8m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_2EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_8m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_4m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_16m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_2m = {
    .Mode = LL_SPI_MODE_MASTER,
    .TransferDirection = LL_SPI_FULL_DUPLEX,
    .DataWidth = LL_SPI_DATAWIDTH_8BIT,
    .ClockPolarity = LL_SPI_POLARITY_LOW,
    .ClockPhase = LL_SPI_PHASE_1EDGE,
    .NSS = LL_SPI_NSS_SOFT,
    .BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV32,
    .BitOrder = LL_SPI_MSB_FIRST,
    .CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE,
    .CRCPoly = 7,
};

/* SPI Buses */

FurryMutex* furry_hal_spi_bus_r_mutex = NULL;

void furry_hal_spi_config_init_early() {
    furry_hal_spi_bus_init(&furry_hal_spi_bus_d);
    furry_hal_spi_bus_handle_init(&furry_hal_spi_bus_handle_display);
}

void furry_hal_spi_config_deinit_early() {
    furry_hal_spi_bus_handle_deinit(&furry_hal_spi_bus_handle_display);
    furry_hal_spi_bus_deinit(&furry_hal_spi_bus_d);
}

void furry_hal_spi_config_init() {
    furry_hal_spi_bus_init(&furry_hal_spi_bus_r);

    furry_hal_spi_bus_handle_init(furry_hal_subghz.spi_bus_handle);
    furry_hal_spi_bus_handle_init(&furry_hal_spi_bus_handle_nfc);
    furry_hal_spi_bus_handle_init(&furry_hal_spi_bus_handle_sd_fast);
    furry_hal_spi_bus_handle_init(&furry_hal_spi_bus_handle_sd_slow);

    FURRY_LOG_I(TAG, "Init OK");
}

static void furry_hal_spi_bus_r_event_callback(FurryHalSpiBus* bus, FurryHalSpiBusEvent event) {
    if(event == FurryHalSpiBusEventInit) {
        furry_hal_spi_bus_r_mutex = furry_mutex_alloc(FurryMutexTypeNormal);
        FURRY_CRITICAL_ENTER();
        LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_SPI1);
        FURRY_CRITICAL_EXIT();
        bus->current_handle = NULL;
    } else if(event == FurryHalSpiBusEventDeinit) {
        furry_mutex_free(furry_hal_spi_bus_r_mutex);
        FURRY_CRITICAL_ENTER();
        LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_SPI1);
        LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_SPI1);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalSpiBusEventLock) {
        furry_check(furry_mutex_acquire(furry_hal_spi_bus_r_mutex, FurryWaitForever) == FurryStatusOk);
    } else if(event == FurryHalSpiBusEventUnlock) {
        furry_check(furry_mutex_release(furry_hal_spi_bus_r_mutex) == FurryStatusOk);
    } else if(event == FurryHalSpiBusEventActivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_SPI1);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalSpiBusEventDeactivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_SPI1);
        FURRY_CRITICAL_EXIT();
    }
}

FurryHalSpiBus furry_hal_spi_bus_r = {
    .spi = SPI1,
    .callback = furry_hal_spi_bus_r_event_callback,
};

FurryMutex* furry_hal_spi_bus_d_mutex = NULL;

static void furry_hal_spi_bus_d_event_callback(FurryHalSpiBus* bus, FurryHalSpiBusEvent event) {
    if(event == FurryHalSpiBusEventInit) {
        furry_hal_spi_bus_d_mutex = furry_mutex_alloc(FurryMutexTypeNormal);
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_SPI2);
        FURRY_CRITICAL_EXIT();
        bus->current_handle = NULL;
    } else if(event == FurryHalSpiBusEventDeinit) {
        furry_mutex_free(furry_hal_spi_bus_d_mutex);
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_SPI2);
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_SPI2);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalSpiBusEventLock) {
        furry_check(furry_mutex_acquire(furry_hal_spi_bus_d_mutex, FurryWaitForever) == FurryStatusOk);
    } else if(event == FurryHalSpiBusEventUnlock) {
        furry_check(furry_mutex_release(furry_hal_spi_bus_d_mutex) == FurryStatusOk);
    } else if(event == FurryHalSpiBusEventActivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_SPI2);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalSpiBusEventDeactivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_SPI2);
        FURRY_CRITICAL_EXIT();
    }
}

FurryHalSpiBus furry_hal_spi_bus_d = {
    .spi = SPI2,
    .callback = furry_hal_spi_bus_d_event_callback,
};

/* SPI Bus Handles */

inline static void furry_hal_spi_bus_r_handle_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event,
    const LL_SPI_InitTypeDef* preset) {
    if(event == FurryHalSpiBusHandleEventInit) {
        furry_hal_gpio_write(handle->cs, true);
        furry_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    } else if(event == FurryHalSpiBusHandleEventDeinit) {
        furry_hal_gpio_write(handle->cs, true);
        furry_hal_gpio_init(handle->cs, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    } else if(event == FurryHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)preset);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);

        furry_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);

        furry_hal_gpio_write(handle->cs, false);
    } else if(event == FurryHalSpiBusHandleEventDeactivate) {
        furry_hal_gpio_write(handle->cs, true);

        furry_hal_gpio_init(handle->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furry_hal_gpio_init(handle->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furry_hal_gpio_init(handle->sck, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        LL_SPI_Disable(handle->bus->spi);
    }
}

inline static void furry_hal_spi_bus_nfc_handle_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event,
    const LL_SPI_InitTypeDef* preset) {
    if(event == FurryHalSpiBusHandleEventInit) {
        // Configure GPIOs in normal SPI mode
        furry_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_write(handle->cs, true);
        furry_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    } else if(event == FurryHalSpiBusHandleEventDeinit) {
        // Configure GPIOs for st25r3916 Transparent mode
        furry_hal_gpio_init(handle->sck, GpioModeInput, GpioPullUp, GpioSpeedLow);
        furry_hal_gpio_init(handle->miso, GpioModeInput, GpioPullUp, GpioSpeedLow);
        furry_hal_gpio_init(handle->cs, GpioModeInput, GpioPullUp, GpioSpeedLow);
        furry_hal_gpio_write(handle->mosi, false);
        furry_hal_gpio_init(handle->mosi, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    } else if(event == FurryHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)preset);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);

        furry_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furry_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);

    } else if(event == FurryHalSpiBusHandleEventDeactivate) {
        furry_hal_gpio_init(handle->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furry_hal_gpio_init(handle->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furry_hal_gpio_init(handle->sck, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        LL_SPI_Disable(handle->bus->spi);
    }
}

static void furry_hal_spi_bus_handle_subghz_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event) {
    furry_hal_spi_bus_r_handle_event_callback(handle, event, &furry_hal_spi_preset_1edge_low_8m);
}

FurryHalSpiBusHandle furry_hal_spi_bus_handle_subghz_int = {
    .bus = &furry_hal_spi_bus_r,
    .callback = furry_hal_spi_bus_handle_subghz_event_callback,
    .miso = &gpio_spi_r_miso,
    .mosi = &gpio_spi_r_mosi,
    .sck = &gpio_spi_r_sck,
    .cs = &gpio_subghz_cs,
};

FurryHalSpiBusHandle furry_hal_spi_bus_handle_subghz = {
    .bus = &furry_hal_spi_bus_r,
    .callback = furry_hal_spi_bus_handle_subghz_event_callback,
    .miso = &gpio_spi_r_miso,
    .mosi = &gpio_spi_r_mosi,
    .sck = &gpio_spi_r_sck,
    .cs = &gpio_subghz_cs,
};

FurryHalSpiBusHandle furry_hal_spi_bus_handle_subghz_ext = {
    .bus = &furry_hal_spi_bus_r,
    .callback = furry_hal_spi_bus_handle_subghz_event_callback,
    .miso = &gpio_ext_pa6,
    .mosi = &gpio_ext_pa7,
    .sck = &gpio_ext_pb3,
    .cs = &gpio_ext_pa4,
};

static void furry_hal_spi_bus_handle_nfc_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event) {
    furry_hal_spi_bus_nfc_handle_event_callback(handle, event, &furry_hal_spi_preset_2edge_low_8m);
}

FurryHalSpiBusHandle furry_hal_spi_bus_handle_nfc = {
    .bus = &furry_hal_spi_bus_r,
    .callback = furry_hal_spi_bus_handle_nfc_event_callback,
    .miso = &gpio_spi_r_miso,
    .mosi = &gpio_spi_r_mosi,
    .sck = &gpio_spi_r_sck,
    .cs = &gpio_nfc_cs,
};

static void furry_hal_spi_bus_handle_external_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event) {
    furry_hal_spi_bus_r_handle_event_callback(handle, event, &furry_hal_spi_preset_1edge_low_2m);
}

FurryHalSpiBusHandle furry_hal_spi_bus_handle_external = {
    .bus = &furry_hal_spi_bus_r,
    .callback = furry_hal_spi_bus_handle_external_event_callback,
    .miso = &gpio_ext_pa6,
    .mosi = &gpio_ext_pa7,
    .sck = &gpio_ext_pb3,
    .cs = &gpio_ext_pa4,
};

inline static void furry_hal_spi_bus_d_handle_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event,
    const LL_SPI_InitTypeDef* preset) {
    if(event == FurryHalSpiBusHandleEventInit) {
        furry_hal_gpio_write(handle->cs, true);
        furry_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);

        furry_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI2);
        furry_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI2);
        furry_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI2);

    } else if(event == FurryHalSpiBusHandleEventDeinit) {
        furry_hal_gpio_write(handle->cs, true);
        furry_hal_gpio_init(handle->cs, GpioModeAnalog, GpioPullUp, GpioSpeedLow);
    } else if(event == FurryHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)preset);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);
        furry_hal_gpio_write(handle->cs, false);
    } else if(event == FurryHalSpiBusHandleEventDeactivate) {
        furry_hal_gpio_write(handle->cs, true);
        LL_SPI_Disable(handle->bus->spi);
    }
}

static void furry_hal_spi_bus_handle_display_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event) {
    furry_hal_spi_bus_d_handle_event_callback(handle, event, &furry_hal_spi_preset_1edge_low_4m);
}

FurryHalSpiBusHandle furry_hal_spi_bus_handle_display = {
    .bus = &furry_hal_spi_bus_d,
    .callback = furry_hal_spi_bus_handle_display_event_callback,
    .miso = &gpio_spi_d_miso,
    .mosi = &gpio_spi_d_mosi,
    .sck = &gpio_spi_d_sck,
    .cs = &gpio_display_cs,
};

static void furry_hal_spi_bus_handle_sd_fast_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event) {
    furry_hal_spi_bus_d_handle_event_callback(handle, event, &furry_hal_spi_preset_1edge_low_16m);
}

FurryHalSpiBusHandle furry_hal_spi_bus_handle_sd_fast = {
    .bus = &furry_hal_spi_bus_d,
    .callback = furry_hal_spi_bus_handle_sd_fast_event_callback,
    .miso = &gpio_spi_d_miso,
    .mosi = &gpio_spi_d_mosi,
    .sck = &gpio_spi_d_sck,
    .cs = &gpio_sdcard_cs,
};

static void furry_hal_spi_bus_handle_sd_slow_event_callback(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event) {
    furry_hal_spi_bus_d_handle_event_callback(handle, event, &furry_hal_spi_preset_1edge_low_2m);
}

FurryHalSpiBusHandle furry_hal_spi_bus_handle_sd_slow = {
    .bus = &furry_hal_spi_bus_d,
    .callback = furry_hal_spi_bus_handle_sd_slow_event_callback,
    .miso = &gpio_spi_d_miso,
    .mosi = &gpio_spi_d_mosi,
    .sck = &gpio_spi_d_sck,
    .cs = &gpio_sdcard_cs,
};
