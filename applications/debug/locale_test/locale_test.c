#include <furry.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/dialog_ex.h>
#include <locale/locale.h>

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* view;
} LocaleTestApp;

static void locale_test_view_draw_callback(Canvas* canvas, void* _model) {
    UNUSED(_model);

    // Prepare canvas
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    FurryString* tmp_string = furry_string_alloc();

    float temp = 25.3f;
    LocaleMeasurementUnits units = locale_get_measurement_unit();
    if(units == LocaleMeasurementUnitsMetric) {
        furry_string_printf(tmp_string, "Temp: %5.1fC", (double)temp);
    } else {
        temp = locale_celsius_to_fahrenheit(temp);
        furry_string_printf(tmp_string, "Temp: %5.1fF", (double)temp);
    }
    canvas_draw_str(canvas, 0, 10, furry_string_get_cstr(tmp_string));

    FurryHalRtcDateTime datetime;
    furry_hal_rtc_get_datetime(&datetime);

    locale_format_time(tmp_string, &datetime, locale_get_time_format(), false);
    canvas_draw_str(canvas, 0, 25, furry_string_get_cstr(tmp_string));

    locale_format_date(tmp_string, &datetime, locale_get_date_format(), "/");
    canvas_draw_str(canvas, 0, 40, furry_string_get_cstr(tmp_string));

    furry_string_free(tmp_string);
}

static bool locale_test_view_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

static uint32_t locale_test_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static LocaleTestApp* locale_test_alloc() {
    LocaleTestApp* app = malloc(sizeof(LocaleTestApp));

    // Gui
    app->gui = furry_record_open(RECORD_GUI);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Views
    app->view = view_alloc();
    view_set_draw_callback(app->view, locale_test_view_draw_callback);
    view_set_input_callback(app->view, locale_test_view_input_callback);

    view_set_previous_callback(app->view, locale_test_exit);
    view_dispatcher_add_view(app->view_dispatcher, 0, app->view);
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    return app;
}

static void locale_test_free(LocaleTestApp* app) {
    furry_assert(app);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, 0);

    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);

    // Close gui record
    furry_record_close(RECORD_GUI);
    app->gui = NULL;

    // Free rest
    free(app);
}

int32_t locale_test_app(void* p) {
    UNUSED(p);
    LocaleTestApp* app = locale_test_alloc();
    view_dispatcher_run(app->view_dispatcher);
    locale_test_free(app);
    return 0;
}
