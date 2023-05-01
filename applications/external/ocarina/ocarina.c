#include <furry.h>
#include <furry_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define NOTE_UP 587.33f
#define NOTE_LEFT 493.88f
#define NOTE_RIGHT 440.00f
#define NOTE_DOWN 349.23
#define NOTE_OK 293.66f

typedef struct {
    FurryMutex* model_mutex;

    FurryMessageQueue* event_queue;

    ViewPort* view_port;
    Gui* gui;
} Ocarina;

void draw_callback(Canvas* canvas, void* ctx) {
    Ocarina* ocarina = ctx;
    furry_check(furry_mutex_acquire(ocarina->model_mutex, FurryWaitForever) == FurryStatusOk);

    //canvas_draw_box(canvas, ocarina->model->x, ocarina->model->y, 4, 4);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_str(canvas, 50, 10, "Ocarina");
    canvas_draw_str(canvas, 30, 20, "OK button for A");

    furry_mutex_release(ocarina->model_mutex);
}

void input_callback(InputEvent* input, void* ctx) {
    Ocarina* ocarina = ctx;
    // Puts input onto event queue with priority 0, and waits until completion.
    furry_message_queue_put(ocarina->event_queue, input, FurryWaitForever);
}

Ocarina* ocarina_alloc() {
    Ocarina* instance = malloc(sizeof(Ocarina));

    instance->model_mutex = furry_mutex_alloc(FurryMutexTypeNormal);

    instance->event_queue = furry_message_queue_alloc(8, sizeof(InputEvent));

    instance->view_port = view_port_alloc();
    view_port_draw_callback_set(instance->view_port, draw_callback, instance);
    view_port_input_callback_set(instance->view_port, input_callback, instance);

    instance->gui = furry_record_open("gui");
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);

    return instance;
}

void ocarina_free(Ocarina* instance) {
    view_port_enabled_set(instance->view_port, false); // Disabsles our ViewPort
    gui_remove_view_port(instance->gui, instance->view_port); // Removes our ViewPort from the Gui
    furry_record_close("gui"); // Closes the gui record
    view_port_free(instance->view_port); // Frees memory allocated by view_port_alloc
    furry_message_queue_free(instance->event_queue);

    furry_mutex_free(instance->model_mutex);

    if(furry_hal_speaker_is_mine()) {
        furry_hal_speaker_stop();
        furry_hal_speaker_release();
    }

    free(instance);
}

int32_t ocarina_app(void* p) {
    UNUSED(p);

    Ocarina* ocarina = ocarina_alloc();

    InputEvent event;
    for(bool processing = true; processing;) {
        // Pops a message off the queue and stores it in `event`.
        // No message priority denoted by NULL, and 100 ticks of timeout.
        FurryStatus status = furry_message_queue_get(ocarina->event_queue, &event, 100);
        furry_check(furry_mutex_acquire(ocarina->model_mutex, FurryWaitForever) == FurryStatusOk);

        float volume = 1.0f;
        if(status == FurryStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyUp:
                    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
                        furry_hal_speaker_start(NOTE_UP, volume);
                    }
                    break;
                case InputKeyDown:
                    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
                        furry_hal_speaker_start(NOTE_DOWN, volume);
                    }
                    break;
                case InputKeyLeft:
                    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
                        furry_hal_speaker_start(NOTE_LEFT, volume);
                    }
                    break;
                case InputKeyRight:
                    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
                        furry_hal_speaker_start(NOTE_RIGHT, volume);
                    }
                    break;
                case InputKeyOk:
                    if(furry_hal_speaker_is_mine() || furry_hal_speaker_acquire(1000)) {
                        furry_hal_speaker_start(NOTE_OK, volume);
                    }
                    break;
                case InputKeyBack:
                    processing = false;
                    break;
                default:
                    break;
                }
            } else if(event.type == InputTypeRelease) {
                if(furry_hal_speaker_is_mine()) {
                    furry_hal_speaker_stop();
                    furry_hal_speaker_release();
                }
            }
        }

        furry_mutex_release(ocarina->model_mutex);
        view_port_update(ocarina->view_port); // signals our draw callback
    }
    ocarina_free(ocarina);
    return 0;
}
