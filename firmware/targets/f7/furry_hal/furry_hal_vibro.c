#include <furry_hal_vibro.h>
#include <furry_hal_gpio.h>

#define TAG "FurryHalVibro"

void furry_hal_vibro_init() {
    furry_hal_gpio_init(&gpio_vibro, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_write(&gpio_vibro, false);
    FURRY_LOG_I(TAG, "Init OK");
}

void furry_hal_vibro_on(bool value) {
    furry_hal_gpio_write(&gpio_vibro, value);
}
