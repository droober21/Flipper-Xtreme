#include "../dap_gui_i.h"

#define DAP_VERSION_APP "0.1.0"
#define DAP_DEVELOPED "Dr_Zlo"
#define DAP_GITHUB "https://github.com/flipperdevices/flipperzero-firmware"

void dap_scene_about_on_enter(void* context) {
    DapGuiApp* app = context;

    FurryString* temp_str;
    temp_str = furry_string_alloc();
    furry_string_printf(temp_str, "\e#%s\n", "Information");

    furry_string_cat_printf(temp_str, "Version: %s\n", DAP_VERSION_APP);
    furry_string_cat_printf(temp_str, "Developed by: %s\n", DAP_DEVELOPED);
    furry_string_cat_printf(temp_str, "Github: %s\n\n", DAP_GITHUB);

    furry_string_cat_printf(temp_str, "\e#%s\n", "Description");
    furry_string_cat_printf(
        temp_str, "CMSIS-DAP debugger\nbased on Free-DAP\nThanks to Alex Taradov\n\n");

    furry_string_cat_printf(
        temp_str,
        "Supported protocols:\n"
        "SWD, JTAG, UART\n"
        "DAP v1 (cmsis_backend hid), DAP v2 (cmsis_backend usb_bulk), VCP\n");

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
        "\e#\e!              DAP Link              \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furry_string_get_cstr(temp_str));
    furry_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, DapGuiAppViewWidget);
}

bool dap_scene_about_on_event(void* context, SceneManagerEvent event) {
    DapGuiApp* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void dap_scene_about_on_exit(void* context) {
    DapGuiApp* app = context;

    // Clear views
    widget_reset(app->widget);
}
