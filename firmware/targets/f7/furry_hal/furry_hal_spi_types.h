#pragma once

#include <stdint.h>
#include <stddef.h>

#include <furry_hal_gpio.h>

#include <stm32wbxx_ll_spi.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_bus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FurryHalSpiBus FurryHalSpiBus;
typedef struct FurryHalSpiBusHandle FurryHalSpiBusHandle;

/** FurryHal spi bus states */
typedef enum {
    FurryHalSpiBusEventInit, /**< Bus initialization event, called on system start */
    FurryHalSpiBusEventDeinit, /**< Bus deinitialization event, called on system stop */
    FurryHalSpiBusEventLock, /**< Bus lock event, called before activation */
    FurryHalSpiBusEventUnlock, /**< Bus unlock event, called after deactivation */
    FurryHalSpiBusEventActivate, /**< Bus activation event, called before handle activation */
    FurryHalSpiBusEventDeactivate, /**< Bus deactivation event, called after handle deactivation  */
} FurryHalSpiBusEvent;

/** FurryHal spi bus event callback */
typedef void (*FurryHalSpiBusEventCallback)(FurryHalSpiBus* bus, FurryHalSpiBusEvent event);

/** FurryHal spi bus */
struct FurryHalSpiBus {
    SPI_TypeDef* spi;
    FurryHalSpiBusEventCallback callback;
    FurryHalSpiBusHandle* current_handle;
};

/** FurryHal spi handle states */
typedef enum {
    FurryHalSpiBusHandleEventInit, /**< Handle init, called on system start, initialize gpio for idle state */
    FurryHalSpiBusHandleEventDeinit, /**< Handle deinit, called on system stop, deinitialize gpio for default state */
    FurryHalSpiBusHandleEventActivate, /**< Handle activate: connect gpio and apply bus config */
    FurryHalSpiBusHandleEventDeactivate, /**< Handle deactivate: disconnect gpio and reset bus config */
} FurryHalSpiBusHandleEvent;

/** FurryHal spi handle event callback */
typedef void (*FurryHalSpiBusHandleEventCallback)(
    FurryHalSpiBusHandle* handle,
    FurryHalSpiBusHandleEvent event);

/** FurryHal spi handle */
struct FurryHalSpiBusHandle {
    FurryHalSpiBus* bus;
    FurryHalSpiBusHandleEventCallback callback;
    const GpioPin* miso;
    const GpioPin* mosi;
    const GpioPin* sck;
    const GpioPin* cs;
};

#ifdef __cplusplus
}
#endif
