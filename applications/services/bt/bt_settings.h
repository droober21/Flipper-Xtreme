#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_MODE_OFF,
    BT_MODE_ON,
    BT_MODE_OHS
} BtMode;

typedef struct {
    bool enabled;
    BtMode mode;
} BtSettings;

bool bt_settings_load(BtSettings* bt_settings);

bool bt_settings_save(BtSettings* bt_settings);

#ifdef __cplusplus
}
#endif
