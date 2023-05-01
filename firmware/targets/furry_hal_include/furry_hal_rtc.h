/**
 * @file furry_hal_rtc.h
 * Furry Hal RTC API
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Time
    uint8_t hour; /**< Hour in 24H format: 0-23 */
    uint8_t minute; /**< Minute: 0-59 */
    uint8_t second; /**< Second: 0-59 */
    // Date
    uint8_t day; /**< Current day: 1-31 */
    uint8_t month; /**< Current month: 1-12 */
    uint16_t year; /**< Current year: 2000-2099 */
    uint8_t weekday; /**< Current weekday: 1-7 */
} FurryHalRtcDateTime;

typedef enum {
    FurryHalRtcFlagDebug = (1 << 0),
    FurryHalRtcFlagFactoryReset = (1 << 1),
    FurryHalRtcFlagLock = (1 << 2),
    FurryHalRtcFlagC2Update = (1 << 3),
    FurryHalRtcFlagHandOrient = (1 << 4),
    FurryHalRtcFlagResetPin = (1 << 5),
    FurryHalRtcFlagLegacySleep = (1 << 6),
    FurryHalRtcFlagStealthMode = (1 << 7),
} FurryHalRtcFlag;

typedef enum {
    FurryHalRtcBootModeNormal = 0, /**< Normal boot mode, default value */
    FurryHalRtcBootModeDfu, /**< Boot to DFU (MCU bootloader by ST) */
    FurryHalRtcBootModePreUpdate, /**< Boot to Update, pre update */
    FurryHalRtcBootModeUpdate, /**< Boot to Update, main */
    FurryHalRtcBootModePostUpdate, /**< Boot to Update, post update */
} FurryHalRtcBootMode;

typedef enum {
    FurryHalRtcHeapTrackModeNone = 0, /**< Disable allocation tracking */
    FurryHalRtcHeapTrackModeMain, /**< Enable allocation tracking for main application thread */
    FurryHalRtcHeapTrackModeTree, /**< Enable allocation tracking for main and children application threads */
    FurryHalRtcHeapTrackModeAll, /**< Enable allocation tracking for all threads */
} FurryHalRtcHeapTrackMode;

typedef enum {
    FurryHalRtcRegisterHeader, /**< RTC structure header */
    FurryHalRtcRegisterSystem, /**< Various system bits */
    FurryHalRtcRegisterVersion, /**< Pointer to Version */
    FurryHalRtcRegisterLfsFingerprint, /**< LFS geometry fingerprint */
    FurryHalRtcRegisterFaultData, /**< Pointer to last fault message */
    FurryHalRtcRegisterPinFails, /**< Failed pins count */
    /* Index of FS directory entry corresponding to FW update to be applied */
    FurryHalRtcRegisterUpdateFolderFSIndex,

    FurryHalRtcRegisterMAX, /**< Service value, do not use */
} FurryHalRtcRegister;

typedef enum {
    FurryHalRtcLocaleUnitsMetric = 0, /**< Metric measurement units */
    FurryHalRtcLocaleUnitsImperial = 1, /**< Imperial measurement units */
} FurryHalRtcLocaleUnits;

typedef enum {
    FurryHalRtcLocaleTimeFormat24h = 0, /**< 24-hour format */
    FurryHalRtcLocaleTimeFormat12h = 1, /**< 12-hour format */
} FurryHalRtcLocaleTimeFormat;

typedef enum {
    FurryHalRtcLocaleDateFormatDMY = 0, /**< Day/Month/Year */
    FurryHalRtcLocaleDateFormatMDY = 1, /**< Month/Day/Year */
    FurryHalRtcLocaleDateFormatYMD = 2, /**< Year/Month/Day */
} FurryHalRtcLocaleDateFormat;

/** Early initialization */
void furry_hal_rtc_init_early();

/** Early de-initialization */
void furry_hal_rtc_deinit_early();

/** Initialize RTC subsystem */
void furry_hal_rtc_init();

/** Force sync shadow registers */
void furry_hal_rtc_sync_shadow();

/** Get RTC register content
 *
 * @param[in]  reg   The register identifier
 *
 * @return     content of the register
 */
uint32_t furry_hal_rtc_get_register(FurryHalRtcRegister reg);

/** Set register content
 *
 * @param[in]  reg    The register identifier
 * @param[in]  value  The value to store into register
 */
void furry_hal_rtc_set_register(FurryHalRtcRegister reg, uint32_t value);

/** Set Log Level value
 *
 * @param[in]  level  The level to store
 */
void furry_hal_rtc_set_log_level(uint8_t level);

/** Get Log Level value
 *
 * @return     The Log Level value
 */
uint8_t furry_hal_rtc_get_log_level();

/** Set RTC Flag
 *
 * @param[in]  flag  The flag to set
 */
void furry_hal_rtc_set_flag(FurryHalRtcFlag flag);

/** Reset RTC Flag
 *
 * @param[in]  flag  The flag to reset
 */
void furry_hal_rtc_reset_flag(FurryHalRtcFlag flag);

/** Check if RTC Flag is set
 *
 * @param[in]  flag  The flag to check
 *
 * @return     true if set
 */
bool furry_hal_rtc_is_flag_set(FurryHalRtcFlag flag);

/** Set RTC boot mode
 *
 * @param[in]  mode  The mode to set
 */
void furry_hal_rtc_set_boot_mode(FurryHalRtcBootMode mode);

/** Get RTC boot mode
 *
 * @return     The RTC boot mode.
 */
FurryHalRtcBootMode furry_hal_rtc_get_boot_mode();

/** Set Heap Track mode
 *
 * @param[in]  mode  The mode to set
 */
void furry_hal_rtc_set_heap_track_mode(FurryHalRtcHeapTrackMode mode);

/** Get RTC Heap Track mode
 *
 * @return     The RTC heap track mode.
 */
FurryHalRtcHeapTrackMode furry_hal_rtc_get_heap_track_mode();

/** Set locale units
 *
 * @param[in]  mode  The RTC Locale Units
 */
void furry_hal_rtc_set_locale_units(FurryHalRtcLocaleUnits value);

/** Get RTC Locale Units
 *
 * @return     The RTC Locale Units.
 */
FurryHalRtcLocaleUnits furry_hal_rtc_get_locale_units();

/** Set RTC Locale Time Format
 *
 * @param[in]  value  The RTC Locale Time Format
 */
void furry_hal_rtc_set_locale_timeformat(FurryHalRtcLocaleTimeFormat value);

/** Get RTC Locale Time Format
 *
 * @return     The RTC Locale Time Format.
 */
FurryHalRtcLocaleTimeFormat furry_hal_rtc_get_locale_timeformat();

/** Set RTC Locale Date Format
 *
 * @param[in]  value  The RTC Locale Date Format
 */
void furry_hal_rtc_set_locale_dateformat(FurryHalRtcLocaleDateFormat value);

/** Get RTC Locale Date Format
 *
 * @return     The RTC Locale Date Format
 */
FurryHalRtcLocaleDateFormat furry_hal_rtc_get_locale_dateformat();

/** Set RTC Date Time
 *
 * @param      datetime  The date time to set
 */
void furry_hal_rtc_set_datetime(FurryHalRtcDateTime* datetime);

/** Get RTC Date Time
 *
 * @param      datetime  The datetime
 */
void furry_hal_rtc_get_datetime(FurryHalRtcDateTime* datetime);

/** Validate Date Time
 *
 * @param      datetime  The datetime to validate
 *
 * @return     { description_of_the_return_value }
 */
bool furry_hal_rtc_validate_datetime(FurryHalRtcDateTime* datetime);

/** Set RTC Fault Data
 *
 * @param[in]  value  The value
 */
void furry_hal_rtc_set_fault_data(uint32_t value);

/** Get RTC Fault Data
 *
 * @return     RTC Fault Data value
 */
uint32_t furry_hal_rtc_get_fault_data();

/** Set Pin Fails count
 *
 * @param[in]  value  The Pin Fails count
 */
void furry_hal_rtc_set_pin_fails(uint32_t value);

/** Get Pin Fails count
 *
 * @return     Pin Fails Count
 */
uint32_t furry_hal_rtc_get_pin_fails();

/** Get UNIX Timestamp
 *
 * @return     Unix Timestamp in seconds from UNIX epoch start
 */
uint32_t furry_hal_rtc_get_timestamp();

/** Convert DateTime to UNIX timestamp
 *
 * @param      datetime  The datetime
 *
 * @return     UNIX Timestamp in seconds from UNIX epoch start
 */
uint32_t furry_hal_rtc_datetime_to_timestamp(FurryHalRtcDateTime* datetime);

#ifdef __cplusplus
}
#endif
