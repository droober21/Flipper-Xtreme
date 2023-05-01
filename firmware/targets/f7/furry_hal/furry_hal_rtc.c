#include <furry_hal_rtc.h>
#include <furry_hal_light.h>
#include <furry_hal_debug.h>

#include <stm32wbxx_ll_bus.h>
#include <stm32wbxx_ll_pwr.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_rtc.h>
#include <stm32wbxx_ll_utils.h>

#include <furry.h>

#define TAG "FurryHalRtc"

#define FURRY_HAL_RTC_LSE_STARTUP_TIME 300

#define FURRY_HAL_RTC_CLOCK_IS_READY() (LL_RCC_LSE_IsReady() && LL_RCC_LSI1_IsReady())

#define FURRY_HAL_RTC_HEADER_MAGIC 0x10F1
#define FURRY_HAL_RTC_HEADER_VERSION 0

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t unused;
} FurryHalRtcHeader;

typedef struct {
    uint8_t log_level : 4;
    uint8_t log_reserved : 4;
    uint8_t flags;
    FurryHalRtcBootMode boot_mode : 4;
    FurryHalRtcHeapTrackMode heap_track_mode : 2;
    FurryHalRtcLocaleUnits locale_units : 1;
    FurryHalRtcLocaleTimeFormat locale_timeformat : 1;
    FurryHalRtcLocaleDateFormat locale_dateformat : 2;
    uint8_t reserved : 6;
} SystemReg;

_Static_assert(sizeof(SystemReg) == 4, "SystemReg size mismatch");

#define FURRY_HAL_RTC_SECONDS_PER_MINUTE 60
#define FURRY_HAL_RTC_SECONDS_PER_HOUR (FURRY_HAL_RTC_SECONDS_PER_MINUTE * 60)
#define FURRY_HAL_RTC_SECONDS_PER_DAY (FURRY_HAL_RTC_SECONDS_PER_HOUR * 24)
#define FURRY_HAL_RTC_MONTHS_COUNT 12
#define FURRY_HAL_RTC_EPOCH_START_YEAR 1970
#define FURRY_HAL_RTC_IS_LEAP_YEAR(year) \
    ((((year) % 4 == 0) && ((year) % 100 != 0)) || ((year) % 400 == 0))

static const uint8_t furry_hal_rtc_days_per_month[][FURRY_HAL_RTC_MONTHS_COUNT] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

static const uint16_t furry_hal_rtc_days_per_year[] = {365, 366};

static void furry_hal_rtc_reset() {
    LL_RCC_ForceBackupDomainReset();
    LL_RCC_ReleaseBackupDomainReset();
}

static bool furry_hal_rtc_start_clock_and_switch() {
    // Clock operation require access to Backup Domain
    LL_PWR_EnableBkUpAccess();

    // Enable LSI and LSE
    LL_RCC_LSI1_Enable();
    LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_HIGH);
    LL_RCC_LSE_Enable();

    // Wait for LSI and LSE startup
    uint32_t c = 0;
    while(!FURRY_HAL_RTC_CLOCK_IS_READY() && c < FURRY_HAL_RTC_LSE_STARTUP_TIME) {
        LL_mDelay(1);
        c++;
    }

    if(FURRY_HAL_RTC_CLOCK_IS_READY()) {
        LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
        LL_RCC_EnableRTC();
        return LL_RCC_GetRTCClockSource() == LL_RCC_RTC_CLKSOURCE_LSE;
    } else {
        return false;
    }
}

static void furry_hal_rtc_recover() {
    FurryHalRtcDateTime datetime = {0};

    // Handle fixable LSE failure
    if(LL_RCC_LSE_IsCSSDetected()) {
        furry_hal_light_sequence("rgb B");
        // Shutdown LSE and LSECSS
        LL_RCC_LSE_DisableCSS();
        LL_RCC_LSE_Disable();
    } else {
        furry_hal_light_sequence("rgb R");
    }

    // Temporary switch to LSI
    LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
    if(LL_RCC_GetRTCClockSource() == LL_RCC_RTC_CLKSOURCE_LSI) {
        // Get datetime before RTC Domain reset
        furry_hal_rtc_get_datetime(&datetime);
    }

    // Reset RTC Domain
    furry_hal_rtc_reset();

    // Start Clock
    if(!furry_hal_rtc_start_clock_and_switch()) {
        // Plan C: reset RTC and restart
        furry_hal_light_sequence("rgb R.r.R.r.R.r");
        furry_hal_rtc_reset();
        NVIC_SystemReset();
    }

    // Set date if it valid
    if(datetime.year != 0) {
        furry_hal_rtc_set_datetime(&datetime);
    }
}

void furry_hal_rtc_init_early() {
    // Enable RTCAPB clock
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTCAPB);

    // Prepare clock
    if(!furry_hal_rtc_start_clock_and_switch()) {
        // Plan B: try to recover
        furry_hal_rtc_recover();
    }

    // Verify header register
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterHeader);
    FurryHalRtcHeader* data = (FurryHalRtcHeader*)&data_reg;
    if(data->magic != FURRY_HAL_RTC_HEADER_MAGIC || data->version != FURRY_HAL_RTC_HEADER_VERSION) {
        // Reset all our registers to ensure consistency
        for(size_t i = 0; i < FurryHalRtcRegisterMAX; i++) {
            furry_hal_rtc_set_register(i, 0);
        }
        data->magic = FURRY_HAL_RTC_HEADER_MAGIC;
        data->version = FURRY_HAL_RTC_HEADER_VERSION;
        furry_hal_rtc_set_register(FurryHalRtcRegisterHeader, data_reg);
    }

    if(furry_hal_rtc_is_flag_set(FurryHalRtcFlagDebug)) {
        furry_hal_debug_enable();
    } else {
        furry_hal_debug_disable();
    }
}

void furry_hal_rtc_deinit_early() {
}

void furry_hal_rtc_init() {
    LL_RTC_InitTypeDef RTC_InitStruct;
    RTC_InitStruct.HourFormat = LL_RTC_HOURFORMAT_24HOUR;
    RTC_InitStruct.AsynchPrescaler = 127;
    RTC_InitStruct.SynchPrescaler = 255;
    LL_RTC_Init(RTC, &RTC_InitStruct);

    furry_log_set_level(furry_hal_rtc_get_log_level());

    FURRY_LOG_I(TAG, "Init OK");
}

void furry_hal_rtc_sync_shadow() {
    if(!LL_RTC_IsShadowRegBypassEnabled(RTC)) {
        LL_RTC_ClearFlag_RS(RTC);
        while(!LL_RTC_IsActiveFlag_RS(RTC)) {
        };
    }
}

uint32_t furry_hal_rtc_get_register(FurryHalRtcRegister reg) {
    return LL_RTC_BAK_GetRegister(RTC, reg);
}

void furry_hal_rtc_set_register(FurryHalRtcRegister reg, uint32_t value) {
    LL_RTC_BAK_SetRegister(RTC, reg, value);
}

void furry_hal_rtc_set_log_level(uint8_t level) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->log_level = level;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);
    furry_log_set_level(level);
}

uint8_t furry_hal_rtc_get_log_level() {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->log_level;
}

void furry_hal_rtc_set_flag(FurryHalRtcFlag flag) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->flags |= flag;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);

    if(flag & FurryHalRtcFlagDebug) {
        furry_hal_debug_enable();
    }
}

void furry_hal_rtc_reset_flag(FurryHalRtcFlag flag) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->flags &= ~flag;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);

    if(flag & FurryHalRtcFlagDebug) {
        furry_hal_debug_disable();
    }
}

bool furry_hal_rtc_is_flag_set(FurryHalRtcFlag flag) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->flags & flag;
}

void furry_hal_rtc_set_boot_mode(FurryHalRtcBootMode mode) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->boot_mode = mode;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);
}

FurryHalRtcBootMode furry_hal_rtc_get_boot_mode() {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->boot_mode;
}

void furry_hal_rtc_set_heap_track_mode(FurryHalRtcHeapTrackMode mode) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->heap_track_mode = mode;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);
}

FurryHalRtcHeapTrackMode furry_hal_rtc_get_heap_track_mode() {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->heap_track_mode;
}

void furry_hal_rtc_set_locale_units(FurryHalRtcLocaleUnits value) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->locale_units = value;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);
}

FurryHalRtcLocaleUnits furry_hal_rtc_get_locale_units() {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->locale_units;
}

void furry_hal_rtc_set_locale_timeformat(FurryHalRtcLocaleTimeFormat value) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->locale_timeformat = value;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);
}

FurryHalRtcLocaleTimeFormat furry_hal_rtc_get_locale_timeformat() {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->locale_timeformat;
}

void furry_hal_rtc_set_locale_dateformat(FurryHalRtcLocaleDateFormat value) {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    data->locale_dateformat = value;
    furry_hal_rtc_set_register(FurryHalRtcRegisterSystem, data_reg);
}

FurryHalRtcLocaleDateFormat furry_hal_rtc_get_locale_dateformat() {
    uint32_t data_reg = furry_hal_rtc_get_register(FurryHalRtcRegisterSystem);
    SystemReg* data = (SystemReg*)&data_reg;
    return data->locale_dateformat;
}

void furry_hal_rtc_set_datetime(FurryHalRtcDateTime* datetime) {
    furry_check(!FURRY_IS_IRQ_MODE());
    furry_assert(datetime);

    FURRY_CRITICAL_ENTER();
    /* Disable write protection */
    LL_RTC_DisableWriteProtection(RTC);

    /* Enter Initialization mode and wait for INIT flag to be set */
    LL_RTC_EnableInitMode(RTC);
    while(!LL_RTC_IsActiveFlag_INIT(RTC)) {
    }

    /* Set time */
    LL_RTC_TIME_Config(
        RTC,
        LL_RTC_TIME_FORMAT_AM_OR_24,
        __LL_RTC_CONVERT_BIN2BCD(datetime->hour),
        __LL_RTC_CONVERT_BIN2BCD(datetime->minute),
        __LL_RTC_CONVERT_BIN2BCD(datetime->second));

    /* Set date */
    LL_RTC_DATE_Config(
        RTC,
        datetime->weekday,
        __LL_RTC_CONVERT_BIN2BCD(datetime->day),
        __LL_RTC_CONVERT_BIN2BCD(datetime->month),
        __LL_RTC_CONVERT_BIN2BCD(datetime->year - 2000));

    /* Exit Initialization mode */
    LL_RTC_DisableInitMode(RTC);

    furry_hal_rtc_sync_shadow();

    /* Enable write protection */
    LL_RTC_EnableWriteProtection(RTC);
    FURRY_CRITICAL_EXIT();
}

void furry_hal_rtc_get_datetime(FurryHalRtcDateTime* datetime) {
    furry_check(!FURRY_IS_IRQ_MODE());
    furry_assert(datetime);

    FURRY_CRITICAL_ENTER();
    uint32_t time = LL_RTC_TIME_Get(RTC); // 0x00HHMMSS
    uint32_t date = LL_RTC_DATE_Get(RTC); // 0xWWDDMMYY
    FURRY_CRITICAL_EXIT();

    datetime->second = __LL_RTC_CONVERT_BCD2BIN((time >> 0) & 0xFF);
    datetime->minute = __LL_RTC_CONVERT_BCD2BIN((time >> 8) & 0xFF);
    datetime->hour = __LL_RTC_CONVERT_BCD2BIN((time >> 16) & 0xFF);
    datetime->year = __LL_RTC_CONVERT_BCD2BIN((date >> 0) & 0xFF) + 2000;
    datetime->month = __LL_RTC_CONVERT_BCD2BIN((date >> 8) & 0xFF);
    datetime->day = __LL_RTC_CONVERT_BCD2BIN((date >> 16) & 0xFF);
    datetime->weekday = __LL_RTC_CONVERT_BCD2BIN((date >> 24) & 0xFF);
}

bool furry_hal_rtc_validate_datetime(FurryHalRtcDateTime* datetime) {
    bool invalid = false;

    invalid |= (datetime->second > 59);
    invalid |= (datetime->minute > 59);
    invalid |= (datetime->hour > 23);

    invalid |= (datetime->year < 2000);
    invalid |= (datetime->year > 2099);

    invalid |= (datetime->month == 0);
    invalid |= (datetime->month > 12);

    invalid |= (datetime->day == 0);
    invalid |= (datetime->day > 31);

    invalid |= (datetime->weekday == 0);
    invalid |= (datetime->weekday > 7);

    return !invalid;
}

void furry_hal_rtc_set_fault_data(uint32_t value) {
    furry_hal_rtc_set_register(FurryHalRtcRegisterFaultData, value);
}

uint32_t furry_hal_rtc_get_fault_data() {
    return furry_hal_rtc_get_register(FurryHalRtcRegisterFaultData);
}

void furry_hal_rtc_set_pin_fails(uint32_t value) {
    furry_hal_rtc_set_register(FurryHalRtcRegisterPinFails, value);
}

uint32_t furry_hal_rtc_get_pin_fails() {
    return furry_hal_rtc_get_register(FurryHalRtcRegisterPinFails);
}

uint32_t furry_hal_rtc_get_timestamp() {
    FurryHalRtcDateTime datetime = {0};
    furry_hal_rtc_get_datetime(&datetime);
    return furry_hal_rtc_datetime_to_timestamp(&datetime);
}

uint32_t furry_hal_rtc_datetime_to_timestamp(FurryHalRtcDateTime* datetime) {
    uint32_t timestamp = 0;
    uint8_t years = 0;
    uint8_t leap_years = 0;

    for(uint16_t y = FURRY_HAL_RTC_EPOCH_START_YEAR; y < datetime->year; y++) {
        if(FURRY_HAL_RTC_IS_LEAP_YEAR(y)) {
            leap_years++;
        } else {
            years++;
        }
    }

    timestamp +=
        ((years * furry_hal_rtc_days_per_year[0]) + (leap_years * furry_hal_rtc_days_per_year[1])) *
        FURRY_HAL_RTC_SECONDS_PER_DAY;

    uint8_t year_index = (FURRY_HAL_RTC_IS_LEAP_YEAR(datetime->year)) ? 1 : 0;

    for(uint8_t m = 0; m < (datetime->month - 1); m++) {
        timestamp += furry_hal_rtc_days_per_month[year_index][m] * FURRY_HAL_RTC_SECONDS_PER_DAY;
    }

    timestamp += (datetime->day - 1) * FURRY_HAL_RTC_SECONDS_PER_DAY;
    timestamp += datetime->hour * FURRY_HAL_RTC_SECONDS_PER_HOUR;
    timestamp += datetime->minute * FURRY_HAL_RTC_SECONDS_PER_MINUTE;
    timestamp += datetime->second;

    return timestamp;
}
