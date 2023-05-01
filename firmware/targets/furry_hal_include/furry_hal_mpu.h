/**
 * @file furry_hal_light.h
 * Light control HAL API
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FurryHalMpuRegionNULL = 0x00, // region 0 used to protect null pointer dereference
    FurryHalMpuRegionStack = 0x01, // region 1 used to protect stack
    FurryHalMpuRegion2 = 0x02,
    FurryHalMpuRegion3 = 0x03,
    FurryHalMpuRegion4 = 0x04,
    FurryHalMpuRegion5 = 0x05,
    FurryHalMpuRegion6 = 0x06,
    FurryHalMpuRegion7 = 0x07,
} FurryHalMpuRegion;

typedef enum {
    FurryHalMPURegionSize32B = 0x04U,
    FurryHalMPURegionSize64B = 0x05U,
    FurryHalMPURegionSize128B = 0x06U,
    FurryHalMPURegionSize256B = 0x07U,
    FurryHalMPURegionSize512B = 0x08U,
    FurryHalMPURegionSize1KB = 0x09U,
    FurryHalMPURegionSize2KB = 0x0AU,
    FurryHalMPURegionSize4KB = 0x0BU,
    FurryHalMPURegionSize8KB = 0x0CU,
    FurryHalMPURegionSize16KB = 0x0DU,
    FurryHalMPURegionSize32KB = 0x0EU,
    FurryHalMPURegionSize64KB = 0x0FU,
    FurryHalMPURegionSize128KB = 0x10U,
    FurryHalMPURegionSize256KB = 0x11U,
    FurryHalMPURegionSize512KB = 0x12U,
    FurryHalMPURegionSize1MB = 0x13U,
    FurryHalMPURegionSize2MB = 0x14U,
    FurryHalMPURegionSize4MB = 0x15U,
    FurryHalMPURegionSize8MB = 0x16U,
    FurryHalMPURegionSize16MB = 0x17U,
    FurryHalMPURegionSize32MB = 0x18U,
    FurryHalMPURegionSize64MB = 0x19U,
    FurryHalMPURegionSize128MB = 0x1AU,
    FurryHalMPURegionSize256MB = 0x1BU,
    FurryHalMPURegionSize512MB = 0x1CU,
    FurryHalMPURegionSize1GB = 0x1DU,
    FurryHalMPURegionSize2GB = 0x1EU,
    FurryHalMPURegionSize4GB = 0x1FU,
} FurryHalMPURegionSize;

/**
 * @brief Initialize memory protection unit
 */
void furry_hal_mpu_init();

/**
* @brief Enable memory protection unit
*/
void furry_hal_mpu_enable();

/**
* @brief Disable memory protection unit
*/
void furry_hal_mpu_disable();

void furry_hal_mpu_protect_no_access(
    FurryHalMpuRegion region,
    uint32_t address,
    FurryHalMPURegionSize size);

void furry_hal_mpu_protect_read_only(
    FurryHalMpuRegion region,
    uint32_t address,
    FurryHalMPURegionSize size);

void furry_hal_mpu_protect_disable(FurryHalMpuRegion region);

#ifdef __cplusplus
}
#endif
