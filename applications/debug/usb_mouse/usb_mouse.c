#include <furry.h>
#include <furry_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#define MOUSE_MOVE_SHORT 5
#define MOUSE_MOVE_LONG 20

typedef enum {
    EventTypeInput,
} EventType;

typedef struct {
    union {
        InputEvent input;
    };
    EventType type;
} UsbMouseEvent;

static void usb_mouse_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "USB Mouse Demo");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 63, "Hold [back] to exit");
}

static void usb_mouse_input_callback(InputEvent* input_event, void* ctx) {
    FurryMessageQueue* event_queue = ctx;

    UsbMouseEvent event;
    event.type = EventTypeInput;
    event.input = *input_event;
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

int32_t usb_mouse_app(void* p) {
    UNUSED(p);
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(UsbMouseEvent));
    furry_check(event_queue);
    ViewPort* view_port = view_port_alloc();

    FurryHalUsbInterface* usb_mode_prev = furry_hal_usb_get_config();
    furry_hal_usb_unlock();
    furry_check(furry_hal_usb_set_config(&usb_hid, NULL) == true);

    view_port_draw_callback_set(view_port, usb_mouse_render_callback, NULL);
    view_port_input_callback_set(view_port, usb_mouse_input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    UsbMouseEvent event;
    while(1) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, FurryWaitForever);

        if(event_status == FurryStatusOk) {
            if(event.type == EventTypeInput) {
                if(event.input.type == InputTypeLong && event.input.key == InputKeyBack) {
                    break;
                }

                if(event.input.type == InputTypeShort && event.input.key == InputKeyBack) {
                    furry_hal_hid_mouse_press(HID_MOUSE_BTN_RIGHT);
                    furry_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
                }

                if(event.input.key == InputKeyOk) {
                    if(event.input.type == InputTypePress) {
                        furry_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
                    } else if(event.input.type == InputTypeRelease) {
                        furry_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
                    }
                }

                if(event.input.key == InputKeyRight) {
                    if(event.input.type == InputTypePress) {
                        furry_hal_hid_mouse_move(MOUSE_MOVE_SHORT, 0);
                    } else if(event.input.type == InputTypeRepeat) {
                        furry_hal_hid_mouse_move(MOUSE_MOVE_LONG, 0);
                    }
                }

                if(event.input.key == InputKeyLeft) {
                    if(event.input.type == InputTypePress) {
                        furry_hal_hid_mouse_move(-MOUSE_MOVE_SHORT, 0);
                    } else if(event.input.type == InputTypeRepeat) {
                        furry_hal_hid_mouse_move(-MOUSE_MOVE_LONG, 0);
                    }
                }

                if(event.input.key == InputKeyDown) {
                    if(event.input.type == InputTypePress) {
                        furry_hal_hid_mouse_move(0, MOUSE_MOVE_SHORT);
                    } else if(event.input.type == InputTypeRepeat) {
                        furry_hal_hid_mouse_move(0, MOUSE_MOVE_LONG);
                    }
                }

                if(event.input.key == InputKeyUp) {
                    if(event.input.type == InputTypePress) {
                        furry_hal_hid_mouse_move(0, -MOUSE_MOVE_SHORT);
                    } else if(event.input.type == InputTypeRepeat) {
                        furry_hal_hid_mouse_move(0, -MOUSE_MOVE_LONG);
                    }
                }
            }
        }
        view_port_update(view_port);
    }

    furry_hal_usb_set_config(usb_mode_prev, NULL);

    // remove & free all stuff created by app
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);

    return 0;
}
