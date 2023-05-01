
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stddef.h>
#include <furry.h>
#include <furry_hal.h>
#include <gui/gui.h>

#include "../helpers/pin_lock.h"
#include "../desktop_i.h"
#include <cli/cli.h>
#include <cli/cli_vcp.h>
#include <xtreme.h>

static const NotificationSequence sequence_pin_fail = {
    &message_display_backlight_on,

    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,

    &message_delay_250,

    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

void desktop_pin_lock_error_notify() {
    NotificationApp* notification = furry_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_pin_fail);
    furry_record_close(RECORD_NOTIFICATION);
}

uint32_t desktop_pin_lock_get_fail_timeout() {
    uint32_t pin_fails = furry_hal_rtc_get_pin_fails();
    if(pin_fails < 3) {
        return 0;
    }
    return 30 * pow(2, pin_fails - 3);
}

void desktop_pin_lock(DesktopSettings* settings) {
    furry_assert(settings);

    furry_hal_rtc_set_pin_fails(0);
    furry_hal_rtc_set_flag(FurryHalRtcFlagLock);
    Cli* cli = furry_record_open(RECORD_CLI);
    cli_session_close(cli);
    furry_record_close(RECORD_CLI);
    settings->is_locked = 1;
    DESKTOP_SETTINGS_SAVE(settings);
}

void desktop_pin_unlock(DesktopSettings* settings) {
    furry_assert(settings);

    furry_hal_rtc_reset_flag(FurryHalRtcFlagLock);
    Cli* cli = furry_record_open(RECORD_CLI);
    cli_session_open(cli, &cli_vcp);
    furry_record_close(RECORD_CLI);
    settings->is_locked = 0;
    DESKTOP_SETTINGS_SAVE(settings);
}

void desktop_pin_lock_init(DesktopSettings* settings) {
    furry_assert(settings);

    if(settings->pin_code.length > 0) {
        if(settings->is_locked == 1) {
            furry_hal_rtc_set_flag(FurryHalRtcFlagLock);
        } else {
            if(desktop_pin_lock_is_locked()) {
                settings->is_locked = 1;
                DESKTOP_SETTINGS_SAVE(settings);
            }
        }
    } else {
        furry_hal_rtc_set_pin_fails(0);
        furry_hal_rtc_reset_flag(FurryHalRtcFlagLock);
    }

    if(desktop_pin_lock_is_locked()) {
        Cli* cli = furry_record_open(RECORD_CLI);
        cli_session_close(cli);
        furry_record_close(RECORD_CLI);
    }
}

bool desktop_pin_lock_verify(const PinCode* pin_set, const PinCode* pin_entered) {
    bool result = false;
    if(desktop_pins_are_equal(pin_set, pin_entered)) {
        furry_hal_rtc_set_pin_fails(0);
        result = true;
    } else {
        uint32_t pin_fails = furry_hal_rtc_get_pin_fails();
        if(pin_fails >= 9 && XTREME_SETTINGS()->bad_pins_format) {
            furry_hal_rtc_set_pin_fails(0);
            furry_hal_rtc_set_flag(FurryHalRtcFlagFactoryReset);
            Storage* storage = furry_record_open(RECORD_STORAGE);
            storage_simply_remove(storage, INT_PATH(".cnt.u2f"));
            storage_simply_remove(storage, INT_PATH(".key.u2f"));
            storage_sd_format(storage);
            furry_record_close(RECORD_STORAGE);
            power_reboot(PowerBootModeNormal);
        }
        furry_hal_rtc_set_pin_fails(pin_fails + 1);
        result = false;
    }
    return result;
}

bool desktop_pin_lock_is_locked() {
    return furry_hal_rtc_is_flag_set(FurryHalRtcFlagLock);
}

bool desktop_pins_are_equal(const PinCode* pin_code1, const PinCode* pin_code2) {
    furry_assert(pin_code1);
    furry_assert(pin_code2);
    bool result = false;

    if(pin_code1->length == pin_code2->length) {
        result = !memcmp(pin_code1->data, pin_code2->data, pin_code1->length);
    }

    return result;
}
