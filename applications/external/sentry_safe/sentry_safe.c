#include <furry.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dolphin/dolphin.h>

#include <furry_hal.h>

typedef struct {
    uint8_t status;
    FurryMutex* mutex;
} SentryState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} Event;

const char* status_texts[3] = {"[Press OK to open safe]", "Sending...", "Done !"};

static void sentry_safe_render_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    const SentryState* sentry_state = ctx;
    furry_mutex_acquire(sentry_state->mutex, FurryWaitForever);

    // Before the function is called, the state is set with the canvas_reset(canvas)

    // Frame
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    // Message
    canvas_set_font(canvas, FontPrimary);

    canvas_draw_frame(canvas, 22, 4, 84, 24);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignBottom, "BLACK <-> GND");
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "GREEN <-> C1 ");
    canvas_draw_str_aligned(
        canvas, 64, 50, AlignCenter, AlignBottom, status_texts[sentry_state->status]);

    furry_mutex_release(sentry_state->mutex);
}

static void sentry_safe_input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    Event event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

void send_request(int command, int a, int b, int c, int d, int e) {
    int checksum = (command + a + b + c + d + e);

    furry_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeOutputPushPull);
    furry_hal_gpio_write(&gpio_ext_pc1, false);
    furry_delay_ms(3.4);
    furry_hal_gpio_write(&gpio_ext_pc1, true);

    furry_hal_uart_init(FurryHalUartIdLPUART1, 4800);
    //furry_hal_uart_set_br(FurryHalUartIdLPUART1, 4800);
    //furry_hal_uart_set_irq_cb(FurryHalUartIdLPUART1, usb_uart_on_irq_cb, usb_uart);

    uint8_t data[8] = {0x0, command, a, b, c, d, e, checksum};
    furry_hal_uart_tx(FurryHalUartIdLPUART1, data, 8);

    furry_delay_ms(100);

    furry_hal_uart_set_irq_cb(FurryHalUartIdLPUART1, NULL, NULL);
    furry_hal_uart_deinit(FurryHalUartIdLPUART1);
}

void reset_code(int a, int b, int c, int d, int e) {
    send_request(0x75, a, b, c, d, e);
}

void try_code(int a, int b, int c, int d, int e) {
    send_request(0x71, a, b, c, d, e);
}

int32_t sentry_safe_app(void* p) {
    UNUSED(p);

    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(Event));
    DOLPHIN_DEED(DolphinDeedPluginStart);

    SentryState* sentry_state = malloc(sizeof(SentryState));

    sentry_state->status = 0;

    sentry_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!sentry_state->mutex) {
        FURRY_LOG_E("SentrySafe", "cannot create mutex\r\n");
        furry_message_queue_free(event_queue);
        free(sentry_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, sentry_safe_render_callback, sentry_state);
    view_port_input_callback_set(view_port, sentry_safe_input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    Event event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 100);

        furry_mutex_acquire(sentry_state->mutex, FurryWaitForever);

        if(event_status == FurryStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        break;
                    case InputKeyDown:
                        break;
                    case InputKeyRight:
                        break;
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:

                        if(sentry_state->status == 2) {
                            sentry_state->status = 0;

                        } else if(sentry_state->status == 0) {
                            sentry_state->status = 1;

                            reset_code(1, 2, 3, 4, 5);
                            furry_delay_ms(500);
                            try_code(1, 2, 3, 4, 5);

                            sentry_state->status = 2;
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

        view_port_update(view_port);
        furry_mutex_release(sentry_state->mutex);
    }

    // Reset GPIO pins to default state
    furry_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(sentry_state->mutex);
    free(sentry_state);

    return 0;
}
