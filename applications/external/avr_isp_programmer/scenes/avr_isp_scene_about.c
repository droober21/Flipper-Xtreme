#include "../avr_isp_app_i.h"
#include "../helpers/avr_isp_types.h"

void avr_isp_scene_about_widget_callback(GuiButtonType result, InputType type, void* context) {
    furry_assert(context);

    AvrIspApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void avr_isp_scene_about_on_enter(void* context) {
    furry_assert(context);

    AvrIspApp* app = context;
    FurryString* temp_str = furry_string_alloc();
    furry_string_printf(temp_str, "\e#%s\n", "Information");

    furry_string_cat_printf(temp_str, "Version: %s\n", AVR_ISP_VERSION_APP);
    furry_string_cat_printf(temp_str, "Developed by: %s\n", AVR_ISP_DEVELOPED);
    furry_string_cat_printf(temp_str, "Github: %s\n\n", AVR_ISP_GITHUB);

    furry_string_cat_printf(temp_str, "\e#%s\n", "Description");
    furry_string_cat_printf(
        temp_str,
        "This application is an AVR in-system programmer based on stk500mk1. It is compatible with AVR-based"
        " microcontrollers including Arduino. You can also use it to repair the chip if you accidentally"
        " corrupt the bootloader.\n\n");

    furry_string_cat_printf(temp_str, "\e#%s\n", "What it can do:");
    furry_string_cat_printf(temp_str, "- Create a dump of your chip on an SD card\n");
    furry_string_cat_printf(temp_str, "- Flash your chip firmware from the SD card\n");
    furry_string_cat_printf(temp_str, "- Act as a wired USB ISP using avrdude software\n\n");

    furry_string_cat_printf(temp_str, "\e#%s\n", "Supported chip series:");
    furry_string_cat_printf(
        temp_str,
        "Example command for avrdude flashing: avrdude.exe -p m328p -c stk500v1 -P COMxx -U flash:r:"
        "X:\\sketch_sample.hex"
        ":i\n");
    furry_string_cat_printf(
        temp_str,
        "Where: "
        "-p m328p"
        " brand of your chip, "
        "-P COMxx"
        " com port number in the system when "
        "ISP Programmer"
        " is enabled\n\n");

    furry_string_cat_printf(temp_str, "\e#%s\n", "Info");
    furry_string_cat_printf(
        temp_str,
        "ATtinyXXXX\nATmegaXXXX\nAT43Uxxx\nAT76C711\nAT86RF401\nAT90xxxxx\nAT94K\n"
        "ATAxxxxx\nATA664251\nM3000\nLGT8F88P\nLGT8F168P\nLGT8F328P\n");

    furry_string_cat_printf(
        temp_str, "For a more detailed list of supported chips, see AVRDude help\n");

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
        "\e#\e!        ISP Programmer       \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furry_string_get_cstr(temp_str));
    furry_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, AvrIspViewWidget);
}

bool avr_isp_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void avr_isp_scene_about_on_exit(void* context) {
    furry_assert(context);

    AvrIspApp* app = context;
    // Clear views
    widget_reset(app->widget);
}
