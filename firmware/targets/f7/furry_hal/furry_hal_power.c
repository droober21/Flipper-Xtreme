#include <furry_hal_power.h>
#include <furry_hal_clock.h>
#include <furry_hal_bt.h>
#include <furry_hal_vibro.h>
#include <furry_hal_resources.h>
#include <furry_hal_uart.h>
#include <furry_hal_rtc.h>
#include <furry_hal_debug.h>

#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_pwr.h>
#include <stm32wbxx_ll_hsem.h>
#include <stm32wbxx_ll_cortex.h>
#include <stm32wbxx_ll_gpio.h>

#include <hw_conf.h>
#include <bq27220.h>
#include <bq25896.h>

#include <furry.h>

#define TAG "FurryHalPower"

#ifndef FURRY_HAL_POWER_DEBUG_WFI_GPIO
#define FURRY_HAL_POWER_DEBUG_WFI_GPIO (&gpio_ext_pb2)
#endif

#ifndef FURRY_HAL_POWER_DEBUG_STOP_GPIO
#define FURRY_HAL_POWER_DEBUG_STOP_GPIO (&gpio_ext_pc3)
#endif

#ifndef FURRY_HAL_POWER_STOP_MODE
#define FURRY_HAL_POWER_STOP_MODE (LL_PWR_MODE_STOP2)
#endif

typedef struct {
    volatile uint8_t insomnia;
    volatile uint8_t suppress_charge;

    uint8_t gauge_initialized;
    uint8_t charger_initialized;
} FurryHalPower;

static volatile FurryHalPower furry_hal_power = {
    .insomnia = 0,
    .suppress_charge = 0,
};

const ParamCEDV cedv = {
    .cedv_conf.gauge_conf =
        {
            .CCT = 1,
            .CSYNC = 0,
            .EDV_CMP = 0,
            .SC = 1,
            .FIXED_EDV0 = 1,
            .FCC_LIM = 1,
            .FC_FOR_VDQ = 1,
            .IGNORE_SD = 1,
            .SME0 = 0,
        },
    .full_charge_cap = 2101,
    .design_cap = 2101,
    .EDV0 = 3300,
    .EDV1 = 3321,
    .EDV2 = 3355,
    .EMF = 3679,
    .C0 = 430,
    .C1 = 0,
    .R1 = 408,
    .R0 = 334,
    .T0 = 4626,
    .TC = 11,
    .DOD0 = 4044,
    .DOD10 = 3905,
    .DOD20 = 3807,
    .DOD30 = 3718,
    .DOD40 = 3642,
    .DOD50 = 3585,
    .DOD60 = 3546,
    .DOD70 = 3514,
    .DOD80 = 3477,
    .DOD90 = 3411,
    .DOD100 = 3299,
};

void furry_hal_power_init() {
#ifdef FURRY_HAL_POWER_DEBUG
    furry_hal_gpio_init_simple(FURRY_HAL_POWER_DEBUG_WFI_GPIO, GpioModeOutputPushPull);
    furry_hal_gpio_init_simple(FURRY_HAL_POWER_DEBUG_STOP_GPIO, GpioModeOutputPushPull);
    furry_hal_gpio_write(FURRY_HAL_POWER_DEBUG_WFI_GPIO, 0);
    furry_hal_gpio_write(FURRY_HAL_POWER_DEBUG_STOP_GPIO, 0);
#endif

    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_PWR_SMPS_SetMode(LL_PWR_SMPS_STEP_DOWN);

    LL_PWR_SetPowerMode(FURRY_HAL_POWER_STOP_MODE);
    LL_C2_PWR_SetPowerMode(FURRY_HAL_POWER_STOP_MODE);

    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bq27220_init(&furry_hal_i2c_handle_power, &cedv);
    bq25896_init(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);

    FURRY_LOG_I(TAG, "Init OK");
}

bool furry_hal_power_gauge_is_ok() {
    bool ret = true;

    BatteryStatus battery_status;
    OperationStatus operation_status;

    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);

    if(bq27220_get_battery_status(&furry_hal_i2c_handle_power, &battery_status) == BQ27220_ERROR ||
       bq27220_get_operation_status(&furry_hal_i2c_handle_power, &operation_status) ==
           BQ27220_ERROR) {
        ret = false;
    } else {
        ret &= battery_status.BATTPRES;
        ret &= operation_status.INITCOMP;
        ret &= (cedv.design_cap == bq27220_get_design_capacity(&furry_hal_i2c_handle_power));
    }

    furry_hal_i2c_release(&furry_hal_i2c_handle_power);

    return ret;
}

uint16_t furry_hal_power_insomnia_level() {
    return furry_hal_power.insomnia;
}

void furry_hal_power_insomnia_enter() {
    FURRY_CRITICAL_ENTER();
    furry_assert(furry_hal_power.insomnia < UINT8_MAX);
    furry_hal_power.insomnia++;
    FURRY_CRITICAL_EXIT();
}

void furry_hal_power_insomnia_exit() {
    FURRY_CRITICAL_ENTER();
    furry_assert(furry_hal_power.insomnia > 0);
    furry_hal_power.insomnia--;
    FURRY_CRITICAL_EXIT();
}

bool furry_hal_power_sleep_available() {
    return furry_hal_power.insomnia == 0;
}

static inline bool furry_hal_power_deep_sleep_available() {
    return furry_hal_bt_is_alive() && !furry_hal_rtc_is_flag_set(FurryHalRtcFlagLegacySleep) &&
           !furry_hal_debug_is_gdb_session_active();
}

static inline void furry_hal_power_light_sleep() {
    __WFI();
}

static inline void furry_hal_power_suspend_aux_periphs() {
    // Disable USART
    furry_hal_uart_suspend(FurryHalUartIdUSART1);
    furry_hal_uart_suspend(FurryHalUartIdLPUART1);
}

static inline void furry_hal_power_resume_aux_periphs() {
    // Re-enable USART
    furry_hal_uart_resume(FurryHalUartIdUSART1);
    furry_hal_uart_resume(FurryHalUartIdLPUART1);
}

static inline void furry_hal_power_deep_sleep() {
    furry_hal_power_suspend_aux_periphs();

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID))
        ;

    if(!LL_HSEM_1StepLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID)) {
        if(LL_PWR_IsActiveFlag_C2DS() || LL_PWR_IsActiveFlag_C2SB()) {
            // Release ENTRY_STOP_MODE semaphore
            LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);

            // The switch on HSI before entering Stop Mode is required
            furry_hal_clock_switch_to_hsi();
        }
    } else {
        /**
         * The switch on HSI before entering Stop Mode is required 
         */
        furry_hal_clock_switch_to_hsi();
    }

    /* Release RCC semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);

    // Prepare deep sleep
    LL_LPM_EnableDeepSleep();

#if defined(__CC_ARM)
    // Force store operations
    __force_stores();
#endif

    __WFI();

    LL_LPM_EnableSleep();

    /* Release ENTRY_STOP_MODE semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID))
        ;

    if(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
        furry_hal_clock_switch_to_pll();
    }

    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);

    furry_hal_power_resume_aux_periphs();
    furry_hal_rtc_sync_shadow();
}

void furry_hal_power_sleep() {
    if(furry_hal_power_deep_sleep_available()) {
#ifdef FURRY_HAL_POWER_DEBUG
        furry_hal_gpio_write(FURRY_HAL_POWER_DEBUG_STOP_GPIO, 1);
#endif
        furry_hal_power_deep_sleep();
#ifdef FURRY_HAL_POWER_DEBUG
        furry_hal_gpio_write(FURRY_HAL_POWER_DEBUG_STOP_GPIO, 0);
#endif
    } else {
#ifdef FURRY_HAL_POWER_DEBUG
        furry_hal_gpio_write(FURRY_HAL_POWER_DEBUG_WFI_GPIO, 1);
#endif
        furry_hal_power_light_sleep();
#ifdef FURRY_HAL_POWER_DEBUG
        furry_hal_gpio_write(FURRY_HAL_POWER_DEBUG_WFI_GPIO, 0);
#endif
    }
}

uint8_t furry_hal_power_get_pct() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    uint8_t ret = bq27220_get_state_of_charge(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

uint8_t furry_hal_power_get_bat_health_pct() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    uint8_t ret = bq27220_get_state_of_health(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

bool furry_hal_power_is_charging() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bool ret = bq25896_is_charging(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

bool furry_hal_power_is_charging_done() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bool ret = bq25896_is_charging_done(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

void furry_hal_power_shutdown() {
    furry_hal_power_insomnia_enter();

    furry_hal_bt_reinit();

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID))
        ;

    if(!LL_HSEM_1StepLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID)) {
        if(LL_PWR_IsActiveFlag_C2DS() || LL_PWR_IsActiveFlag_C2SB()) {
            // Release ENTRY_STOP_MODE semaphore
            LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);
        }
    }

    // Prepare Wakeup pin
    LL_PWR_SetWakeUpPinPolarityLow(LL_PWR_WAKEUP_PIN2);
    LL_PWR_EnableWakeUpPin(LL_PWR_WAKEUP_PIN2);
    LL_C2_PWR_EnableWakeUpPin(LL_PWR_WAKEUP_PIN2);

    /* Release RCC semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);

    LL_PWR_DisableBootC2();
    LL_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
    LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
    LL_LPM_EnableDeepSleep();

    __WFI();
    furry_crash("Insomniac core2");
}

void furry_hal_power_off() {
    // Crutch: shutting down with ext 3V3 off is causing LSE to stop
    furry_hal_power_enable_external_3_3v();
    furry_hal_vibro_on(true);
    furry_delay_us(50000);
    // Send poweroff to charger
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bq25896_poweroff(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    furry_hal_vibro_on(false);
}

void furry_hal_power_reset() {
    NVIC_SystemReset();
}

void furry_hal_power_enable_otg() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bq25896_enable_otg(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_power_disable_otg() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bq25896_disable_otg(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

bool furry_hal_power_is_otg_enabled() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    bool ret = bq25896_is_otg_enabled(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

float furry_hal_power_get_battery_charge_voltage_limit() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    float ret = (float)bq25896_get_vreg_voltage(&furry_hal_i2c_handle_power) / 1000.0f;
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

void furry_hal_power_set_battery_charge_voltage_limit(float voltage) {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    // Adding 0.0005 is necessary because 4.016f is 4.015999794000, which gets truncated
    bq25896_set_vreg_voltage(&furry_hal_i2c_handle_power, (uint16_t)(voltage * 1000.0f + 0.0005f));
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_power_check_otg_status() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    if(bq25896_check_otg_fault(&furry_hal_i2c_handle_power))
        bq25896_disable_otg(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

uint32_t furry_hal_power_get_battery_remaining_capacity() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    uint32_t ret = bq27220_get_remaining_capacity(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

uint32_t furry_hal_power_get_battery_full_capacity() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    uint32_t ret = bq27220_get_full_charge_capacity(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

uint32_t furry_hal_power_get_battery_design_capacity() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    uint32_t ret = bq27220_get_design_capacity(&furry_hal_i2c_handle_power);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

float furry_hal_power_get_battery_voltage(FurryHalPowerIC ic) {
    float ret = 0.0f;

    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    if(ic == FurryHalPowerICCharger) {
        ret = (float)bq25896_get_vbat_voltage(&furry_hal_i2c_handle_power) / 1000.0f;
    } else if(ic == FurryHalPowerICFuelGauge) {
        ret = (float)bq27220_get_voltage(&furry_hal_i2c_handle_power) / 1000.0f;
    }
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);

    return ret;
}

float furry_hal_power_get_battery_current(FurryHalPowerIC ic) {
    float ret = 0.0f;

    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    if(ic == FurryHalPowerICCharger) {
        ret = (float)bq25896_get_vbat_current(&furry_hal_i2c_handle_power) / 1000.0f;
    } else if(ic == FurryHalPowerICFuelGauge) {
        ret = (float)bq27220_get_current(&furry_hal_i2c_handle_power) / 1000.0f;
    }
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);

    return ret;
}

static float furry_hal_power_get_battery_temperature_internal(FurryHalPowerIC ic) {
    float ret = 0.0f;

    if(ic == FurryHalPowerICCharger) {
        // Linear approximation, +/- 5 C
        ret = (71.0f - (float)bq25896_get_ntc_mpct(&furry_hal_i2c_handle_power) / 1000) / 0.6f;
    } else if(ic == FurryHalPowerICFuelGauge) {
        ret = ((float)bq27220_get_temperature(&furry_hal_i2c_handle_power) - 2731.0f) / 10.0f;
    }

    return ret;
}

float furry_hal_power_get_battery_temperature(FurryHalPowerIC ic) {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    float ret = furry_hal_power_get_battery_temperature_internal(ic);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);

    return ret;
}

float furry_hal_power_get_usb_voltage() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    float ret = (float)bq25896_get_vbus_voltage(&furry_hal_i2c_handle_power) / 1000.0f;
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    return ret;
}

void furry_hal_power_enable_external_3_3v() {
    furry_hal_gpio_write(&gpio_periph_power, 1);
}

void furry_hal_power_disable_external_3_3v() {
    furry_hal_gpio_write(&gpio_periph_power, 0);
}

void furry_hal_power_suppress_charge_enter() {
    vTaskSuspendAll();
    bool disable_charging = furry_hal_power.suppress_charge == 0;
    furry_hal_power.suppress_charge++;
    xTaskResumeAll();

    if(disable_charging) {
        furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
        bq25896_disable_charging(&furry_hal_i2c_handle_power);
        furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    }
}

void furry_hal_power_suppress_charge_exit() {
    vTaskSuspendAll();
    furry_hal_power.suppress_charge--;
    bool enable_charging = furry_hal_power.suppress_charge == 0;
    xTaskResumeAll();

    if(enable_charging) {
        furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
        bq25896_enable_charging(&furry_hal_i2c_handle_power);
        furry_hal_i2c_release(&furry_hal_i2c_handle_power);
    }
}

void furry_hal_power_info_get(PropertyValueCallback out, char sep, void* context) {
    furry_assert(out);

    FurryString* value = furry_string_alloc();
    FurryString* key = furry_string_alloc();

    PropertyValueContext property_context = {
        .key = key, .value = value, .out = out, .sep = sep, .last = false, .context = context};

    if(sep == '.') {
        property_value_out(&property_context, NULL, 2, "format", "major", "2");
        property_value_out(&property_context, NULL, 2, "format", "minor", "1");
    } else {
        property_value_out(&property_context, NULL, 3, "power", "info", "major", "2");
        property_value_out(&property_context, NULL, 3, "power", "info", "minor", "1");
    }

    uint8_t charge = furry_hal_power_get_pct();
    property_value_out(&property_context, "%u", 2, "charge", "level", charge);

    const char* charge_state;
    if(furry_hal_power_is_charging()) {
        if((charge < 100) && (!furry_hal_power_is_charging_done())) {
            charge_state = "charging";
        } else {
            charge_state = "charged";
        }
    } else {
        charge_state = "discharging";
    }

    property_value_out(&property_context, NULL, 2, "charge", "state", charge_state);
    uint16_t charge_voltage_limit =
        (uint16_t)(furry_hal_power_get_battery_charge_voltage_limit() * 1000.f);
    property_value_out(
        &property_context, "%u", 3, "charge", "voltage", "limit", charge_voltage_limit);
    uint16_t voltage =
        (uint16_t)(furry_hal_power_get_battery_voltage(FurryHalPowerICFuelGauge) * 1000.f);
    property_value_out(&property_context, "%u", 2, "battery", "voltage", voltage);
    int16_t current =
        (int16_t)(furry_hal_power_get_battery_current(FurryHalPowerICFuelGauge) * 1000.f);
    property_value_out(&property_context, "%d", 2, "battery", "current", current);
    int16_t temperature = (int16_t)furry_hal_power_get_battery_temperature(FurryHalPowerICFuelGauge);
    property_value_out(&property_context, "%d", 2, "battery", "temp", temperature);
    property_value_out(
        &property_context, "%u", 2, "battery", "health", furry_hal_power_get_bat_health_pct());
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "remain",
        furry_hal_power_get_battery_remaining_capacity());
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "full",
        furry_hal_power_get_battery_full_capacity());
    property_context.last = true;
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "design",
        furry_hal_power_get_battery_design_capacity());

    furry_string_free(key);
    furry_string_free(value);
}

void furry_hal_power_debug_get(PropertyValueCallback out, void* context) {
    furry_assert(out);

    FurryString* value = furry_string_alloc();
    FurryString* key = furry_string_alloc();

    PropertyValueContext property_context = {
        .key = key, .value = value, .out = out, .sep = '.', .last = false, .context = context};

    BatteryStatus battery_status;
    OperationStatus operation_status;

    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);

    // Power Debug version
    property_value_out(&property_context, NULL, 2, "format", "major", "1");
    property_value_out(&property_context, NULL, 2, "format", "minor", "0");

    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vbus",
        bq25896_get_vbus_voltage(&furry_hal_i2c_handle_power));
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vsys",
        bq25896_get_vsys_voltage(&furry_hal_i2c_handle_power));
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vbat",
        bq25896_get_vbat_voltage(&furry_hal_i2c_handle_power));
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vreg",
        bq25896_get_vreg_voltage(&furry_hal_i2c_handle_power));
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "current",
        bq25896_get_vbat_current(&furry_hal_i2c_handle_power));

    const uint32_t ntc_mpct = bq25896_get_ntc_mpct(&furry_hal_i2c_handle_power);

    if(bq27220_get_battery_status(&furry_hal_i2c_handle_power, &battery_status) != BQ27220_ERROR &&
       bq27220_get_operation_status(&furry_hal_i2c_handle_power, &operation_status) !=
           BQ27220_ERROR) {
        property_value_out(&property_context, "%lu", 2, "charger", "ntc", ntc_mpct);
        property_value_out(&property_context, "%d", 2, "gauge", "calmd", operation_status.CALMD);
        property_value_out(&property_context, "%d", 2, "gauge", "sec", operation_status.SEC);
        property_value_out(&property_context, "%d", 2, "gauge", "edv2", operation_status.EDV2);
        property_value_out(&property_context, "%d", 2, "gauge", "vdq", operation_status.VDQ);
        property_value_out(
            &property_context, "%d", 2, "gauge", "initcomp", operation_status.INITCOMP);
        property_value_out(&property_context, "%d", 2, "gauge", "smth", operation_status.SMTH);
        property_value_out(&property_context, "%d", 2, "gauge", "btpint", operation_status.BTPINT);
        property_value_out(
            &property_context, "%d", 2, "gauge", "cfgupdate", operation_status.CFGUPDATE);

        // Battery status register, part 1
        property_value_out(&property_context, "%d", 2, "gauge", "chginh", battery_status.CHGINH);
        property_value_out(&property_context, "%d", 2, "gauge", "fc", battery_status.FC);
        property_value_out(&property_context, "%d", 2, "gauge", "otd", battery_status.OTD);
        property_value_out(&property_context, "%d", 2, "gauge", "otc", battery_status.OTC);
        property_value_out(&property_context, "%d", 2, "gauge", "sleep", battery_status.SLEEP);
        property_value_out(&property_context, "%d", 2, "gauge", "ocvfail", battery_status.OCVFAIL);
        property_value_out(&property_context, "%d", 2, "gauge", "ocvcomp", battery_status.OCVCOMP);
        property_value_out(&property_context, "%d", 2, "gauge", "fd", battery_status.FD);

        // Battery status register, part 2
        property_value_out(&property_context, "%d", 2, "gauge", "dsg", battery_status.DSG);
        property_value_out(&property_context, "%d", 2, "gauge", "sysdwn", battery_status.SYSDWN);
        property_value_out(&property_context, "%d", 2, "gauge", "tda", battery_status.TDA);
        property_value_out(
            &property_context, "%d", 2, "gauge", "battpres", battery_status.BATTPRES);
        property_value_out(&property_context, "%d", 2, "gauge", "authgd", battery_status.AUTH_GD);
        property_value_out(&property_context, "%d", 2, "gauge", "ocvgd", battery_status.OCVGD);
        property_value_out(&property_context, "%d", 2, "gauge", "tca", battery_status.TCA);
        property_value_out(&property_context, "%d", 2, "gauge", "rsvd", battery_status.RSVD);

        // Voltage and current info
        property_value_out(
            &property_context,
            "%d",
            3,
            "gauge",
            "capacity",
            "full",
            bq27220_get_full_charge_capacity(&furry_hal_i2c_handle_power));
        property_value_out(
            &property_context,
            "%d",
            3,
            "gauge",
            "capacity",
            "design",
            bq27220_get_design_capacity(&furry_hal_i2c_handle_power));
        property_value_out(
            &property_context,
            "%d",
            3,
            "gauge",
            "capacity",
            "remain",
            bq27220_get_remaining_capacity(&furry_hal_i2c_handle_power));
        property_value_out(
            &property_context,
            "%d",
            3,
            "gauge",
            "state",
            "charge",
            bq27220_get_state_of_charge(&furry_hal_i2c_handle_power));
        property_value_out(
            &property_context,
            "%d",
            3,
            "gauge",
            "state",
            "health",
            bq27220_get_state_of_health(&furry_hal_i2c_handle_power));
        property_value_out(
            &property_context,
            "%d",
            2,
            "gauge",
            "voltage",
            bq27220_get_voltage(&furry_hal_i2c_handle_power));
        property_value_out(
            &property_context,
            "%d",
            2,
            "gauge",
            "current",
            bq27220_get_current(&furry_hal_i2c_handle_power));

        property_context.last = true;
        const int battery_temp =
            (int)furry_hal_power_get_battery_temperature_internal(FurryHalPowerICFuelGauge);
        property_value_out(&property_context, "%d", 2, "gauge", "temperature", battery_temp);
    } else {
        property_context.last = true;
        property_value_out(&property_context, "%lu", 2, "charger", "ntc", ntc_mpct);
    }

    furry_string_free(key);
    furry_string_free(value);

    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}
