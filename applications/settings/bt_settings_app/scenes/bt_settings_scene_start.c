#include "../bt_settings_app.h"
#include <furi_hal_bt.h>
#include <furi_hal_ohs.h>

enum BtSetting {
    BtSettingOff,
    BtSettingOn,
    BtSettingOhs,
    BtSettingNum,
};

enum BtSettingIndex {
    BtSettingIndexSwitchBt,
    BtSettingIndexForgetDev,
};

const char* const bt_settings_text[BtSettingNum] = {
    "OFF",
    "ON",
    "OHS",
};

static void bt_settings_scene_start_var_list_change_callback(VariableItem* item) {
    BtSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, bt_settings_text[index]);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void bt_settings_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    BtSettingsApp* app = context;
    if(index == BtSettingIndexForgetDev) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtSettingsCustomEventForgetDevices);
    }
}

void bt_settings_scene_start_on_enter(void* context) {
    BtSettingsApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    if(furi_hal_bt_is_ble_gatt_gap_supported()) {
        item = variable_item_list_add(
            var_item_list,
            "Bluetooth",
            BtSettingNum,
            bt_settings_scene_start_var_list_change_callback,
            app);
        if(app->bt->bt_settings.mode == BT_MODE_ON) {
            variable_item_set_current_value_index(item, BtSettingOn);
            variable_item_set_current_value_text(item, bt_settings_text[BtSettingOn]);
        } else if(app->settings.mode == BT_MODE_OHS) {
            variable_item_set_current_value_index(item, BtSettingOhs);
            variable_item_set_current_value_text(item, bt_settings_text[BtSettingOhs]);
        } else {
            variable_item_set_current_value_index(item, BtSettingOff);
            variable_item_set_current_value_text(item, bt_settings_text[BtSettingOff]);
        }
        variable_item_list_add(var_item_list, "Forget All Paired Devices", 1, NULL, NULL);
        variable_item_list_set_enter_callback(
            var_item_list, bt_settings_scene_start_var_list_enter_callback, app);
    } else {
        item = variable_item_list_add(var_item_list, "Bluetooth", 1, NULL, NULL);
        variable_item_set_current_value_text(item, "Broken");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BtSettingsAppViewVarItemList);
}

bool bt_settings_scene_start_on_event(void* context, SceneManagerEvent event) {
    BtSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtSettingOn) {
            furi_hal_ohs_stop();
            furi_hal_bt_start_advertising();
            app->bt->bt_settings.mode = BT_MODE_ON;
            consumed = true;
        } else if(event.event == BtSettingOff) {
            furi_hal_ohs_stop();
            furi_hal_bt_stop_advertising();
            app->bt->bt_settings.mode = BT_MODE_OFF;
            consumed = true;
        } else if(event.event == BtSettingOhs) {
            furi_hal_ohs_stop();
            furi_hal_bt_stop_advertising();
            furi_hal_ohs_start();
            app->settings.mode = BT_MODE_OHS;
            consumed = true;
        } else if(event.event == BtSettingsCustomEventForgetDevices) {
            scene_manager_next_scene(app->scene_manager, BtSettingsAppSceneForgetDevConfirm);
            consumed = true;
        }
    }
    return consumed;
}

void bt_settings_scene_start_on_exit(void* context) {
    BtSettingsApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
