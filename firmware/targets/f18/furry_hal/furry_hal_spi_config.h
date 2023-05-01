#pragma once

#include <furry_hal_spi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Preset for ST25R916 */
extern const LL_SPI_InitTypeDef furry_hal_spi_preset_2edge_low_8m;

/** Preset for CC1101 */
extern const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_8m;

/** Preset for ST7567 (Display) */
extern const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_4m;

/** Preset for SdCard in fast mode */
extern const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_16m;

/** Preset for SdCard in slow mode */
extern const LL_SPI_InitTypeDef furry_hal_spi_preset_1edge_low_2m;

/** Furry Hal Spi Bus R (External) */
extern FurryHalSpiBus furry_hal_spi_bus_r;

/** Furry Hal Spi Bus D (Display, SdCard) */
extern FurryHalSpiBus furry_hal_spi_bus_d;

/** External on `furry_hal_spi_bus_r`
 * Preset: `furry_hal_spi_preset_1edge_low_2m`
 * 
 * miso: pa6
 * mosi: pa7
 * sck: pb3
 * cs:  pa4 (software controlled)
 * 
 * @warning not initialized by default, call `furry_hal_spi_bus_handle_init` to initialize
 * Bus pins are floating on inactive state, CS high after initialization
 * 
 */
extern FurryHalSpiBusHandle furry_hal_spi_bus_handle_external;

/** ST7567(Display) on `furry_hal_spi_bus_d` */
extern FurryHalSpiBusHandle furry_hal_spi_bus_handle_display;

/** SdCard in fast mode on `furry_hal_spi_bus_d` */
extern FurryHalSpiBusHandle furry_hal_spi_bus_handle_sd_fast;

/** SdCard in slow mode on `furry_hal_spi_bus_d` */
extern FurryHalSpiBusHandle furry_hal_spi_bus_handle_sd_slow;

#ifdef __cplusplus
}
#endif
