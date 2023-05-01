#include <furry_hal_version.h>

bool furry_hal_version_do_i_belong_here() {
    return (furry_hal_version_get_hw_target() == 7) || (furry_hal_version_get_hw_target() == 0);
}

const char* furry_hal_version_get_model_name() {
    return "Flipper Zero";
}

const char* furry_hal_version_get_model_code() {
    return "FZ.1";
}

const char* furry_hal_version_get_fcc_id() {
    return "2A2V6-FZ";
}

const char* furry_hal_version_get_ic_id() {
    return "27624-FZ";
}
