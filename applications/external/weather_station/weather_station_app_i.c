#include "weather_station_app_i.h"

#define TAG "WeatherStation"
#include <flipper_format/flipper_format_i.h>

void ws_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furry_assert(context);
    WeatherStationApp* app = context;
    furry_string_set(app->txrx->preset->name, preset_name);
    app->txrx->preset->frequency = frequency;
    app->txrx->preset->data = preset_data;
    app->txrx->preset->data_size = preset_data_size;
}

bool ws_set_preset(WeatherStationApp* app, const char* preset) {
    if(!strcmp(preset, "FurryHalSubGhzPresetOok270Async")) {
        furry_string_set(app->txrx->preset->name, "AM270");
    } else if(!strcmp(preset, "FurryHalSubGhzPresetOok650Async")) {
        furry_string_set(app->txrx->preset->name, "AM650");
    } else if(!strcmp(preset, "FurryHalSubGhzPreset2FSKDev238Async")) {
        furry_string_set(app->txrx->preset->name, "FM238");
    } else if(!strcmp(preset, "FurryHalSubGhzPreset2FSKDev476Async")) {
        furry_string_set(app->txrx->preset->name, "FM476");
    } else if(!strcmp(preset, "FurryHalSubGhzPresetCustom")) {
        furry_string_set(app->txrx->preset->name, "CUSTOM");
    } else {
        FURRY_LOG_E(TAG, "Unknown preset");
        return false;
    }
    return true;
}

void ws_get_frequency_modulation(
    WeatherStationApp* app,
    FurryString* frequency,
    FurryString* modulation) {
    furry_assert(app);
    if(frequency != NULL) {
        furry_string_printf(
            frequency,
            "%03ld.%02ld",
            app->txrx->preset->frequency / 1000000 % 1000,
            app->txrx->preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        furry_string_printf(modulation, "%.2s", furry_string_get_cstr(app->txrx->preset->name));
    }
}

void ws_begin(WeatherStationApp* app, uint8_t* preset_data) {
    furry_assert(app);
    UNUSED(preset_data);
    furry_hal_subghz_reset();
    furry_hal_subghz_idle();
    furry_hal_subghz_load_custom_preset(preset_data);
    furry_hal_gpio_init(furry_hal_subghz.cc1101_g0_pin, GpioModeInput, GpioPullNo, GpioSpeedLow);
    app->txrx->txrx_state = WSTxRxStateIDLE;
}

uint32_t ws_rx(WeatherStationApp* app, uint32_t frequency) {
    furry_assert(app);
    if(!furry_hal_subghz_is_frequency_valid(frequency)) {
        furry_crash("WeatherStation: Incorrect RX frequency.");
    }
    furry_assert(
        app->txrx->txrx_state != WSTxRxStateRx && app->txrx->txrx_state != WSTxRxStateSleep);

    furry_hal_subghz_idle();
    uint32_t value = furry_hal_subghz_set_frequency_and_path(frequency);
    furry_hal_gpio_init(furry_hal_subghz.cc1101_g0_pin, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furry_hal_subghz_flush_rx();
    furry_hal_subghz_rx();

    furry_hal_subghz_start_async_rx(subghz_worker_rx_callback, app->txrx->worker);
    subghz_worker_start(app->txrx->worker);
    app->txrx->txrx_state = WSTxRxStateRx;
    return value;
}

void ws_idle(WeatherStationApp* app) {
    furry_assert(app);
    furry_assert(app->txrx->txrx_state != WSTxRxStateSleep);
    furry_hal_subghz_idle();
    app->txrx->txrx_state = WSTxRxStateIDLE;
}

void ws_rx_end(WeatherStationApp* app) {
    furry_assert(app);
    furry_assert(app->txrx->txrx_state == WSTxRxStateRx);
    if(subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
        furry_hal_subghz_stop_async_rx();
    }
    furry_hal_subghz_idle();
    app->txrx->txrx_state = WSTxRxStateIDLE;
}

void ws_sleep(WeatherStationApp* app) {
    furry_assert(app);
    furry_hal_subghz_sleep();
    app->txrx->txrx_state = WSTxRxStateSleep;
}

void ws_hopper_update(WeatherStationApp* app) {
    furry_assert(app);

    switch(app->txrx->hopper_state) {
    case WSHopperStateOFF:
    case WSHopperStatePause:
        return;
    case WSHopperStateRSSITimeOut:
        if(app->txrx->hopper_timeout != 0) {
            app->txrx->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    float rssi = -127.0f;
    if(app->txrx->hopper_state != WSHopperStateRSSITimeOut) {
        // See RSSI Calculation timings in CC1101 17.3 RSSI
        rssi = furry_hal_subghz_get_rssi();

        // Stay if RSSI is high enough
        if(rssi > -90.0f) {
            app->txrx->hopper_timeout = 10;
            app->txrx->hopper_state = WSHopperStateRSSITimeOut;
            return;
        }
    } else {
        app->txrx->hopper_state = WSHopperStateRunnig;
    }
    // Select next frequency
    if(app->txrx->hopper_idx_frequency <
       subghz_setting_get_hopper_frequency_count(app->setting) - 1) {
        app->txrx->hopper_idx_frequency++;
    } else {
        app->txrx->hopper_idx_frequency = 0;
    }

    if(app->txrx->txrx_state == WSTxRxStateRx) {
        ws_rx_end(app);
    };
    if(app->txrx->txrx_state == WSTxRxStateIDLE) {
        subghz_receiver_reset(app->txrx->receiver);
        app->txrx->preset->frequency =
            subghz_setting_get_hopper_frequency(app->setting, app->txrx->hopper_idx_frequency);
        ws_rx(app, app->txrx->preset->frequency);
    }
}
