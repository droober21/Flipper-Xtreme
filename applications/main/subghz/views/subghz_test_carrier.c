#include "subghz_test_carrier.h"
#include "../subghz_i.h"
#include "../helpers/subghz_testing.h"

#include <math.h>
#include <furry.h>
#include <furry_hal.h>
#include <input/input.h>

struct SubGhzTestCarrier {
    View* view;
    FurryTimer* timer;
    SubGhzTestCarrierCallback callback;
    void* context;
};

typedef enum {
    SubGhzTestCarrierModelStatusRx,
    SubGhzTestCarrierModelStatusTx,
} SubGhzTestCarrierModelStatus;

typedef struct {
    uint8_t frequency;
    uint32_t real_frequency;
    FurryHalSubGhzPath path;
    float rssi;
    SubGhzTestCarrierModelStatus status;
} SubGhzTestCarrierModel;

void subghz_test_carrier_set_callback(
    SubGhzTestCarrier* subghz_test_carrier,
    SubGhzTestCarrierCallback callback,
    void* context) {
    furry_assert(subghz_test_carrier);
    furry_assert(callback);
    subghz_test_carrier->callback = callback;
    subghz_test_carrier->context = context;
}

void subghz_test_carrier_draw(Canvas* canvas, SubGhzTestCarrierModel* model) {
    char buffer[64];

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 8, "CC1101 Basic Test");

    canvas_set_font(canvas, FontSecondary);
    // Frequency
    snprintf(
        buffer,
        sizeof(buffer),
        "Freq: %03ld.%03ld.%03ld Hz",
        model->real_frequency / 1000000 % 1000,
        model->real_frequency / 1000 % 1000,
        model->real_frequency % 1000);
    canvas_draw_str(canvas, 0, 20, buffer);
    // Path
    char* path_name = "Unknown";
    if(model->path == FurryHalSubGhzPathIsolate) {
        path_name = "isolate";
    } else if(model->path == FurryHalSubGhzPath433) {
        path_name = "433MHz";
    } else if(model->path == FurryHalSubGhzPath315) {
        path_name = "315MHz";
    } else if(model->path == FurryHalSubGhzPath868) {
        path_name = "868MHz";
    }
    snprintf(buffer, sizeof(buffer), "Path: %d - %s", model->path, path_name);
    canvas_draw_str(canvas, 0, 31, buffer);
    if(model->status == SubGhzTestCarrierModelStatusRx) {
        snprintf(
            buffer,
            sizeof(buffer),
            "RSSI: %ld.%ld dBm",
            (int32_t)(model->rssi),
            (int32_t)fabs(model->rssi * 10) % 10);
        canvas_draw_str(canvas, 0, 42, buffer);
    } else {
        canvas_draw_str(canvas, 0, 42, "TX");
    }
}

bool subghz_test_carrier_input(InputEvent* event, void* context) {
    furry_assert(context);
    SubGhzTestCarrier* subghz_test_carrier = context;

    if(event->key == InputKeyBack || event->type != InputTypeShort) {
        return false;
    }

    with_view_model(
        subghz_test_carrier->view,
        SubGhzTestCarrierModel * model,
        {
            furry_hal_subghz_idle();

            if(event->key == InputKeyLeft) {
                if(model->frequency > 0) model->frequency--;
            } else if(event->key == InputKeyRight) {
                if(model->frequency < subghz_frequencies_count_testing - 1) model->frequency++;
            } else if(event->key == InputKeyDown) {
                if(model->path > 0) model->path--;
            } else if(event->key == InputKeyUp) {
                if(model->path < FurryHalSubGhzPath868) model->path++;
            } else if(event->key == InputKeyOk) {
                if(model->status == SubGhzTestCarrierModelStatusTx) {
                    model->status = SubGhzTestCarrierModelStatusRx;
                } else {
                    model->status = SubGhzTestCarrierModelStatusTx;
                }
            }

            model->real_frequency =
                furry_hal_subghz_set_frequency(subghz_frequencies_testing[model->frequency]);
            furry_hal_subghz_set_path(model->path);

            if(model->status == SubGhzTestCarrierModelStatusRx) {
                furry_hal_gpio_init(
                    furry_hal_subghz.cc1101_g0_pin, GpioModeInput, GpioPullNo, GpioSpeedLow);
                furry_hal_subghz_rx();
            } else {
                furry_hal_gpio_init(
                    furry_hal_subghz.cc1101_g0_pin,
                    GpioModeOutputPushPull,
                    GpioPullNo,
                    GpioSpeedLow);
                furry_hal_gpio_write(furry_hal_subghz.cc1101_g0_pin, true);
                if(!furry_hal_subghz_tx()) {
                    furry_hal_gpio_init(
                        furry_hal_subghz.cc1101_g0_pin, GpioModeInput, GpioPullNo, GpioSpeedLow);
                    subghz_test_carrier->callback(
                        SubGhzTestCarrierEventOnlyRx, subghz_test_carrier->context);
                }
            }
        },
        true);

    return true;
}

void subghz_test_carrier_enter(void* context) {
    furry_assert(context);
    SubGhzTestCarrier* subghz_test_carrier = context;

    furry_hal_subghz_reset();
    furry_hal_subghz_load_preset(FurryHalSubGhzPresetOok650Async);

    furry_hal_gpio_init(furry_hal_subghz.cc1101_g0_pin, GpioModeInput, GpioPullNo, GpioSpeedLow);

    with_view_model(
        subghz_test_carrier->view,
        SubGhzTestCarrierModel * model,
        {
            model->frequency = subghz_frequencies_433_92_testing; // 433
            model->real_frequency =
                furry_hal_subghz_set_frequency(subghz_frequencies_testing[model->frequency]);
            model->path = FurryHalSubGhzPathIsolate; // isolate
            model->rssi = 0.0f;
            model->status = SubGhzTestCarrierModelStatusRx;
        },
        true);

    furry_hal_subghz_rx();

    furry_timer_start(subghz_test_carrier->timer, furry_kernel_get_tick_frequency() / 4);
}

void subghz_test_carrier_exit(void* context) {
    furry_assert(context);
    SubGhzTestCarrier* subghz_test_carrier = context;

    furry_timer_stop(subghz_test_carrier->timer);

    // Reinitialize IC to default state
    furry_hal_subghz_sleep();
}

void subghz_test_carrier_rssi_timer_callback(void* context) {
    furry_assert(context);
    SubGhzTestCarrier* subghz_test_carrier = context;

    with_view_model(
        subghz_test_carrier->view,
        SubGhzTestCarrierModel * model,
        {
            if(model->status == SubGhzTestCarrierModelStatusRx) {
                model->rssi = furry_hal_subghz_get_rssi();
            }
        },
        false);
}

SubGhzTestCarrier* subghz_test_carrier_alloc() {
    SubGhzTestCarrier* subghz_test_carrier = malloc(sizeof(SubGhzTestCarrier));

    // View allocation and configuration
    subghz_test_carrier->view = view_alloc();
    view_allocate_model(
        subghz_test_carrier->view, ViewModelTypeLocking, sizeof(SubGhzTestCarrierModel));
    view_set_context(subghz_test_carrier->view, subghz_test_carrier);
    view_set_draw_callback(subghz_test_carrier->view, (ViewDrawCallback)subghz_test_carrier_draw);
    view_set_input_callback(subghz_test_carrier->view, subghz_test_carrier_input);
    view_set_enter_callback(subghz_test_carrier->view, subghz_test_carrier_enter);
    view_set_exit_callback(subghz_test_carrier->view, subghz_test_carrier_exit);

    subghz_test_carrier->timer = furry_timer_alloc(
        subghz_test_carrier_rssi_timer_callback, FurryTimerTypePeriodic, subghz_test_carrier);

    return subghz_test_carrier;
}

void subghz_test_carrier_free(SubGhzTestCarrier* subghz_test_carrier) {
    furry_assert(subghz_test_carrier);
    furry_timer_free(subghz_test_carrier->timer);
    view_free(subghz_test_carrier->view);
    free(subghz_test_carrier);
}

View* subghz_test_carrier_get_view(SubGhzTestCarrier* subghz_test_carrier) {
    furry_assert(subghz_test_carrier);
    return subghz_test_carrier->view;
}
