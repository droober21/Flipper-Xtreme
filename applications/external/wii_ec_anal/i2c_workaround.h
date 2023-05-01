/*
	As of the date of releasing this code, there is (seemingly) a bug in the FZ i2c library code
	It is described here: https://github.com/flipperdevices/flipperzero-firmware/issues/1670

	This is a short-term workaround so I can keep developing while we get to the bottom of the issue

	FYI. *something* in the following code is the fix

void  furry_hal_i2c_acquire (FurryHalI2cBusHandle* handle)
{
	// 1. Disable the power/backlight (it uses i2c)
    furry_hal_power_insomnia_enter();
    // 2. Lock bus access
    handle->bus->callback(handle->bus, FurryHalI2cBusEventLock);
    // 3. Ensuree that no active handle set
    furry_check(handle->bus->current_handle == NULL);
    // 4. Set current handle
    handle->bus->current_handle = handle;
    // 5. Activate bus
    handle->bus->callback(handle->bus, FurryHalI2cBusEventActivate);
    // 6. Activate handle
    handle->callback(handle, FurryHalI2cBusHandleEventActivate);
}

void  furry_hal_i2c_release (FurryHalI2cBusHandle* handle)
{
    // Ensure that current handle is our handle
    furry_check(handle->bus->current_handle == handle);
    // 6. Deactivate handle
    handle->callback(handle, FurryHalI2cBusHandleEventDeactivate);
    // 5. Deactivate bus
    handle->bus->callback(handle->bus, FurryHalI2cBusEventDeactivate);
    // 3,4. Reset current handle
    handle->bus->current_handle = NULL;
    // 2. Unlock bus
    handle->bus->callback(handle->bus, FurryHalI2cBusEventUnlock);
	// 1. Re-enable the power system
    furry_hal_power_insomnia_exit();
}

*/

#ifndef I2C_WORKAROUND_H_
#define I2C_WORKAROUND_H_

#include <furry_hal.h>

#define ENABLE_WORKAROUND 1

#if ENABLE_WORKAROUND == 1
//+============================================================================ ========================================
static inline bool furry_hal_Wi2c_is_device_ready(
    FurryHalI2cBusHandle* const bus,
    const uint8_t addr,
    const uint32_t tmo) {
    furry_hal_i2c_acquire(bus);
    bool rv = furry_hal_i2c_is_device_ready(bus, addr, tmo);
    furry_hal_i2c_release(bus);
    return rv;
}

//+============================================================================
static inline bool furry_hal_Wi2c_tx(
    FurryHalI2cBusHandle* const bus,
    const uint8_t addr,
    const void* buf,
    const size_t len,
    const uint32_t tmo) {
    furry_hal_i2c_acquire(bus);
    bool rv = furry_hal_i2c_tx(bus, addr, buf, len, tmo);
    furry_hal_i2c_release(bus);
    return rv;
}

//+============================================================================
static inline bool furry_hal_Wi2c_rx(
    FurryHalI2cBusHandle* const bus,
    const uint8_t addr,
    void* buf,
    const size_t len,
    const uint32_t tmo) {
    furry_hal_i2c_acquire(bus);
    bool rv = furry_hal_i2c_rx(bus, addr, buf, len, tmo);
    furry_hal_i2c_release(bus);
    return rv;
}

//+============================================================================
static inline bool furry_hal_Wi2c_trx(
    FurryHalI2cBusHandle* const bus,
    const uint8_t addr,
    const void* tx,
    const size_t txlen,
    void* rx,
    const size_t rxlen,
    const uint32_t tmo) {
    bool rv = furry_hal_Wi2c_tx(bus, addr, tx, txlen, tmo);
    if(rv) rv = furry_hal_Wi2c_rx(bus, addr, rx, rxlen, tmo);
    return rv;
}

//----------------------------------------------------------------------------- ----------------------------------------
#define furry_hal_i2c_is_device_ready(...) furry_hal_Wi2c_is_device_ready(__VA_ARGS__)
#define furry_hal_i2c_tx(...) furry_hal_Wi2c_tx(__VA_ARGS__)
#define furry_hal_i2c_rx(...) furry_hal_Wi2c_rx(__VA_ARGS__)
#define furry_hal_i2c_trx(...) furry_hal_Wi2c_trx(__VA_ARGS__)

#endif //ENABLE_WORKAROUND

//+============================================================================ ========================================
// Some devices take a moment to respond to read requests
// The puts a delay between the address being set and the data being read
//
static inline bool furry_hal_i2c_trxd(
    FurryHalI2cBusHandle* const bus,
    const uint8_t addr,
    const void* tx,
    const size_t txlen,
    void* rx,
    const size_t rxlen,
    const uint32_t tmo,
    const uint32_t us) {
    bool rv = furry_hal_i2c_tx(bus, addr, tx, txlen, tmo);
    if(rv) {
        furry_delay_us(us);
        rv = furry_hal_i2c_rx(bus, addr, rx, rxlen, tmo);
    }
    return rv;
}

#endif //I2C_WORKAROUND_H_
