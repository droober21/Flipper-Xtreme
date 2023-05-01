#pragma once

#include <furry_hal_i2c_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Internal(power) i2c bus, I2C1, under reset when not used */
extern FurryHalI2cBus furry_hal_i2c_bus_power;

/** External i2c bus, I2C3, under reset when not used */
extern FurryHalI2cBus furry_hal_i2c_bus_external;

/** Handle for internal(power) i2c bus
 * Bus: furry_hal_i2c_bus_external
 * Pins: PA9(SCL) / PA10(SDA), float on release
 * Params: 400khz
 */
extern FurryHalI2cBusHandle furry_hal_i2c_handle_power;

/** Handle for external i2c bus
 * Bus: furry_hal_i2c_bus_external
 * Pins: PC0(SCL) / PC1(SDA), float on release
 * Params: 100khz
 */
extern FurryHalI2cBusHandle furry_hal_i2c_handle_external;

#ifdef __cplusplus
}
#endif
