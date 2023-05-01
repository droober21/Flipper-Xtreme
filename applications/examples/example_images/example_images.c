#include <furry.h>
#include <furry_hal.h>

#include <gui/gui.h>
#include <input/input.h>

/* Magic happens here -- this file is generated by fbt.
 * Just set fap_icon_assets in application.fam and #include {APPID}_icons.h */
#include "example_images_icons.h"

typedef struct {
    uint8_t x, y;
} ImagePosition;

static ImagePosition image_position = {.x = 0, .y = 0};

// Screen is 128x64 px
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    canvas_draw_icon(canvas, image_position.x % 128, image_position.y % 64, &I_dolphin_71x25);
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furry_assert(ctx);

    FurryMessageQueue* event_queue = ctx;
    furry_message_queue_put(event_queue, input_event, FurryWaitForever);
}

int32_t example_images_main(void* p) {
    UNUSED(p);
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(InputEvent));

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, view_port);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;

    bool running = true;
    while(running) {
        if(furry_message_queue_get(event_queue, &event, 100) == FurryStatusOk) {
            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                switch(event.key) {
                case InputKeyLeft:
                    image_position.x -= 2;
                    break;
                case InputKeyRight:
                    image_position.x += 2;
                    break;
                case InputKeyUp:
                    image_position.y -= 2;
                    break;
                case InputKeyDown:
                    image_position.y += 2;
                    break;
                default:
                    running = false;
                    break;
                }
            }
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);

    furry_record_close(RECORD_GUI);

    return 0;
}
