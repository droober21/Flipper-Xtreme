#include <furry.h>
#include <gui/gui.h>

#include <input/input.h>
#include <stdlib.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TOTAL_PIXELS SCREEN_WIDTH* SCREEN_HEIGHT

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    bool revive;
    int evo;
    FurryMutex* mutex;
} State;

unsigned char new[TOTAL_PIXELS] = {};
unsigned char old[TOTAL_PIXELS] = {};
unsigned char* fields[] = {new, old};

int current = 0;
int next = 1;

unsigned char get_cell(int x, int y) {
    if(x <= 0 || x >= SCREEN_WIDTH) return 0;
    if(y <= 0 || y >= SCREEN_HEIGHT) return 0;

    int pix = (y * SCREEN_WIDTH) + x;
    return fields[current][pix];
}

int count_neightbors(int x, int y) {
    return get_cell(x + 1, y - 1) + get_cell(x - 1, y - 1) + get_cell(x - 1, y + 1) +
           get_cell(x + 1, y + 1) + get_cell(x + 1, y) + get_cell(x - 1, y) + get_cell(x, y - 1) +
           get_cell(x, y + 1);
}

static void update_field(State* state) {
    if(state->revive) {
        for(int i = 0; i < TOTAL_PIXELS; ++i) {
            if((random() % 100) == 1) {
                fields[current][i] = 1;
            }
            state->revive = false;
        }
    }

    for(int i = 0; i < TOTAL_PIXELS; ++i) {
        int x = i % SCREEN_WIDTH;
        int y = (int)(i / SCREEN_WIDTH);

        int v = get_cell(x, y);
        int n = count_neightbors(x, y);

        if(v && n == 3) {
            ++state->evo;
        } else if(v && (n < 2 || n > 3)) {
            ++state->evo;
            v = 0;
        } else if(!v && n == 3) {
            ++state->evo;
            v = 1;
        }

        fields[next][i] = v;
    }

    next ^= current;
    current ^= next;
    next ^= current;

    if(state->evo < TOTAL_PIXELS) {
        state->revive = true;
        state->evo = 0;
    }
}

static void input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    AppEvent event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, 0);
}

static void render_callback(Canvas* canvas, void* ctx) {
    //furry_assert(ctx);
    State* state = ctx;
    furry_mutex_acquire(state->mutex, FurryWaitForever);

    canvas_clear(canvas);

    for(int i = 0; i < TOTAL_PIXELS; ++i) {
        int x = i % SCREEN_WIDTH;
        int y = (int)(i / SCREEN_WIDTH);
        if(fields[current][i] == 1) canvas_draw_dot(canvas, x, y);
    }
    furry_mutex_release(state->mutex);
}

int32_t game_of_life_app(void* p) {
    UNUSED(p);
    srand(DWT->CYCCNT);

    FurryMessageQueue* event_queue = furry_message_queue_alloc(1, sizeof(AppEvent));
    furry_check(event_queue);

    State* _state = malloc(sizeof(State));

    _state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!_state->mutex) {
        printf("cannot create mutex\r\n");
        furry_message_queue_free(event_queue);
        free(_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, _state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    AppEvent event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 25);
        furry_mutex_acquire(_state->mutex, FurryWaitForever);

        if(event_status == FurryStatusOk && event.type == EventTypeKey &&
           event.input.type == InputTypePress) {
            if(event.input.key == InputKeyBack) {
                // furryac_exit(NULL);
                processing = false;
                furry_mutex_release(_state->mutex);
                break;
            }
        }

        update_field(_state);

        view_port_update(view_port);
        furry_mutex_release(_state->mutex);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(_state->mutex);
    free(_state);

    return 0;
}