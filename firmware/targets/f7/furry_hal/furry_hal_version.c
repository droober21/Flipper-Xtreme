#include <furry_hal_version.h>
#include <furry_hal_rtc.h>

#include <furry.h>
#include <stm32wbxx.h>
#include <stm32wbxx_ll_rtc.h>

#include <stdio.h>
#include <ble/ble.h>

#define TAG "FurryHalVersion"

#define FURRY_HAL_VERSION_OTP_HEADER_MAGIC 0xBABE
#define FURRY_HAL_VERSION_OTP_ADDRESS OTP_AREA_BASE

/** OTP V0 Structure: prototypes and early EVT */
typedef struct {
    uint8_t board_version;
    uint8_t board_target;
    uint8_t board_body;
    uint8_t board_connect;
    uint32_t header_timestamp;
    char name[FURRY_HAL_VERSION_NAME_LENGTH];
} FurryHalVersionOTPv0;

/** OTP V1 Structure: late EVT, DVT */
typedef struct {
    /* First 64 bits: header */
    uint16_t header_magic;
    uint8_t header_version;
    uint8_t header_reserved;
    uint32_t header_timestamp;

    /* Second 64 bits: board info */
    uint8_t board_version; /** Board version */
    uint8_t board_target; /** Board target firmware */
    uint8_t board_body; /** Board body */
    uint8_t board_connect; /** Board interconnect */
    uint8_t board_color; /** Board color */
    uint8_t board_region; /** Board region */
    uint16_t board_reserved; /** Reserved for future use, 0x0000 */

    /* Third 64 bits: Unique Device Name */
    char name[FURRY_HAL_VERSION_NAME_LENGTH]; /** Unique Device Name */
} FurryHalVersionOTPv1;

/** OTP V2 Structure: DVT2, PVT, Production */
typedef struct {
    /* Early First 64 bits: header */
    uint16_t header_magic;
    uint8_t header_version;
    uint8_t header_reserved;
    uint32_t header_timestamp;

    /* Early Second 64 bits: board info */
    uint8_t board_version; /** Board version */
    uint8_t board_target; /** Board target firmware */
    uint8_t board_body; /** Board body */
    uint8_t board_connect; /** Board interconnect */
    uint8_t board_display; /** Board display */
    uint8_t board_reserved2_0; /** Reserved for future use, 0x00 */
    uint16_t board_reserved2_1; /** Reserved for future use, 0x0000 */

    /* Late Third 64 bits: device info */
    uint8_t board_color; /** Board color */
    uint8_t board_region; /** Board region */
    uint16_t board_reserved3_0; /** Reserved for future use, 0x0000 */
    uint32_t board_reserved3_1; /** Reserved for future use, 0x00000000 */

    /* Late Fourth 64 bits: Unique Device Name */
    char name[FURRY_HAL_VERSION_NAME_LENGTH]; /** Unique Device Name */
} FurryHalVersionOTPv2;

/** Represenation Model: */
typedef struct {
    uint32_t timestamp;

    uint8_t board_version; /** Board version */
    uint8_t board_target; /** Board target firmware */
    uint8_t board_body; /** Board body */
    uint8_t board_connect; /** Board interconnect */
    uint8_t board_color; /** Board color */
    uint8_t board_region; /** Board region */
    uint8_t board_display; /** Board display */

    char name[FURRY_HAL_VERSION_ARRAY_NAME_LENGTH]; /** \0 terminated name */
    char device_name[FURRY_HAL_VERSION_DEVICE_NAME_LENGTH]; /** device name for special needs */
    uint8_t ble_mac[6];
} FurryHalVersion;

static FurryHalVersion furry_hal_version = {0};

void furry_hal_version_set_custom_name(const char* name) {
    if((name != NULL) && ((strlen(name) >= 1) && (strlen(name) <= 8))) {
        strlcpy(furry_hal_version.name, name, FURRY_HAL_VERSION_ARRAY_NAME_LENGTH);
        snprintf(
            furry_hal_version.device_name,
            FURRY_HAL_VERSION_DEVICE_NAME_LENGTH,
            "x%s", // Someone tell me why that X is needed
            name);

        furry_hal_version.device_name[0] = AD_TYPE_COMPLETE_LOCAL_NAME;
    }
}

static void furry_hal_version_set_name(const char* name) {
    if(name != NULL) {
        strlcpy(furry_hal_version.name, name, FURRY_HAL_VERSION_ARRAY_NAME_LENGTH);
        snprintf(
            furry_hal_version.device_name,
            FURRY_HAL_VERSION_DEVICE_NAME_LENGTH,
            "x%s", // Someone tell me why that X is needed
            furry_hal_version.name);
    } else {
        snprintf(furry_hal_version.device_name, FURRY_HAL_VERSION_DEVICE_NAME_LENGTH, "xFlipper");
    }

    furry_hal_version.device_name[0] = AD_TYPE_COMPLETE_LOCAL_NAME;

    // BLE Mac address
    uint32_t udn = LL_FLASH_GetUDN();
    if(version_get_custom_name(NULL) != NULL) {
        udn = *((uint32_t*)version_get_custom_name(NULL));
    }

    uint32_t company_id = LL_FLASH_GetSTCompanyID();
    uint32_t device_id = LL_FLASH_GetDeviceID();
    furry_hal_version.ble_mac[0] = (uint8_t)(udn & 0x000000FF);
    furry_hal_version.ble_mac[1] = (uint8_t)((udn & 0x0000FF00) >> 8);
    furry_hal_version.ble_mac[2] = (uint8_t)((udn & 0x00FF0000) >> 16);
    furry_hal_version.ble_mac[3] = (uint8_t)device_id;
    furry_hal_version.ble_mac[4] = (uint8_t)(company_id & 0x000000FF);
    furry_hal_version.ble_mac[5] = (uint8_t)((company_id & 0x0000FF00) >> 8);
}

static void furry_hal_version_load_otp_default() {
    furry_hal_version_set_name(NULL);
}

static void furry_hal_version_load_otp_v0() {
    const FurryHalVersionOTPv0* otp = (FurryHalVersionOTPv0*)FURRY_HAL_VERSION_OTP_ADDRESS;

    furry_hal_version.timestamp = otp->header_timestamp;
    furry_hal_version.board_version = otp->board_version;
    furry_hal_version.board_target = otp->board_target;
    furry_hal_version.board_body = otp->board_body;
    furry_hal_version.board_connect = otp->board_connect;

    if(version_get_custom_name(NULL) != NULL) {
        furry_hal_version_set_name(version_get_custom_name(NULL));
    } else {
        furry_hal_version_set_name(otp->name);
    }
}

static void furry_hal_version_load_otp_v1() {
    const FurryHalVersionOTPv1* otp = (FurryHalVersionOTPv1*)FURRY_HAL_VERSION_OTP_ADDRESS;

    furry_hal_version.timestamp = otp->header_timestamp;
    furry_hal_version.board_version = otp->board_version;
    furry_hal_version.board_target = otp->board_target;
    furry_hal_version.board_body = otp->board_body;
    furry_hal_version.board_connect = otp->board_connect;
    furry_hal_version.board_color = otp->board_color;
    furry_hal_version.board_region = otp->board_region;

    if(version_get_custom_name(NULL) != NULL) {
        furry_hal_version_set_name(version_get_custom_name(NULL));
    } else {
        furry_hal_version_set_name(otp->name);
    }
}

static void furry_hal_version_load_otp_v2() {
    const FurryHalVersionOTPv2* otp = (FurryHalVersionOTPv2*)FURRY_HAL_VERSION_OTP_ADDRESS;

    // 1st block, programmed afer baking
    furry_hal_version.timestamp = otp->header_timestamp;

    // 2nd block, programmed afer baking
    furry_hal_version.board_version = otp->board_version;
    furry_hal_version.board_target = otp->board_target;
    furry_hal_version.board_body = otp->board_body;
    furry_hal_version.board_connect = otp->board_connect;
    furry_hal_version.board_display = otp->board_display;

    // 3rd and 4th blocks, programmed on FATP stage
    if(otp->board_color != 0xFF) {
        furry_hal_version.board_color = otp->board_color;
        furry_hal_version.board_region = otp->board_region;
        if(version_get_custom_name(NULL) != NULL) {
            furry_hal_version_set_name(version_get_custom_name(NULL));
        } else {
            furry_hal_version_set_name(otp->name);
        }
    } else {
        furry_hal_version.board_color = 0;
        furry_hal_version.board_region = 0;
        furry_hal_version_set_name(NULL);
    }
}

void furry_hal_version_init() {
    switch(furry_hal_version_get_otp_version()) {
    case FurryHalVersionOtpVersionUnknown:
    case FurryHalVersionOtpVersionEmpty:
        furry_hal_version_load_otp_default();
        break;
    case FurryHalVersionOtpVersion0:
        furry_hal_version_load_otp_v0();
        break;
    case FurryHalVersionOtpVersion1:
        furry_hal_version_load_otp_v1();
        break;
    case FurryHalVersionOtpVersion2:
        furry_hal_version_load_otp_v2();
        break;
    default:
        furry_crash(NULL);
    }

    furry_hal_rtc_set_register(FurryHalRtcRegisterVersion, (uint32_t)version_get());

    FURRY_LOG_I(TAG, "Init OK");
}

FurryHalVersionOtpVersion furry_hal_version_get_otp_version() {
    if(*(uint64_t*)FURRY_HAL_VERSION_OTP_ADDRESS == 0xFFFFFFFF) {
        return FurryHalVersionOtpVersionEmpty;
    } else {
        if(((FurryHalVersionOTPv1*)FURRY_HAL_VERSION_OTP_ADDRESS)->header_magic ==
           FURRY_HAL_VERSION_OTP_HEADER_MAGIC) {
            // Version 1+
            uint8_t version = ((FurryHalVersionOTPv1*)FURRY_HAL_VERSION_OTP_ADDRESS)->header_version;
            if(version >= FurryHalVersionOtpVersion1 && version <= FurryHalVersionOtpVersion2) {
                return version;
            } else {
                return FurryHalVersionOtpVersionUnknown;
            }
        } else if(((FurryHalVersionOTPv0*)FURRY_HAL_VERSION_OTP_ADDRESS)->board_version <= 10) {
            // Version 0
            return FurryHalVersionOtpVersion0;
        } else {
            // Version Unknown
            return FurryHalVersionOtpVersionUnknown;
        }
    }
}

uint8_t furry_hal_version_get_hw_version() {
    return furry_hal_version.board_version;
}

uint8_t furry_hal_version_get_hw_target() {
    return furry_hal_version.board_target;
}

uint8_t furry_hal_version_get_hw_body() {
    return furry_hal_version.board_body;
}

FurryHalVersionColor furry_hal_version_get_hw_color() {
    return furry_hal_version.board_color;
}

uint8_t furry_hal_version_get_hw_connect() {
    return furry_hal_version.board_connect;
}

FurryHalVersionRegion furry_hal_version_get_hw_region() {
    return FurryHalVersionRegionUnknown;
}

FurryHalVersionRegion furry_hal_version_get_hw_region_otp() {
    return furry_hal_version.board_region;
}

const char* furry_hal_version_get_hw_region_name() {
    return "R00";
}

const char* furry_hal_version_get_hw_region_name_otp() {
    switch(furry_hal_version_get_hw_region_otp()) {
    case FurryHalVersionRegionUnknown:
        return "R00";
    case FurryHalVersionRegionEuRu:
        return "R01";
    case FurryHalVersionRegionUsCaAu:
        return "R02";
    case FurryHalVersionRegionJp:
        return "R03";
    case FurryHalVersionRegionWorld:
        return "R04";
    }
    return "R??";
}

FurryHalVersionDisplay furry_hal_version_get_hw_display() {
    return furry_hal_version.board_display;
}

uint32_t furry_hal_version_get_hw_timestamp() {
    return furry_hal_version.timestamp;
}

const char* furry_hal_version_get_name_ptr() {
    return *furry_hal_version.name == 0x00 ? NULL : furry_hal_version.name;
}

const char* furry_hal_version_get_device_name_ptr() {
    return furry_hal_version.device_name + 1;
}

const char* furry_hal_version_get_ble_local_device_name_ptr() {
    return furry_hal_version.device_name;
}

const uint8_t* furry_hal_version_get_ble_mac() {
    return furry_hal_version.ble_mac;
}

const struct Version* furry_hal_version_get_firmware_version(void) {
    return version_get();
}

size_t furry_hal_version_uid_size() {
    return 64 / 8;
}

const uint8_t* furry_hal_version_uid() {
    if(version_get_custom_name(NULL) != NULL) {
        return (const uint8_t*)&(*((uint32_t*)version_get_custom_name(NULL)));
    }
    return (const uint8_t*)UID64_BASE;
}
