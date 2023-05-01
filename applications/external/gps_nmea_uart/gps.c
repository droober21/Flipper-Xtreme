#include "gps_uart.h"

#include <furry.h>
#include <gui/gui.h>
#include <string.h>

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

static void render_callback(Canvas* const canvas, void* context) {
    furry_assert(context);
    GpsUart* gps_uart = context;
    furry_mutex_acquire(gps_uart->mutex, FurryWaitForever);

    if(!gps_uart->changing_baudrate) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 8, AlignCenter, AlignBottom, "Latitude");
        canvas_draw_str_aligned(canvas, 96, 8, AlignCenter, AlignBottom, "Longitude");
        canvas_draw_str_aligned(canvas, 21, 30, AlignCenter, AlignBottom, "Course");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "Speed");
        canvas_draw_str_aligned(canvas, 107, 30, AlignCenter, AlignBottom, "Altitude");
        canvas_draw_str_aligned(canvas, 32, 52, AlignCenter, AlignBottom, "Satellites");
        canvas_draw_str_aligned(canvas, 96, 52, AlignCenter, AlignBottom, "Last Fix");

        canvas_set_font(canvas, FontSecondary);
        char buffer[64];
        snprintf(buffer, 64, "%f", (double)gps_uart->status.latitude);
        canvas_draw_str_aligned(canvas, 32, 18, AlignCenter, AlignBottom, buffer);
        snprintf(buffer, 64, "%f", (double)gps_uart->status.longitude);
        canvas_draw_str_aligned(canvas, 96, 18, AlignCenter, AlignBottom, buffer);
        snprintf(buffer, 64, "%.1f", (double)gps_uart->status.course);
        canvas_draw_str_aligned(canvas, 21, 40, AlignCenter, AlignBottom, buffer);
        if(!gps_uart->speed_in_kms) {
            snprintf(buffer, 64, "%.2f kn", (double)gps_uart->status.speed);
        } else {
            snprintf(buffer, 64, "%.2f km", (double)(gps_uart->status.speed * 1.852));
        }
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, buffer);
        snprintf(
            buffer,
            64,
            "%.1f %c",
            (double)gps_uart->status.altitude,
            tolower(gps_uart->status.altitude_units));
        canvas_draw_str_aligned(canvas, 107, 40, AlignCenter, AlignBottom, buffer);
        snprintf(buffer, 64, "%d", gps_uart->status.satellites_tracked);
        canvas_draw_str_aligned(canvas, 32, 62, AlignCenter, AlignBottom, buffer);
        snprintf(
            buffer,
            64,
            "%02d:%02d:%02d UTC",
            gps_uart->status.time_hours,
            gps_uart->status.time_minutes,
            gps_uart->status.time_seconds);
        canvas_draw_str_aligned(canvas, 96, 62, AlignCenter, AlignBottom, buffer);
    } else {
        char buffer[64];
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, "Baudrate set to:");

        snprintf(buffer, 64, "%ld baud", gps_uart->baudrate);
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignBottom, buffer);
    }

    furry_mutex_release(gps_uart->mutex);
}

static void input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

int32_t gps_app(void* p) {
    UNUSED(p);

    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(PluginEvent));

    GpsUart* gps_uart = gps_uart_enable();

    gps_uart->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!gps_uart->mutex) {
        FURRY_LOG_E("GPS", "cannot create mutex\r\n");
        free(gps_uart);
        return 255;
    }

    // set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, gps_uart);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    PluginEvent event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 100);

        furry_mutex_acquire(gps_uart->mutex, FurryWaitForever);

        if(event_status == FurryStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeShort) {
                    switch(event.input.key) {
                    case InputKeyUp:
                    case InputKeyDown:
                    case InputKeyRight:
                    case InputKeyLeft:
                    case InputKeyBack:
                        break;
                    case InputKeyOk:
                        if(!gps_uart->backlight_on) {
                            notification_message_block(
                                gps_uart->notifications, &sequence_display_backlight_enforce_on);
                            gps_uart->backlight_on = true;
                        } else {
                            notification_message_block(
                                gps_uart->notifications, &sequence_display_backlight_enforce_auto);
                            notification_message(
                                gps_uart->notifications, &sequence_display_backlight_off);
                            gps_uart->backlight_on = false;
                        }
                        break;
                    default:
                        break;
                    }
                } else if(event.input.type == InputTypeLong) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        gps_uart_deinit_thread(gps_uart);
                        const int baudrate_length =
                            sizeof(gps_baudrates) / sizeof(gps_baudrates[0]);
                        current_gps_baudrate++;
                        if(current_gps_baudrate >= baudrate_length) {
                            current_gps_baudrate = 0;
                        }
                        gps_uart->baudrate = gps_baudrates[current_gps_baudrate];

                        gps_uart_init_thread(gps_uart);
                        gps_uart->changing_baudrate = true;
                        view_port_update(view_port);
                        furry_mutex_release(gps_uart->mutex);
                        break;
                    case InputKeyRight:
                        if(gps_uart->speed_in_kms) {
                            gps_uart->speed_in_kms = false;
                        } else {
                            gps_uart->speed_in_kms = true;
                        }
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
        if(!gps_uart->changing_baudrate) {
            view_port_update(view_port);
            furry_mutex_release(gps_uart->mutex);
        } else {
            furry_delay_ms(1000);
            gps_uart->changing_baudrate = false;
        }
    }

    notification_message_block(gps_uart->notifications, &sequence_display_backlight_enforce_auto);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(gps_uart->mutex);
    gps_uart_disable(gps_uart);

    return 0;
}
