// CC0 1.0 Universal (CC0 1.0)
// Public Domain Dedication
// https://github.com/nmrr

#include <stdio.h>
#include <furry.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furry_hal_random.h>
#include <furry_hal_pwm.h>
#include <furry_hal_power.h>

#define SCREEN_SIZE_X 128
#define SCREEN_SIZE_Y 64

// FOR J305 GEIGER TUBE
#define CONVERSION_FACTOR 0.0081

typedef enum {
    EventTypeInput,
    ClockEventTypeTick,
    EventGPIO,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} EventApp;

typedef struct {
    FurryMutex* mutex;
    uint32_t cps, cpm;
    uint32_t line[SCREEN_SIZE_X / 2];
    float coef;
    uint8_t data;
} mutexStruct;

static void draw_callback(Canvas* canvas, void* ctx) {
    furry_assert(ctx);

    mutexStruct displayStruct;
    mutexStruct* geigerMutex = ctx;
    furry_mutex_acquire(geigerMutex->mutex, FurryWaitForever);
    memcpy(&displayStruct, geigerMutex, sizeof(mutexStruct));
    furry_mutex_release(geigerMutex->mutex);

    char buffer[32];
    if(displayStruct.data == 0)
        snprintf(
            buffer, sizeof(buffer), "%ld cps - %ld cpm", displayStruct.cps, displayStruct.cpm);
    else if(displayStruct.data == 1)
        snprintf(
            buffer,
            sizeof(buffer),
            "%ld cps - %.2f uSv/h",
            displayStruct.cps,
            ((double)displayStruct.cpm * (double)CONVERSION_FACTOR));
    else
        snprintf(
            buffer,
            sizeof(buffer),
            "%ld cps - %.2f mSv/y",
            displayStruct.cps,
            (((double)displayStruct.cpm * (double)CONVERSION_FACTOR)) * (double)8.76);

    for(int i = 0; i < SCREEN_SIZE_X; i += 2) {
        float Y = SCREEN_SIZE_Y - (displayStruct.line[i / 2] * displayStruct.coef);

        canvas_draw_line(canvas, i, Y, i, SCREEN_SIZE_Y);
        canvas_draw_line(canvas, i + 1, Y, i + 1, SCREEN_SIZE_Y);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, buffer);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furry_assert(ctx);
    FurryMessageQueue* event_queue = ctx;
    EventApp event = {.type = EventTypeInput, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

static void clock_tick(void* ctx) {
    furry_assert(ctx);

    uint32_t randomNumber = furry_hal_random_get();
    randomNumber &= 0xFFF;
    if(randomNumber == 0) randomNumber = 1;

    furry_hal_pwm_start(FurryHalPwmOutputIdLptim2PA4, randomNumber, 50);

    FurryMessageQueue* queue = ctx;
    EventApp event = {.type = ClockEventTypeTick};
    furry_message_queue_put(queue, &event, 0);
}

static void gpiocallback(void* ctx) {
    furry_assert(ctx);
    FurryMessageQueue* queue = ctx;
    EventApp event = {.type = EventGPIO};
    furry_message_queue_put(queue, &event, 0);
}

int32_t flipper_geiger_app(void* p) {
    UNUSED(p);
    EventApp event;
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(EventApp));

    furry_hal_gpio_init(&gpio_ext_pa7, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
    furry_hal_pwm_start(FurryHalPwmOutputIdLptim2PA4, 5, 50);

    mutexStruct mutexVal;
    mutexVal.cps = 0;
    mutexVal.cpm = 0;
    for(int i = 0; i < SCREEN_SIZE_X / 2; i++) mutexVal.line[i] = 0;
    mutexVal.coef = 1;
    mutexVal.data = 0;

    uint32_t counter = 0;

    mutexVal.mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!mutexVal.mutex) {
        furry_message_queue_free(event_queue);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &mutexVal);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    furry_hal_gpio_add_int_callback(&gpio_ext_pa7, gpiocallback, event_queue);

    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FurryTimer* timer = furry_timer_alloc(clock_tick, FurryTimerTypePeriodic, event_queue);
    furry_timer_start(timer, 1000);

    // ENABLE 5V pin
    furry_hal_power_enable_otg();

    while(1) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, FurryWaitForever);

        uint8_t screenRefresh = 0;

        if(event_status == FurryStatusOk) {
            if(event.type == EventTypeInput) {
                if(event.input.key == InputKeyBack) {
                    break;
                } else if(event.input.key == InputKeyOk && event.input.type == InputTypeShort) {
                    counter = 0;
                    furry_mutex_acquire(mutexVal.mutex, FurryWaitForever);

                    mutexVal.cps = 0;
                    mutexVal.cpm = 0;
                    for(int i = 0; i < SCREEN_SIZE_X / 2; i++) mutexVal.line[i] = 0;

                    screenRefresh = 1;
                    furry_mutex_release(mutexVal.mutex);
                } else if((event.input.key == InputKeyLeft &&
                           event.input.type == InputTypeShort)) {
                    furry_mutex_acquire(mutexVal.mutex, FurryWaitForever);

                    if(mutexVal.data != 0)
                        mutexVal.data--;
                    else
                        mutexVal.data = 2;

                    screenRefresh = 1;
                    furry_mutex_release(mutexVal.mutex);
                } else if((event.input.key == InputKeyRight &&
                           event.input.type == InputTypeShort)) {
                    furry_mutex_acquire(mutexVal.mutex, FurryWaitForever);

                    if(mutexVal.data != 2)
                        mutexVal.data++;
                    else
                        mutexVal.data = 0;

                    screenRefresh = 1;
                    furry_mutex_release(mutexVal.mutex);
                }
            } else if(event.type == ClockEventTypeTick) {
                furry_mutex_acquire(mutexVal.mutex, FurryWaitForever);

                for(int i = 0; i < SCREEN_SIZE_X / 2 - 1; i++)
                    mutexVal.line[SCREEN_SIZE_X / 2 - 1 - i] =
                        mutexVal.line[SCREEN_SIZE_X / 2 - 2 - i];

                mutexVal.line[0] = counter;
                mutexVal.cps = counter;
                counter = 0;

                mutexVal.cpm = mutexVal.line[0];
                uint32_t max = mutexVal.line[0];
                for(int i = 1; i < SCREEN_SIZE_X / 2; i++) {
                    if(i < 60) mutexVal.cpm += mutexVal.line[i];
                    if(mutexVal.line[i] > max) max = mutexVal.line[i];
                }

                if(max > 0)
                    mutexVal.coef = ((float)(SCREEN_SIZE_Y - 15)) / ((float)max);
                else
                    mutexVal.coef = 1;

                screenRefresh = 1;
                furry_mutex_release(mutexVal.mutex);
            } else if(event.type == EventGPIO) {
                counter++;
            }
        }

        if(screenRefresh == 1) view_port_update(view_port);
    }

    furry_hal_power_disable_otg();

    furry_hal_gpio_disable_int_callback(&gpio_ext_pa7);
    furry_hal_gpio_remove_int_callback(&gpio_ext_pa7);
    furry_hal_pwm_stop(FurryHalPwmOutputIdLptim2PA4);

    furry_message_queue_free(event_queue);
    furry_mutex_free(mutexVal.mutex);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furry_timer_free(timer);
    furry_record_close(RECORD_GUI);

    return 0;
}