/**
 * @file furry_hal.h
 * Furry HAL API
 */

#pragma once

#ifdef __cplusplus
template <unsigned int N>
struct STOP_EXTERNING_ME {};
#endif

#include <furry_hal_cortex.h>
#include <furry_hal_clock.h>
#include <furry_hal_crypto.h>
#include <furry_hal_console.h>
#include <furry_hal_debug.h>
#include <furry_hal_os.h>
#include <furry_hal_sd.h>
#include <furry_hal_i2c.h>
#include <furry_hal_region.h>
#include <furry_hal_resources.h>
#include <furry_hal_rtc.h>
#include <furry_hal_speaker.h>
#include <furry_hal_gpio.h>
#include <furry_hal_light.h>
#include <furry_hal_power.h>
#include <furry_hal_interrupt.h>
#include <furry_hal_version.h>
#include <furry_hal_bt.h>
#include <furry_hal_spi.h>
#include <furry_hal_flash.h>
#include <furry_hal_vibro.h>
#include <furry_hal_usb.h>
#include <furry_hal_usb_hid.h>
#include <furry_hal_uart.h>
#include <furry_hal_info.h>
#include <furry_hal_random.h>
#include <furry_hal_target_hw.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Set whether booting normally with all subsystems */
void furry_hal_set_is_normal_boot(bool value);

/** True if booting normally with all subsystems */
bool furry_hal_is_normal_boot();

/** Early FurryHal init, only essential subsystems */
void furry_hal_init_early();

/** Early FurryHal deinit */
void furry_hal_deinit_early();

/** Init FurryHal */
void furry_hal_init();

/** Transfer execution to address
 *
 * @param[in]  address  pointer to new executable
 */
void furry_hal_switch(void* address);

#ifdef __cplusplus
}
#endif
