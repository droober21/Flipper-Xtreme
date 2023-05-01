// by @xMasterX

#include <furry.h>
#include <furry_hal_power.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <gui/elements.h>

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
    bool is_on;
} PluginState;

static void render_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    const PluginState* plugin_state = ctx;
    furry_mutex_acquire(plugin_state->mutex, FurryWaitForever);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Flashlight");

    canvas_set_font(canvas, FontSecondary);

    if(!plugin_state->is_on) {
        elements_multiline_text_aligned(
            canvas, 64, 28, AlignCenter, AlignTop, "Press OK button turn on");
    } else {
        elements_multiline_text_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Light is on!");
        elements_multiline_text_aligned(
            canvas, 64, 40, AlignCenter, AlignTop, "Press OK button to off");
    }

    furry_mutex_release(plugin_state->mutex);
}

static void input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

static void flash_toggle(PluginState* const plugin_state) {
    furry_hal_gpio_write(&gpio_ext_pc3, false);
    furry_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);

    if(plugin_state->is_on) {
        furry_hal_gpio_write(&gpio_ext_pc3, false);
        plugin_state->is_on = false;
    } else {
        furry_hal_gpio_write(&gpio_ext_pc3, true);
        plugin_state->is_on = true;
    }
}

int32_t flashlight_app() {
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(PluginEvent));

    PluginState* plugin_state = malloc(sizeof(PluginState));

    plugin_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!plugin_state->mutex) {
        FURRY_LOG_E("flashlight", "cannot create mutex\r\n");
        furry_message_queue_free(event_queue);
        free(plugin_state);
        return 255;
    }

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
                        flash_toggle(plugin_state);
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

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(plugin_state->mutex);

    return 0;
}
