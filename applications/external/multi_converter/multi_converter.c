#include <furry.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include "multi_converter_definitions.h"
#include "multi_converter_mode_display.h"
#include "multi_converter_mode_select.h"

static void multi_converter_render_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    const MultiConverterState* multi_converter_state = ctx;
    furry_mutex_acquire(multi_converter_state->mutex, FurryWaitForever);

    if(multi_converter_state->mode == ModeDisplay) {
        multi_converter_mode_display_draw(canvas, multi_converter_state);
    } else {
        multi_converter_mode_select_draw(canvas, multi_converter_state);
    }

    furry_mutex_release(multi_converter_state->mutex);
}

static void
    multi_converter_input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    MultiConverterEvent event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

static void multi_converter_init(MultiConverterState* const multi_converter_state) {
    // initial default values

    multi_converter_state->buffer_orig[MULTI_CONVERTER_NUMBER_DIGITS] = '\0';
    multi_converter_state->buffer_dest[MULTI_CONVERTER_NUMBER_DIGITS] = '\0'; // null terminators

    multi_converter_state->unit_type_orig = UnitTypeDec;
    multi_converter_state->unit_type_dest = UnitTypeHex;

    multi_converter_state->keyboard_lock = 0;

    // init the display view
    multi_converter_mode_display_reset(multi_converter_state);

    // init the select view
    multi_converter_mode_select_reset(multi_converter_state);

    // set ModeDisplay as the current mode
    multi_converter_state->mode = ModeDisplay;
}

// main entry point
int32_t multi_converter_app(void* p) {
    UNUSED(p);

    // get event queue
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(MultiConverterEvent));

    // allocate state
    MultiConverterState* multi_converter_state = malloc(sizeof(MultiConverterState));

    // set mutex for plugin state (different threads can access it)
    multi_converter_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!multi_converter_state->mutex) {
        FURRY_LOG_E("MultiConverter", "cannot create mutex\r\n");
        furry_message_queue_free(event_queue);
        free(multi_converter_state);
        return 255;
    }

    // register callbacks for drawing and input processing
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, multi_converter_render_callback, multi_converter_state);
    view_port_input_callback_set(view_port, multi_converter_input_callback, event_queue);

    // open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    multi_converter_init(multi_converter_state);

    // main loop
    MultiConverterEvent event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 100);
        furry_mutex_acquire(multi_converter_state->mutex, FurryWaitForever);

        if(event_status == FurryStatusOk) {
            // press events
            if(event.type == EventTypeKey && !multi_converter_state->keyboard_lock) {
                if(multi_converter_state->mode == ModeDisplay) {
                    if(event.input.key == InputKeyBack) {
                        if(event.input.type == InputTypePress) processing = false;
                    } else if(event.input.key == InputKeyOk) { // the "ok" press can be short or long
                        MultiConverterModeTrigger t = None;

                        if(event.input.type == InputTypeLong)
                            t = multi_converter_mode_display_ok(1, multi_converter_state);
                        else if(event.input.type == InputTypeShort)
                            t = multi_converter_mode_display_ok(0, multi_converter_state);

                        if(t == Reset) {
                            multi_converter_mode_select_reset(multi_converter_state);
                            multi_converter_state->mode = ModeSelector;
                        }
                    } else {
                        if(event.input.type == InputTypePress)
                            multi_converter_mode_display_navigation(
                                event.input.key, multi_converter_state);
                    }

                } else { // ModeSelect
                    if(event.input.type == InputTypePress) {
                        switch(event.input.key) {
                        default:
                            break;
                        case InputKeyBack:
                        case InputKeyOk: {
                            MultiConverterModeTrigger t = multi_converter_mode_select_exit(
                                event.input.key == InputKeyOk ? 1 : 0, multi_converter_state);

                            if(t == Reset) {
                                multi_converter_mode_display_reset(multi_converter_state);
                            } else if(t == Convert) {
                                multi_converter_mode_display_convert(multi_converter_state);
                            }

                            multi_converter_state->keyboard_lock = 1;
                            multi_converter_state->mode = ModeDisplay;
                            break;
                        }
                        case InputKeyLeft:
                        case InputKeyRight:
                            multi_converter_mode_select_switch(multi_converter_state);
                            break;
                        case InputKeyUp:
                            multi_converter_mode_select_change_unit(-1, multi_converter_state);
                            break;
                        case InputKeyDown:
                            multi_converter_mode_select_change_unit(1, multi_converter_state);
                            break;
                        }
                    }
                }
            } else if(multi_converter_state->keyboard_lock) {
                multi_converter_state->keyboard_lock = 0;
            }
        }

        view_port_update(view_port);
        furry_mutex_release(multi_converter_state->mutex);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(multi_converter_state->mutex);
    free(multi_converter_state);

    return 0;
}