#include "locale.h"

#define TAG "LocaleSrv"

LocaleMeasurementUnits locale_get_measurement_unit(void) {
    return (LocaleMeasurementUnits)furry_hal_rtc_get_locale_units();
}

void locale_set_measurement_unit(LocaleMeasurementUnits format) {
    furry_hal_rtc_set_locale_units((FurryHalRtcLocaleUnits)format);
}

LocaleTimeFormat locale_get_time_format(void) {
    return (LocaleTimeFormat)furry_hal_rtc_get_locale_timeformat();
}

void locale_set_time_format(LocaleTimeFormat format) {
    furry_hal_rtc_set_locale_timeformat((FurryHalRtcLocaleTimeFormat)format);
}

LocaleDateFormat locale_get_date_format(void) {
    return (LocaleDateFormat)furry_hal_rtc_get_locale_dateformat();
}

void locale_set_date_format(LocaleDateFormat format) {
    furry_hal_rtc_set_locale_dateformat((FurryHalRtcLocaleDateFormat)format);
}

float locale_fahrenheit_to_celsius(float temp_f) {
    return (temp_f - 32.f) / 1.8f;
}

float locale_celsius_to_fahrenheit(float temp_c) {
    return (temp_c * 1.8f + 32.f);
}

void locale_format_time(
    FurryString* out_str,
    const FurryHalRtcDateTime* datetime,
    const LocaleTimeFormat format,
    const bool show_seconds) {
    furry_assert(out_str);
    furry_assert(datetime);

    uint8_t hours = datetime->hour;
    uint8_t am_pm = 0;
    if(format == LocaleTimeFormat12h) {
        if(hours > 12) {
            hours -= 12;
            am_pm = 2;
        } else {
            am_pm = 1;
        }
        if(hours == 0) {
            hours = 12;
        }
    }

    if(show_seconds) {
        furry_string_printf(out_str, "%02u:%02u:%02u", hours, datetime->minute, datetime->second);
    } else {
        furry_string_printf(out_str, "%02u:%02u", hours, datetime->minute);
    }

    if(am_pm > 0) {
        furry_string_cat_printf(out_str, " %s", (am_pm == 1) ? ("AM") : ("PM"));
    }
}

void locale_format_date(
    FurryString* out_str,
    const FurryHalRtcDateTime* datetime,
    const LocaleDateFormat format,
    const char* separator) {
    furry_assert(out_str);
    furry_assert(datetime);
    furry_assert(separator);

    if(format == LocaleDateFormatDMY) {
        furry_string_printf(
            out_str,
            "%02u%s%02u%s%04u",
            datetime->day,
            separator,
            datetime->month,
            separator,
            datetime->year);
    } else if(format == LocaleDateFormatMDY) {
        furry_string_printf(
            out_str,
            "%02u%s%02u%s%04u",
            datetime->month,
            separator,
            datetime->day,
            separator,
            datetime->year);
    } else {
        furry_string_printf(
            out_str,
            "%04u%s%02u%s%02u",
            datetime->year,
            separator,
            datetime->month,
            separator,
            datetime->day);
    }
}

int32_t locale_on_system_start(void* p) {
    UNUSED(p);
    return 0;
}
