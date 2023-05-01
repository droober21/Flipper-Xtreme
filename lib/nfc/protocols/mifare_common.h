#pragma once

#include <stdint.h>
#include "furry_hal_nfc.h"

typedef enum {
    MifareTypeUnknown,
    MifareTypeUltralight,
    MifareTypeClassic,
    MifareTypeDesfire,
} MifareType;

MifareType mifare_common_get_type(FurryHalNfcADevData* data);
