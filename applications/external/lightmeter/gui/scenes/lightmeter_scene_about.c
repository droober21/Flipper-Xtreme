#include "../../lightmeter.h"

void lightmeter_scene_about_widget_callback(GuiButtonType result, InputType type, void* context) {
    LightMeterApp* app = context;

    UNUSED(app);
    UNUSED(result);
    UNUSED(type);
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void lightmeter_scene_about_on_enter(void* context) {
    LightMeterApp* app = context;

    FurryString* temp_str;
    temp_str = furry_string_alloc();
    furry_string_printf(temp_str, "\e#%s\n", "Information");

    furry_string_cat_printf(temp_str, "Version: %s\n", LM_VERSION_APP);
    furry_string_cat_printf(temp_str, "Developed by: %s\n", LM_DEVELOPED);
    furry_string_cat_printf(temp_str, "Github: %s\n\n", LM_GITHUB);

    furry_string_cat_printf(temp_str, "\e#%s\n", "Description");
    furry_string_cat_printf(
        temp_str,
        "Showing suggested camera\nsettings based on ambient\nlight or flash.\n\nInspired by a lightmeter\nproject by vpominchuk\n");

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                                      \e!\n",
        false);
    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!            Lightmeter            \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furry_string_get_cstr(temp_str));
    furry_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, LightMeterAppViewAbout);
}

bool lightmeter_scene_about_on_event(void* context, SceneManagerEvent event) {
    LightMeterApp* app = context;

    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void lightmeter_scene_about_on_exit(void* context) {
    LightMeterApp* app = context;

    // Clear views
    widget_reset(app->widget);
}
