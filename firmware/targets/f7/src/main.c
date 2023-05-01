#include <furry.h>
#include <furry_hal.h>
#include <flipper.h>
#include <alt_boot.h>
#include <semphr.h>
#include <update_util/update_operation.h>

#define TAG "Main"

int32_t init_task(void* context) {
    UNUSED(context);

    // Flipper FURRY HAL
    furry_hal_init();

    // Init flipper
    flipper_init();

    return 0;
}

int main() {
    // Initialize FURRY layer
    furry_init();

    // Flipper critical FURRY HAL
    furry_hal_init_early();

    furry_hal_set_is_normal_boot(false);
    FurryThread* main_thread = furry_thread_alloc_ex("Init", 4096, init_task, NULL);

#ifdef FURRY_RAM_EXEC
    // Prevent entering sleep mode when executed from RAM
    furry_hal_power_insomnia_enter();
    furry_thread_start(main_thread);
#else
    furry_hal_light_sequence("RGB");

    // Delay is for button sampling
    furry_delay_ms(100);

    FurryHalRtcBootMode boot_mode = furry_hal_rtc_get_boot_mode();
    if(boot_mode == FurryHalRtcBootModeDfu || !furry_hal_gpio_read(&gpio_button_left)) {
        furry_hal_light_sequence("rgb WB");
        furry_hal_rtc_set_boot_mode(FurryHalRtcBootModeNormal);
        flipper_boot_dfu_exec();
        furry_hal_power_reset();
    } else if(boot_mode == FurryHalRtcBootModeUpdate) {
        furry_hal_light_sequence("rgb BR");
        // Do update
        flipper_boot_update_exec();
        // if things go nice, we shouldn't reach this point.
        // But if we do, abandon to avoid bootloops
        furry_hal_rtc_set_boot_mode(FurryHalRtcBootModeNormal);
        furry_hal_power_reset();
    } else if(!furry_hal_gpio_read(&gpio_button_up)) {
        furry_hal_light_sequence("rgb WR");
        flipper_boot_recovery_exec();
        furry_hal_power_reset();
    } else {
        furry_hal_light_sequence("rgb G");
        furry_hal_set_is_normal_boot(true);
        furry_thread_start(main_thread);
    }
#endif

    // Run Kernel
    furry_run();

    furry_crash("Kernel is Dead");
}

void Error_Handler(void) {
    furry_crash("ErrorHandler");
}

void abort() {
    furry_crash("AbortHandler");
}
