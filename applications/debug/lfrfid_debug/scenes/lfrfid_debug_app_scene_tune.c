#include "../lfrfid_debug_i.h"
#include <furry_hal.h>

static void comparator_trigger_callback(bool level, void* comp_ctx) {
    UNUSED(comp_ctx);
    furry_hal_gpio_write(&gpio_ext_pa7, !level);
}

void lfrfid_debug_scene_tune_on_enter(void* context) {
    LfRfidDebug* app = context;

    furry_hal_gpio_init_simple(&gpio_ext_pa7, GpioModeOutputPushPull);

    furry_hal_rfid_comp_set_callback(comparator_trigger_callback, app);
    furry_hal_rfid_comp_start();

    furry_hal_rfid_pins_read();
    furry_hal_rfid_tim_read(125000, 0.5);
    furry_hal_rfid_tim_read_start();

    view_dispatcher_switch_to_view(app->view_dispatcher, LfRfidDebugViewTune);
}

bool lfrfid_debug_scene_tune_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);

    LfRfidDebug* app = context;
    bool consumed = false;

    if(lfrfid_debug_view_tune_is_dirty(app->tune_view)) {
        furry_hal_rfid_set_read_period(lfrfid_debug_view_tune_get_arr(app->tune_view));
        furry_hal_rfid_set_read_pulse(lfrfid_debug_view_tune_get_ccr(app->tune_view));
    }

    return consumed;
}

void lfrfid_debug_scene_tune_on_exit(void* context) {
    UNUSED(context);

    furry_hal_rfid_comp_stop();
    furry_hal_rfid_comp_set_callback(NULL, NULL);

    furry_hal_gpio_init_simple(&gpio_ext_pa7, GpioModeAnalog);
    furry_hal_rfid_tim_read_stop();
    furry_hal_rfid_tim_reset();
    furry_hal_rfid_pins_reset();
}
