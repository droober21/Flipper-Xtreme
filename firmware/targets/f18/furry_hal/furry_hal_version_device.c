#include <furry_hal_version.h>

bool furry_hal_version_do_i_belong_here() {
    return (furry_hal_version_get_hw_target() == 18) || (furry_hal_version_get_hw_target() == 0);
}

const char* furry_hal_version_get_model_name() {
    return "Komi";
}

const char* furry_hal_version_get_model_code() {
    return "N/A";
}

const char* furry_hal_version_get_fcc_id() {
    return "N/A";
}

const char* furry_hal_version_get_ic_id() {
    return "N/A";
}
