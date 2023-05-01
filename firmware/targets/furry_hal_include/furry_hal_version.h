/**
 * @file furry_hal_version.h
 * Version HAL API
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <lib/toolbox/version.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FURRY_HAL_VERSION_NAME_LENGTH 8
#define FURRY_HAL_VERSION_ARRAY_NAME_LENGTH (FURRY_HAL_VERSION_NAME_LENGTH + 1)
#define FURRY_HAL_BT_ADV_NAME_LENGTH (18 + 1) // 18 characters + null terminator
#define FURRY_HAL_VERSION_DEVICE_NAME_LENGTH \
    (1 + FURRY_HAL_BT_ADV_NAME_LENGTH) // Used for custom BT name, BLE symbol + name

/** OTP Versions enum */
typedef enum {
    FurryHalVersionOtpVersion0 = 0x00,
    FurryHalVersionOtpVersion1 = 0x01,
    FurryHalVersionOtpVersion2 = 0x02,
    FurryHalVersionOtpVersionEmpty = 0xFFFFFFFE,
    FurryHalVersionOtpVersionUnknown = 0xFFFFFFFF,
} FurryHalVersionOtpVersion;

/** Device Colors */
typedef enum {
    FurryHalVersionColorUnknown = 0x00,
    FurryHalVersionColorBlack = 0x01,
    FurryHalVersionColorWhite = 0x02,
} FurryHalVersionColor;

/** Device Regions */
typedef enum {
    FurryHalVersionRegionUnknown = 0x00,
    FurryHalVersionRegionEuRu = 0x01,
    FurryHalVersionRegionUsCaAu = 0x02,
    FurryHalVersionRegionJp = 0x03,
    FurryHalVersionRegionWorld = 0x04,
} FurryHalVersionRegion;

/** Device Display */
typedef enum {
    FurryHalVersionDisplayUnknown = 0x00,
    FurryHalVersionDisplayErc = 0x01,
    FurryHalVersionDisplayMgg = 0x02,
} FurryHalVersionDisplay;

/** Init flipper version
 */
void furry_hal_version_init();

/** Check target firmware version
 *
 * @return     true if target and real matches
 */
bool furry_hal_version_do_i_belong_here();

/** Get model name
 *
 * @return     model name C-string
 */
const char* furry_hal_version_get_model_name();

/** Get model name
 *
 * @return     model code C-string
 */
const char* furry_hal_version_get_model_code();

/** Get FCC ID
 *
 * @return     FCC id as C-string
 */
const char* furry_hal_version_get_fcc_id();

/** Get IC id
 *
 * @return     IC id as C-string
 */
const char* furry_hal_version_get_ic_id();

/** Get OTP version
 *
 * @return     OTP Version
 */
FurryHalVersionOtpVersion furry_hal_version_get_otp_version();

/** Get hardware version
 *
 * @return     Hardware Version
 */
uint8_t furry_hal_version_get_hw_version();

/** Get hardware target
 *
 * @return     Hardware Target
 */
uint8_t furry_hal_version_get_hw_target();

/** Get hardware body
 *
 * @return     Hardware Body
 */
uint8_t furry_hal_version_get_hw_body();

/** Get hardware body color
 *
 * @return     Hardware Color
 */
FurryHalVersionColor furry_hal_version_get_hw_color();

/** Get hardware connect
 *
 * @return     Hardware Interconnect
 */
uint8_t furry_hal_version_get_hw_connect();

/** Get hardware region (fake) = 0
 *
 * @return     Hardware Region (fake)
 */
FurryHalVersionRegion furry_hal_version_get_hw_region();

/** Get hardware region name (fake) = R00
 *
 * @return     Hardware Region name (fake)
 */
const char* furry_hal_version_get_hw_region_name();

/** Get hardware region (OTP)
 *
 * @return     Hardware Region
 */
FurryHalVersionRegion furry_hal_version_get_hw_region_otp();

/** Get hardware region name (OTP)
 *
 * @return     Hardware Region name
 */
const char* furry_hal_version_get_hw_region_name_otp();

/** Get hardware display id
 *
 * @return     Display id
 */
FurryHalVersionDisplay furry_hal_version_get_hw_display();

/** Get hardware timestamp
 *
 * @return     Hardware Manufacture timestamp
 */
uint32_t furry_hal_version_get_hw_timestamp();

// Set custom name
void furry_hal_version_set_custom_name(const char* name);

/** Get pointer to target name
 *
 * @return     Hardware Name C-string
 */
const char* furry_hal_version_get_name_ptr();

/** Get pointer to target device name
 *
 * @return     Hardware Device Name C-string
 */
const char* furry_hal_version_get_device_name_ptr();

/** Get pointer to target ble local device name
 *
 * @return     Ble Device Name C-string
 */
const char* furry_hal_version_get_ble_local_device_name_ptr();

/** Get BLE MAC address
 *
 * @return     pointer to BLE MAC address
 */
const uint8_t* furry_hal_version_get_ble_mac();

/** Get address of version structure of firmware.
 *
 * @return     Address of firmware version structure.
 */
const struct Version* furry_hal_version_get_firmware_version();

/** Get platform UID size in bytes
 *
 * @return     UID size in bytes
 */
size_t furry_hal_version_uid_size();

/** Get const pointer to UID
 *
 * @return     pointer to UID
 */
const uint8_t* furry_hal_version_uid();

#ifdef __cplusplus
}
#endif
