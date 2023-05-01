// insired by
// https://github.com/esphome/esphome/blob/ac0d921413c3884752193fe568fa82853f0f99e9/esphome/components/ultrasonic/ultrasonic_sensor.cpp
// Ported and modified by @xMasterX

#include <furry.h>
#include <furry_hal.h>
#include <furry_hal_power.h>
#include <furry_hal_console.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <gui/elements.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    FurryMutex* mutex;
    NotificationApp* notification;
    bool have_5v;
    bool measurement_made;
    uint32_t echo; // us
    float distance; // meters
} PluginState;

const NotificationSequence sequence_done = {
    &message_display_backlight_on,
    &message_green_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static void render_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    const PluginState* plugin_state = ctx;
    furry_mutex_acquire(plugin_state->mutex, FurryWaitForever);

    // border around the edge of the screen
    // canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(
        canvas, 64, 2, AlignCenter, AlignTop, "HC-SR04 Ultrasonic\nDistance Sensor");

    canvas_set_font(canvas, FontSecondary);

    if(!plugin_state->have_5v) {
        elements_multiline_text_aligned(
            canvas,
            4,
            28,
            AlignLeft,
            AlignTop,
            "5V on GPIO must be\nenabled, or USB must\nbe connected.");
    } else {
        if(!plugin_state->measurement_made) {
            elements_multiline_text_aligned(
                canvas, 64, 28, AlignCenter, AlignTop, "Press OK button to measure");
            elements_multiline_text_aligned(
                canvas, 64, 40, AlignCenter, AlignTop, "13/TX -> Trig\n14/RX -> Echo");
        } else {
            elements_multiline_text_aligned(canvas, 4, 28, AlignLeft, AlignTop, "Readout:");

            FurryString* str_buf;
            str_buf = furry_string_alloc();
            furry_string_printf(str_buf, "Echo: %ld us", plugin_state->echo);

            canvas_draw_str_aligned(
                canvas, 8, 38, AlignLeft, AlignTop, furry_string_get_cstr(str_buf));
            furry_string_printf(str_buf, "Distance: %02f m", (double)plugin_state->distance);
            canvas_draw_str_aligned(
                canvas, 8, 48, AlignLeft, AlignTop, furry_string_get_cstr(str_buf));

            furry_string_free(str_buf);
        }
    }

    furry_mutex_release(plugin_state->mutex);
}

static void input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

static void hc_sr04_state_init(PluginState* const plugin_state) {
    plugin_state->echo = -1;
    plugin_state->distance = -1;
    plugin_state->measurement_made = false;

    furry_hal_power_suppress_charge_enter();

    plugin_state->have_5v = false;
    if(furry_hal_power_is_otg_enabled() || furry_hal_power_is_charging()) {
        plugin_state->have_5v = true;
    } else {
        furry_hal_power_enable_otg();
        plugin_state->have_5v = true;
    }
}

float hc_sr04_us_to_m(uint32_t us) {
    //speed of sound for 20°C, 50% relative humidity
    //331.3 + 20 * 0.606 + 50 * 0.0124 = 0.034404
    const float speed_sound_m_per_s = 344.04f;
    const float time_s = us / 1e6f;
    const float total_dist = time_s * speed_sound_m_per_s;
    return total_dist / 2.0f;
}

static void hc_sr04_measure(PluginState* const plugin_state) {
    //plugin_state->echo = 1;
    //return;

    if(!plugin_state->have_5v) {
        if(furry_hal_power_is_otg_enabled() || furry_hal_power_is_charging()) {
            plugin_state->have_5v = true;
        } else {
            return;
        }
    }

    //furry_hal_light_set(LightRed, 0xFF);
    notification_message(plugin_state->notification, &sequence_blink_start_yellow);

    const uint32_t timeout_ms = 2000;
    // Pin 13 / TX -> Trig
    furry_hal_gpio_write(&gpio_usart_tx, false);
    furry_hal_gpio_init(&gpio_usart_tx, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);

    // Pin 14 / RX -> Echo
    furry_hal_gpio_write(&gpio_usart_rx, false);
    furry_hal_gpio_init(&gpio_usart_rx, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);

    //FURRY_CRITICAL_ENTER();
    // 10 ms pulse on TX
    furry_hal_gpio_write(&gpio_usart_tx, true);
    furry_delay_ms(10);
    furry_hal_gpio_write(&gpio_usart_tx, false);

    const uint32_t start = furry_get_tick();

    while(furry_get_tick() - start < timeout_ms && furry_hal_gpio_read(&gpio_usart_rx))
        ;
    while(furry_get_tick() - start < timeout_ms && !furry_hal_gpio_read(&gpio_usart_rx))
        ;

    const uint32_t pulse_start = DWT->CYCCNT;

    while(furry_get_tick() - start < timeout_ms && furry_hal_gpio_read(&gpio_usart_rx))
        ;
    const uint32_t pulse_end = DWT->CYCCNT;

    //FURRY_CRITICAL_EXIT();

    plugin_state->echo =
        (pulse_end - pulse_start) / furry_hal_cortex_instructions_per_microsecond();
    plugin_state->distance = hc_sr04_us_to_m(plugin_state->echo);
    plugin_state->measurement_made = true;

    //furry_hal_light_set(LightRed, 0x00);
    notification_message(plugin_state->notification, &sequence_blink_stop);
    notification_message(plugin_state->notification, &sequence_done);
}

int32_t hc_sr04_app() {
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(PluginEvent));

    PluginState* plugin_state = malloc(sizeof(PluginState));

    hc_sr04_state_init(plugin_state);

    furry_hal_console_disable();

    plugin_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!plugin_state->mutex) {
        FURRY_LOG_E("hc_sr04", "cannot create mutex\r\n");
        if(furry_hal_power_is_otg_enabled()) {
            furry_hal_power_disable_otg();
        }
        furry_hal_console_enable();
        furry_hal_power_suppress_charge_exit();
        furry_message_queue_free(event_queue);
        free(plugin_state);
        return 255;
    }

    plugin_state->notification = furry_record_open(RECORD_NOTIFICATION);

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, plugin_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    PluginEvent event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 100);

        furry_mutex_acquire(plugin_state->mutex, FurryWaitForever);

        if(event_status == FurryStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                    case InputKeyDown:
                    case InputKeyRight:
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:
                        hc_sr04_measure(plugin_state);
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        view_port_update(view_port);
        furry_mutex_release(plugin_state->mutex);
    }

    if(furry_hal_power_is_otg_enabled()) {
        furry_hal_power_disable_otg();
    }
    furry_hal_power_suppress_charge_exit();

    // Return TX / RX back to usart mode
    furry_hal_gpio_init_ex(
        &gpio_usart_tx,
        GpioModeAltFunctionPushPull,
        GpioPullUp,
        GpioSpeedVeryHigh,
        GpioAltFn7USART1);
    furry_hal_gpio_init_ex(
        &gpio_usart_rx,
        GpioModeAltFunctionPushPull,
        GpioPullUp,
        GpioSpeedVeryHigh,
        GpioAltFn7USART1);
    furry_hal_console_enable();

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    furry_record_close(RECORD_NOTIFICATION);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(plugin_state->mutex);
    free(plugin_state);

    return 0;
}
