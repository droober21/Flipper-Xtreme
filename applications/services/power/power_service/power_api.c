#include "power_i.h"

#include <furry.h>
#include <furry_hal.h>
#include <update_util/update_operation.h>

void power_off(Power* power) {
    furry_hal_power_off();
    // Notify user if USB is plugged
    view_dispatcher_send_to_front(power->view_dispatcher);
    view_dispatcher_switch_to_view(power->view_dispatcher, PowerViewUnplugUsb);
    furry_delay_ms(100);
    furry_halt("Disconnect USB for safe shutdown");
}

void power_reboot(PowerBootMode mode) {
    if(mode == PowerBootModeNormal) {
        update_operation_disarm();
    } else if(mode == PowerBootModeDfu) {
        furry_hal_rtc_set_boot_mode(FurryHalRtcBootModeDfu);
    } else if(mode == PowerBootModeUpdateStart) {
        furry_hal_rtc_set_boot_mode(FurryHalRtcBootModePreUpdate);
    }
    furry_hal_power_reset();
}

void power_get_info(Power* power, PowerInfo* info) {
    furry_assert(power);
    furry_assert(info);

    furry_mutex_acquire(power->api_mtx, FurryWaitForever);
    memcpy(info, &power->info, sizeof(power->info));
    furry_mutex_release(power->api_mtx);
}

FurryPubSub* power_get_pubsub(Power* power) {
    furry_assert(power);
    return power->event_pubsub;
}

FurryPubSub* power_get_settings_events_pubsub(Power* power) {
    furry_assert(power);
    return power->settings_events;
}

bool power_is_battery_healthy(Power* power) {
    furry_assert(power);
    bool is_healthy = false;
    furry_mutex_acquire(power->api_mtx, FurryWaitForever);
    is_healthy = power->info.health > POWER_BATTERY_HEALTHY_LEVEL;
    furry_mutex_release(power->api_mtx);
    return is_healthy;
}

void power_enable_low_battery_level_notification(Power* power, bool enable) {
    furry_assert(power);
    furry_mutex_acquire(power->api_mtx, FurryWaitForever);
    power->show_low_bat_level_message = enable;
    furry_mutex_release(power->api_mtx);
}
