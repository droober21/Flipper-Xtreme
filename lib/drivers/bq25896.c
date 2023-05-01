#include "bq25896.h"

#include <stddef.h>

uint8_t bit_reverse(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

typedef struct {
    REG00 r00;
    REG01 r01;
    REG02 r02;
    REG03 r03;
    REG04 r04;
    REG05 r05;
    REG06 r06;
    REG07 r07;
    REG08 r08;
    REG09 r09;
    REG0A r0A;
    REG0B r0B;
    REG0C r0C;
    REG0D r0D;
    REG0E r0E;
    REG0F r0F;
    REG10 r10;
    REG11 r11;
    REG12 r12;
    REG13 r13;
    REG14 r14;
} bq25896_regs_t;

static bq25896_regs_t bq25896_regs;

void bq25896_init(FurryHalI2cBusHandle* handle) {
    bq25896_regs.r14.REG_RST = 1;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x14, *(uint8_t*)&bq25896_regs.r14, BQ25896_I2C_TIMEOUT);

    // Readout all registers
    furry_hal_i2c_read_mem(
        handle,
        BQ25896_ADDRESS,
        0x00,
        (uint8_t*)&bq25896_regs,
        sizeof(bq25896_regs),
        BQ25896_I2C_TIMEOUT);

    // Poll ADC forever
    bq25896_regs.r02.CONV_START = 1;
    bq25896_regs.r02.CONV_RATE = 1;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x02, *(uint8_t*)&bq25896_regs.r02, BQ25896_I2C_TIMEOUT);

    bq25896_regs.r07.WATCHDOG = WatchdogDisable;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x07, *(uint8_t*)&bq25896_regs.r07, BQ25896_I2C_TIMEOUT);

    // OTG power configuration
    bq25896_regs.r0A.BOOSTV = 0x8; // BOOST Voltage: 5.062V
    bq25896_regs.r0A.BOOST_LIM = BOOST_LIM_1400; // BOOST Current limit: 1.4A
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x0A, *(uint8_t*)&bq25896_regs.r0A, BQ25896_I2C_TIMEOUT);

    furry_hal_i2c_read_mem(
        handle,
        BQ25896_ADDRESS,
        0x00,
        (uint8_t*)&bq25896_regs,
        sizeof(bq25896_regs),
        BQ25896_I2C_TIMEOUT);
}

void bq25896_poweroff(FurryHalI2cBusHandle* handle) {
    bq25896_regs.r09.BATFET_DIS = 1;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x09, *(uint8_t*)&bq25896_regs.r09, BQ25896_I2C_TIMEOUT);
}

ChrgStat bq25896_get_charge_status(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_mem(
        handle,
        BQ25896_ADDRESS,
        0x00,
        (uint8_t*)&bq25896_regs,
        sizeof(bq25896_regs),
        BQ25896_I2C_TIMEOUT);
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x0B, (uint8_t*)&bq25896_regs.r0B, BQ25896_I2C_TIMEOUT);
    return bq25896_regs.r0B.CHRG_STAT;
}

bool bq25896_is_charging(FurryHalI2cBusHandle* handle) {
    // Include precharge, fast charging, and charging termination done as "charging"
    return bq25896_get_charge_status(handle) != ChrgStatNo;
}

bool bq25896_is_charging_done(FurryHalI2cBusHandle* handle) {
    return bq25896_get_charge_status(handle) == ChrgStatDone;
}

void bq25896_enable_charging(FurryHalI2cBusHandle* handle) {
    bq25896_regs.r03.CHG_CONFIG = 1;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x03, *(uint8_t*)&bq25896_regs.r03, BQ25896_I2C_TIMEOUT);
}

void bq25896_disable_charging(FurryHalI2cBusHandle* handle) {
    bq25896_regs.r03.CHG_CONFIG = 0;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x03, *(uint8_t*)&bq25896_regs.r03, BQ25896_I2C_TIMEOUT);
}

void bq25896_enable_otg(FurryHalI2cBusHandle* handle) {
    bq25896_regs.r03.OTG_CONFIG = 1;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x03, *(uint8_t*)&bq25896_regs.r03, BQ25896_I2C_TIMEOUT);
}

void bq25896_disable_otg(FurryHalI2cBusHandle* handle) {
    bq25896_regs.r03.OTG_CONFIG = 0;
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x03, *(uint8_t*)&bq25896_regs.r03, BQ25896_I2C_TIMEOUT);
}

bool bq25896_is_otg_enabled(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x03, (uint8_t*)&bq25896_regs.r03, BQ25896_I2C_TIMEOUT);
    return bq25896_regs.r03.OTG_CONFIG;
}

uint16_t bq25896_get_vreg_voltage(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x06, (uint8_t*)&bq25896_regs.r06, BQ25896_I2C_TIMEOUT);
    return (uint16_t)bq25896_regs.r06.VREG * 16 + 3840;
}

void bq25896_set_vreg_voltage(FurryHalI2cBusHandle* handle, uint16_t vreg_voltage) {
    if(vreg_voltage < 3840) {
        // Minimum valid value is 3840 mV
        vreg_voltage = 3840;
    } else if(vreg_voltage > 4208) {
        // Maximum safe value is 4208 mV
        vreg_voltage = 4208;
    }

    // Find the nearest voltage value (subtract offset, divide into sections)
    // Values are truncated downward as needed (e.g. 4200mV -> 4192 mV)
    bq25896_regs.r06.VREG = (uint8_t)((vreg_voltage - 3840) / 16);

    // Apply changes
    furry_hal_i2c_write_reg_8(
        handle, BQ25896_ADDRESS, 0x06, *(uint8_t*)&bq25896_regs.r06, BQ25896_I2C_TIMEOUT);
}

bool bq25896_check_otg_fault(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x0C, (uint8_t*)&bq25896_regs.r0C, BQ25896_I2C_TIMEOUT);
    return bq25896_regs.r0C.BOOST_FAULT;
}

uint16_t bq25896_get_vbus_voltage(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x11, (uint8_t*)&bq25896_regs.r11, BQ25896_I2C_TIMEOUT);
    if(bq25896_regs.r11.VBUS_GD) {
        return (uint16_t)bq25896_regs.r11.VBUSV * 100 + 2600;
    } else {
        return 0;
    }
}

uint16_t bq25896_get_vsys_voltage(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x0F, (uint8_t*)&bq25896_regs.r0F, BQ25896_I2C_TIMEOUT);
    return (uint16_t)bq25896_regs.r0F.SYSV * 20 + 2304;
}

uint16_t bq25896_get_vbat_voltage(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x0E, (uint8_t*)&bq25896_regs.r0E, BQ25896_I2C_TIMEOUT);
    return (uint16_t)bq25896_regs.r0E.BATV * 20 + 2304;
}

uint16_t bq25896_get_vbat_current(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x12, (uint8_t*)&bq25896_regs.r12, BQ25896_I2C_TIMEOUT);
    return (uint16_t)bq25896_regs.r12.ICHGR * 50;
}

uint32_t bq25896_get_ntc_mpct(FurryHalI2cBusHandle* handle) {
    furry_hal_i2c_read_reg_8(
        handle, BQ25896_ADDRESS, 0x10, (uint8_t*)&bq25896_regs.r10, BQ25896_I2C_TIMEOUT);
    return (uint32_t)bq25896_regs.r10.TSPCT * 465 + 21000;
}
