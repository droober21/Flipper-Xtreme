#include <furry_hal_region.h>
#include <furry_hal_subghz.h>
#include <furry_hal_version.h>

const FurryHalRegion furry_hal_region_zero = {
    .country_code = "00",
    .bands_count = 1,
    .bands = {
        {
            .start = 0,
            .end = 1000000000,
            .power_limit = 12,
            .duty_cycle = 50,
        },
    }};

const FurryHalRegion furry_hal_region_eu_ru = {
    .country_code = "EU",
    .bands_count = 2,
    .bands = {
        {
            .start = 433050000,
            .end = 434790000,
            .power_limit = 12,
            .duty_cycle = 50,
        },
        {
            .start = 868150000,
            .end = 868550000,
            .power_limit = 12,
            .duty_cycle = 50,
        }}};

const FurryHalRegion furry_hal_region_us_ca_au = {
    .country_code = "US",
    .bands_count = 3,
    .bands = {
        {
            .start = 304100000,
            .end = 321950000,
            .power_limit = 12,
            .duty_cycle = 50,
        },
        {
            .start = 433050000,
            .end = 434790000,
            .power_limit = 12,
            .duty_cycle = 50,
        },
        {
            .start = 915000000,
            .end = 928000000,
            .power_limit = 12,
            .duty_cycle = 50,
        }}};

const FurryHalRegion furry_hal_region_jp = {
    .country_code = "JP",
    .bands_count = 2,
    .bands = {
        {
            .start = 312000000,
            .end = 315250000,
            .power_limit = 12,
            .duty_cycle = 50,
        },
        {
            .start = 920500000,
            .end = 923500000,
            .power_limit = 12,
            .duty_cycle = 50,
        }}};

static const FurryHalRegion* furry_hal_region = NULL;

void furry_hal_region_init() {
    FurryHalVersionRegion region = furry_hal_version_get_hw_region();

    if(region == FurryHalVersionRegionUnknown) {
        furry_hal_region = &furry_hal_region_zero;
    } else if(region == FurryHalVersionRegionEuRu) {
        furry_hal_region = &furry_hal_region_eu_ru;
    } else if(region == FurryHalVersionRegionUsCaAu) {
        furry_hal_region = &furry_hal_region_us_ca_au;
    } else if(region == FurryHalVersionRegionJp) {
        furry_hal_region = &furry_hal_region_jp;
    }
}

const FurryHalRegion* furry_hal_region_get() {
    return furry_hal_region;
}

void furry_hal_region_set(FurryHalRegion* region) {
    furry_hal_region = region;
}

bool furry_hal_region_is_provisioned() {
    return furry_hal_region != NULL;
}

const char* furry_hal_region_get_name() {
    if(furry_hal_region) {
        return furry_hal_region->country_code;
    } else {
        return "--";
    }
}

bool furry_hal_region_is_frequency_allowed(uint32_t frequency) {
    bool isAllowed = true;
    if(!furry_hal_region) {
        isAllowed = false;
    }
    const FurryHalRegionBand* band = furry_hal_region_get_band(frequency);
    if(!band) {
        isAllowed = false;
    }
    if(!isAllowed) {
        isAllowed = furry_hal_subghz_is_tx_allowed(frequency);
    }
    return isAllowed;
}

const FurryHalRegionBand* furry_hal_region_get_band(uint32_t frequency) {
    if(!furry_hal_region) {
        return NULL;
    }

    for(size_t i = 0; i < furry_hal_region->bands_count; i++) {
        if(furry_hal_region->bands[i].start <= frequency &&
           furry_hal_region->bands[i].end >= frequency) {
            return &furry_hal_region->bands[i];
        }
    }

    return NULL;
}
