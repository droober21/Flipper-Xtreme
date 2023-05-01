#include <string.h>
#include <furry.h>
#include <furry_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include "version.h"
#include "tools.h"

// Uncomment to be able to make a screenshot
//#define USB_HID_AUTOFIRE_SCREENSHOT

typedef enum {
    EventTypeInput,
} EventType;

typedef struct {
    union {
        InputEvent input;
    };
    EventType type;
} UsbMouseEvent;

bool btn_left_autofire = false;
uint32_t autofire_delay = 10;

static void usb_hid_autofire_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    char autofire_delay_str[12];
    //std::string pi = "pi is " + std::to_string(3.1415926);
    itoa(autofire_delay, autofire_delay_str, 10);
    //sprintf(autofire_delay_str, "%lu", autofire_delay);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "USB HID Autofire");
    canvas_draw_str(canvas, 0, 34, btn_left_autofire ? "<active>" : "<inactive>");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 90, 10, "v");
    canvas_draw_str(canvas, 96, 10, VERSION);
    canvas_draw_str(canvas, 0, 22, "Press [ok] for auto left clicking");
    canvas_draw_str(canvas, 0, 46, "delay [ms]:");
    canvas_draw_str(canvas, 50, 46, autofire_delay_str);
    canvas_draw_str(canvas, 0, 63, "Press [back] to exit");
}

static void usb_hid_autofire_input_callback(InputEvent* input_event, void* ctx) {
    FurryMessageQueue* event_queue = ctx;

    UsbMouseEvent event;
    event.type = EventTypeInput;
    event.input = *input_event;
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

int32_t usb_hid_autofire_app(void* p) {
    UNUSED(p);
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(UsbMouseEvent));
    furry_check(event_queue);
    ViewPort* view_port = view_port_alloc();

    FurryHalUsbInterface* usb_mode_prev = furry_hal_usb_get_config();
#ifndef USB_HID_AUTOFIRE_SCREENSHOT
    furry_hal_usb_unlock();
    furry_check(furry_hal_usb_set_config(&usb_hid, NULL) == true);
#endif

    view_port_draw_callback_set(view_port, usb_hid_autofire_render_callback, NULL);
    view_port_input_callback_set(view_port, usb_hid_autofire_input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    UsbMouseEvent event;
    while(1) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 50);

        if(event_status == FurryStatusOk) {
            if(event.type == EventTypeInput) {
                if(event.input.key == InputKeyBack) {
                    break;
                }

                if(event.input.type != InputTypeRelease) {
                    continue;
                }

                switch(event.input.key) {
                case InputKeyOk:
                    btn_left_autofire = !btn_left_autofire;
                    break;
                case InputKeyLeft:
                    if(autofire_delay > 0) {
                        autofire_delay -= 10;
                    }
                    break;
                case InputKeyRight:
                    autofire_delay += 10;
                    break;
                default:
                    break;
                }
            }
        }

        if(btn_left_autofire) {
            furry_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
            // TODO: Don't wait, but use the timer directly to just don't send the release event (see furry_hal_cortex_delay_us)
            furry_delay_us(autofire_delay * 500);
            furry_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
            furry_delay_us(autofire_delay * 500);
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
