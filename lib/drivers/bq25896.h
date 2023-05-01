#pragma once

#include "bq25896_reg.h"

#include <stdbool.h>
#include <stdint.h>
#include <furry_hal_i2c.h>

/** Initialize Driver */
void bq25896_init(FurryHalI2cBusHandle* handle);

/** Send device into shipping mode */
void bq25896_poweroff(FurryHalI2cBusHandle* handle);

/** Get charging status */
ChrgStat bq25896_get_charge_status(FurryHalI2cBusHandle* handle);

/** Is currently charging */
bool bq25896_is_charging(FurryHalI2cBusHandle* handle);

/** Is charging completed while connected to charger */
bool bq25896_is_charging_done(FurryHalI2cBusHandle* handle);

/** Enable charging */
void bq25896_enable_charging(FurryHalI2cBusHandle* handle);

/** Disable charging */
void bq25896_disable_charging(FurryHalI2cBusHandle* handle);

/** Enable otg */
void bq25896_enable_otg(FurryHalI2cBusHandle* handle);

/** Disable otg */
void bq25896_disable_otg(FurryHalI2cBusHandle* handle);

/** Is otg enabled */
bool bq25896_is_otg_enabled(FurryHalI2cBusHandle* handle);

/** Get VREG (charging limit) voltage in mV */
uint16_t bq25896_get_vreg_voltage(FurryHalI2cBusHandle* handle);

/** Set VREG (charging limit) voltage in mV
 *
 * Valid range: 3840mV - 4208mV, in steps of 16mV
 */
void bq25896_set_vreg_voltage(FurryHalI2cBusHandle* handle, uint16_t vreg_voltage);

/** Check OTG BOOST Fault status */
bool bq25896_check_otg_fault(FurryHalI2cBusHandle* handle);

/** Get VBUS Voltage in mV */
uint16_t bq25896_get_vbus_voltage(FurryHalI2cBusHandle* handle);

/** Get VSYS Voltage in mV */
uint16_t bq25896_get_vsys_voltage(FurryHalI2cBusHandle* handle);

/** Get VBAT Voltage in mV */
uint16_t bq25896_get_vbat_voltage(FurryHalI2cBusHandle* handle);

/** Get VBAT current in mA */
uint16_t bq25896_get_vbat_current(FurryHalI2cBusHandle* handle);

/** Get NTC voltage in mpct of REGN */
uint32_t bq25896_get_ntc_mpct(FurryHalI2cBusHandle* handle);
