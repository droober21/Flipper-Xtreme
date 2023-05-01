#include "usb_mouse.h"
#include "../tracking/main_loop.h"

#include <furry.h>
#include <furry_hal_usb.h>
#include <furry_hal_usb_hid.h>
#include <gui/elements.h>

struct UsbMouse {
    View* view;
    ViewDispatcher* view_dispatcher;
    FurryHalUsbInterface* usb_mode_prev;
};

static void usb_mouse_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "USB Mouse mode");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 63, "Hold [back] to exit");
}

#define MOUSE_SCROLL 2

static void usb_mouse_process(UsbMouse* usb_mouse, InputEvent* event) {
    with_view_model(
        usb_mouse->view,
        void* model,
        {
            UNUSED(model);
            if(event->key == InputKeyUp) {
                if(event->type == InputTypePress) {
                    furry_hal_hid_mouse_press(HID_MOUSE_BTN_LEFT);
                } else if(event->type == InputTypeRelease) {
                    furry_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
                }
            } else if(event->key == InputKeyDown) {
                if(event->type == InputTypePress) {
                    furry_hal_hid_mouse_press(HID_MOUSE_BTN_RIGHT);
                } else if(event->type == InputTypeRelease) {
                    furry_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
                }
            } else if(event->key == InputKeyOk) {
                if(event->type == InputTypePress) {
                    furry_hal_hid_mouse_press(HID_MOUSE_BTN_WHEEL);
                } else if(event->type == InputTypeRelease) {
                    furry_hal_hid_mouse_release(HID_MOUSE_BTN_WHEEL);
                }
            } else if(event->key == InputKeyRight) {
                if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                    furry_hal_hid_mouse_scroll(MOUSE_SCROLL);
                }
            } else if(event->key == InputKeyLeft) {
                if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                    furry_hal_hid_mouse_scroll(-MOUSE_SCROLL);
                }
            }
        },
        true);
}

static bool usb_mouse_input_callback(InputEvent* event, void* context) {
    furry_assert(context);
    UsbMouse* usb_mouse = context;
    bool consumed = false;

    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        // furry_hal_hid_mouse_release_all();
    } else {
        usb_mouse_process(usb_mouse, event);
        consumed = true;
    }

    return consumed;
}

void usb_mouse_enter_callback(void* context) {
    furry_assert(context);
    UsbMouse* usb_mouse = context;

    usb_mouse->usb_mode_prev = furry_hal_usb_get_config();
    furry_hal_usb_unlock();
    furry_check(furry_hal_usb_set_config(&usb_hid, NULL) == true);

    tracking_begin();

    view_dispatcher_send_custom_event(usb_mouse->view_dispatcher, 0);
}

bool usb_mouse_move(int8_t dx, int8_t dy, void* context) {
    UNUSED(context);
    return furry_hal_hid_mouse_move(dx, dy);
}

bool usb_mouse_custom_callback(uint32_t event, void* context) {
    UNUSED(event);
    furry_assert(context);
    UsbMouse* usb_mouse = context;

    tracking_step(usb_mouse_move, context);
    furry_delay_ms(3); // Magic! Removing this will break the buttons

    view_dispatcher_send_custom_event(usb_mouse->view_dispatcher, 0);
    return true;
}

void usb_mouse_exit_callback(void* context) {
    furry_assert(context);
    UsbMouse* usb_mouse = context;

    tracking_end();

    furry_hal_usb_set_config(usb_mouse->usb_mode_prev, NULL);
}

UsbMouse* usb_mouse_alloc(ViewDispatcher* view_dispatcher) {
    UsbMouse* usb_mouse = malloc(sizeof(UsbMouse));
    usb_mouse->view = view_alloc();
    usb_mouse->view_dispatcher = view_dispatcher;
    view_set_context(usb_mouse->view, usb_mouse);
    view_set_draw_callback(usb_mouse->view, usb_mouse_draw_callback);
    view_set_input_callback(usb_mouse->view, usb_mouse_input_callback);
    view_set_enter_callback(usb_mouse->view, usb_mouse_enter_callback);
    view_set_custom_callback(usb_mouse->view, usb_mouse_custom_callback);
    view_set_exit_callback(usb_mouse->view, usb_mouse_exit_callback);
    return usb_mouse;
}

void usb_mouse_free(UsbMouse* usb_mouse) {
    furry_assert(usb_mouse);
    view_free(usb_mouse->view);
    free(usb_mouse);
}

View* usb_mouse_get_view(UsbMouse* usb_mouse) {
    furry_assert(usb_mouse);
    return usb_mouse->view;
}
