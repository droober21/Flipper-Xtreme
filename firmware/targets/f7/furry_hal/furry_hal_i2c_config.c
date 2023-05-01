#include <furry_hal_i2c_config.h>
#include <furry_hal_resources.h>
#include <furry_hal_version.h>
#include <stm32wbxx_ll_bus.h>
#include <stm32wbxx_ll_rcc.h>

/** Timing register value is computed with the STM32CubeMX Tool,
  * Standard Mode @100kHz with I2CCLK = 64 MHz,
  * rise time = 0ns, fall time = 0ns
  */
#define FURRY_HAL_I2C_CONFIG_POWER_I2C_TIMINGS_100 0x10707DBC

/** Timing register value is computed with the STM32CubeMX Tool,
  * Fast Mode @400kHz with I2CCLK = 64 MHz,
  * rise time = 0ns, fall time = 0ns
  */
#define FURRY_HAL_I2C_CONFIG_POWER_I2C_TIMINGS_400 0x00602173

FurryMutex* furry_hal_i2c_bus_power_mutex = NULL;

static void furry_hal_i2c_bus_power_event(FurryHalI2cBus* bus, FurryHalI2cBusEvent event) {
    if(event == FurryHalI2cBusEventInit) {
        furry_hal_i2c_bus_power_mutex = furry_mutex_alloc(FurryMutexTypeNormal);
        FURRY_CRITICAL_ENTER();
        LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_I2C1);
        FURRY_CRITICAL_EXIT();
        bus->current_handle = NULL;
    } else if(event == FurryHalI2cBusEventDeinit) {
        furry_mutex_free(furry_hal_i2c_bus_power_mutex);
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_I2C1);
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_I2C1);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalI2cBusEventLock) {
        furry_check(
            furry_mutex_acquire(furry_hal_i2c_bus_power_mutex, FurryWaitForever) == FurryStatusOk);
    } else if(event == FurryHalI2cBusEventUnlock) {
        furry_check(furry_mutex_release(furry_hal_i2c_bus_power_mutex) == FurryStatusOk);
    } else if(event == FurryHalI2cBusEventActivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_I2C1);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalI2cBusEventDeactivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_I2C1);
        FURRY_CRITICAL_EXIT();
    }
}

FurryHalI2cBus furry_hal_i2c_bus_power = {
    .i2c = I2C1,
    .callback = furry_hal_i2c_bus_power_event,
};

FurryMutex* furry_hal_i2c_bus_external_mutex = NULL;

static void furry_hal_i2c_bus_external_event(FurryHalI2cBus* bus, FurryHalI2cBusEvent event) {
    if(event == FurryHalI2cBusEventInit) {
        furry_hal_i2c_bus_external_mutex = furry_mutex_alloc(FurryMutexTypeNormal);
        FURRY_CRITICAL_ENTER();
        LL_RCC_SetI2CClockSource(LL_RCC_I2C3_CLKSOURCE_PCLK1);
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_I2C3);
        FURRY_CRITICAL_EXIT();
        bus->current_handle = NULL;
    } else if(event == FurryHalI2cBusEventDeinit) {
        furry_mutex_free(furry_hal_i2c_bus_external_mutex);
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_I2C3);
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_I2C3);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalI2cBusEventLock) {
        furry_check(
            furry_mutex_acquire(furry_hal_i2c_bus_external_mutex, FurryWaitForever) == FurryStatusOk);
    } else if(event == FurryHalI2cBusEventUnlock) {
        furry_check(furry_mutex_release(furry_hal_i2c_bus_external_mutex) == FurryStatusOk);
    } else if(event == FurryHalI2cBusEventActivate) {
        FURRY_CRITICAL_ENTER();
        LL_RCC_SetI2CClockSource(LL_RCC_I2C3_CLKSOURCE_PCLK1);
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_I2C3);
        FURRY_CRITICAL_EXIT();
    } else if(event == FurryHalI2cBusEventDeactivate) {
        FURRY_CRITICAL_ENTER();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_I2C3);
        FURRY_CRITICAL_EXIT();
    }
}

FurryHalI2cBus furry_hal_i2c_bus_external = {
    .i2c = I2C3,
    .callback = furry_hal_i2c_bus_external_event,
};

void furry_hal_i2c_bus_handle_power_event(
    FurryHalI2cBusHandle* handle,
    FurryHalI2cBusHandleEvent event) {
    if(event == FurryHalI2cBusHandleEventActivate) {
        furry_hal_gpio_init_ex(
            &gpio_i2c_power_sda,
            GpioModeAltFunctionOpenDrain,
            GpioPullNo,
            GpioSpeedLow,
            GpioAltFn4I2C1);
        furry_hal_gpio_init_ex(
            &gpio_i2c_power_scl,
            GpioModeAltFunctionOpenDrain,
            GpioPullNo,
            GpioSpeedLow,
            GpioAltFn4I2C1);

        LL_I2C_InitTypeDef I2C_InitStruct;
        I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
        I2C_InitStruct.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
        I2C_InitStruct.DigitalFilter = 0;
        I2C_InitStruct.OwnAddress1 = 0;
        I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
        I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
        if(furry_hal_version_get_hw_version() > 10) {
            I2C_InitStruct.Timing = FURRY_HAL_I2C_CONFIG_POWER_I2C_TIMINGS_400;
        } else {
            I2C_InitStruct.Timing = FURRY_HAL_I2C_CONFIG_POWER_I2C_TIMINGS_100;
        }
        LL_I2C_Init(handle->bus->i2c, &I2C_InitStruct);
        // I2C is enabled at this point
        LL_I2C_EnableAutoEndMode(handle->bus->i2c);
        LL_I2C_SetOwnAddress2(handle->bus->i2c, 0, LL_I2C_OWNADDRESS2_NOMASK);
        LL_I2C_DisableOwnAddress2(handle->bus->i2c);
        LL_I2C_DisableGeneralCall(handle->bus->i2c);
        LL_I2C_EnableClockStretching(handle->bus->i2c);
    } else if(event == FurryHalI2cBusHandleEventDeactivate) {
        LL_I2C_Disable(handle->bus->i2c);
        furry_hal_gpio_write(&gpio_i2c_power_sda, 1);
        furry_hal_gpio_write(&gpio_i2c_power_scl, 1);
        furry_hal_gpio_init_ex(
            &gpio_i2c_power_sda, GpioModeAnalog, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furry_hal_gpio_init_ex(
            &gpio_i2c_power_scl, GpioModeAnalog, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
}

FurryHalI2cBusHandle furry_hal_i2c_handle_power = {
    .bus = &furry_hal_i2c_bus_power,
    .callback = furry_hal_i2c_bus_handle_power_event,
};

void furry_hal_i2c_bus_handle_external_event(
    FurryHalI2cBusHandle* handle,
    FurryHalI2cBusHandleEvent event) {
    if(event == FurryHalI2cBusHandleEventActivate) {
        furry_hal_gpio_init_ex(
            &gpio_ext_pc0, GpioModeAltFunctionOpenDrain, GpioPullNo, GpioSpeedLow, GpioAltFn4I2C3);
        furry_hal_gpio_init_ex(
            &gpio_ext_pc1, GpioModeAltFunctionOpenDrain, GpioPullNo, GpioSpeedLow, GpioAltFn4I2C3);

        LL_I2C_InitTypeDef I2C_InitStruct;
        I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
        I2C_InitStruct.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
        I2C_InitStruct.DigitalFilter = 0;
        I2C_InitStruct.OwnAddress1 = 0;
        I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
        I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
        I2C_InitStruct.Timing = FURRY_HAL_I2C_CONFIG_POWER_I2C_TIMINGS_100;
        LL_I2C_Init(handle->bus->i2c, &I2C_InitStruct);
        // I2C is enabled at this point
        LL_I2C_EnableAutoEndMode(handle->bus->i2c);
        LL_I2C_SetOwnAddress2(handle->bus->i2c, 0, LL_I2C_OWNADDRESS2_NOMASK);
        LL_I2C_DisableOwnAddress2(handle->bus->i2c);
        LL_I2C_DisableGeneralCall(handle->bus->i2c);
        LL_I2C_EnableClockStretching(handle->bus->i2c);
    } else if(event == FurryHalI2cBusHandleEventDeactivate) {
        LL_I2C_Disable(handle->bus->i2c);
        furry_hal_gpio_write(&gpio_ext_pc0, 1);
        furry_hal_gpio_write(&gpio_ext_pc1, 1);
        furry_hal_gpio_init_ex(
            &gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furry_hal_gpio_init_ex(
            &gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
}

FurryHalI2cBusHandle furry_hal_i2c_handle_external = {
    .bus = &furry_hal_i2c_bus_external,
    .callback = furry_hal_i2c_bus_handle_external_event,
};
