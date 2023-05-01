#pragma once

#include <stm32wbxx_ll_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FurryHalI2cBus FurryHalI2cBus;
typedef struct FurryHalI2cBusHandle FurryHalI2cBusHandle;

/** FurryHal i2c bus states */
typedef enum {
    FurryHalI2cBusEventInit, /**< Bus initialization event, called on system start */
    FurryHalI2cBusEventDeinit, /**< Bus deinitialization event, called on system stop */
    FurryHalI2cBusEventLock, /**< Bus lock event, called before activation */
    FurryHalI2cBusEventUnlock, /**< Bus unlock event, called after deactivation */
    FurryHalI2cBusEventActivate, /**< Bus activation event, called before handle activation */
    FurryHalI2cBusEventDeactivate, /**< Bus deactivation event, called after handle deactivation  */
} FurryHalI2cBusEvent;

/** FurryHal i2c bus event callback */
typedef void (*FurryHalI2cBusEventCallback)(FurryHalI2cBus* bus, FurryHalI2cBusEvent event);

/** FurryHal i2c bus */
struct FurryHalI2cBus {
    I2C_TypeDef* i2c;
    FurryHalI2cBusHandle* current_handle;
    FurryHalI2cBusEventCallback callback;
};

/** FurryHal i2c handle states */
typedef enum {
    FurryHalI2cBusHandleEventActivate, /**< Handle activate: connect gpio and apply bus config */
    FurryHalI2cBusHandleEventDeactivate, /**< Handle deactivate: disconnect gpio and reset bus config */
} FurryHalI2cBusHandleEvent;

/** FurryHal i2c handle event callback */
typedef void (*FurryHalI2cBusHandleEventCallback)(
    FurryHalI2cBusHandle* handle,
    FurryHalI2cBusHandleEvent event);

/** FurryHal i2c handle */
struct FurryHalI2cBusHandle {
    FurryHalI2cBus* bus;
    FurryHalI2cBusHandleEventCallback callback;
};

#ifdef __cplusplus
}
#endif
