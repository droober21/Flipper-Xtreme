#include "../bad_kb_app.h"
#include "../helpers/ducky_script.h"
#include "furi_hal_power.h"
#include "furi_hal_usb.h"
#include <xtreme.h>

enum VarItemListIndex {
    VarItemListIndexKeyboardLayout,
    VarItemListIndexConnection,
    VarItemListIndexBtRemember,
    VarItemListIndexBtDeviceName,
    VarItemListIndexBtMacAddress,
    VarItemListIndexRandomizeBtMac,
};

void bad_kb_scene_config_connection_callback(VariableItem* item) {
    BadKbApp* bad_kb = variable_item_get_context(item);
    bad_kb->is_bt = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, bad_kb->is_bt ? "BT" : "USB");
    view_dispatcher_send_custom_event(bad_kb->view_dispatcher, VarItemListIndexConnection);
}

void bad_kb_scene_config_bt_remember_callback(VariableItem* item) {
    BadKbApp* bad_kb = variable_item_get_context(item);
    bad_kb->bt_remember = variable_item_get_current_value_index(item);
    XTREME_SETTINGS()->bad_bt_remember = bad_kb->bt_remember;
    XTREME_SETTINGS_SAVE();
    variable_item_set_current_value_text(item, bad_kb->bt_remember ? "ON" : "OFF");
    view_dispatcher_send_custom_event(bad_kb->view_dispatcher, VarItemListIndexBtRemember);
}

void bad_kb_scene_config_var_item_list_callback(void* context, uint32_t index) {
    BadKbApp* bad_kb = context;
    view_dispatcher_send_custom_event(bad_kb->view_dispatcher, index);
}

void bad_kb_scene_config_on_enter(void* context) {
    BadKbApp* bad_kb = context;
    VariableItemList* var_item_list = bad_kb->var_item_list;
    VariableItem* item;

    item = variable_item_list_add(var_item_list, "Keyboard layout", 0, NULL, bad_kb);

    item = variable_item_list_add(
        var_item_list, "Connection", 2, bad_kb_scene_config_connection_callback, bad_kb);
    variable_item_set_current_value_index(item, bad_kb->is_bt);
    variable_item_set_current_value_text(item, bad_kb->is_bt ? "BT" : "USB");
    if(bad_kb->bad_kb_script->has_usb_id) {
        variable_item_set_locked(item, true, "Script has\nID cmd!\nLocked to\nUSB Mode!");
    } else if(bad_kb->bad_kb_script->has_bt_id) {
        variable_item_set_locked(item, true, "Script has\nBT_ID cmd!\nLocked to\nBT Mode!");
    }

    if(bad_kb->is_bt) {
        item = variable_item_list_add(
            var_item_list, "BT Remember", 2, bad_kb_scene_config_bt_remember_callback, bad_kb);
        variable_item_set_current_value_index(item, bad_kb->bt_remember);
        variable_item_set_current_value_text(item, bad_kb->bt_remember ? "ON" : "OFF");

        item = variable_item_list_add(var_item_list, "BT Device Name", 0, NULL, bad_kb);
        if(bad_kb->bad_kb_script->set_bt_id) {
            variable_item_set_locked(item, true, "Script has\nBT_ID cmd!\nLocked to\nset Name!");
        }

        item = variable_item_list_add(var_item_list, "BT MAC Address", 0, NULL, bad_kb);
        if(bad_kb->bt_remember) {
            variable_item_set_locked(item, true, "Remember\nmust be Off!");
        } else if(bad_kb->bad_kb_script->set_bt_id) {
            variable_item_set_locked(item, true, "Script has\nBT_ID cmd!\nLocked to\nset MAC!");
        }

        item = variable_item_list_add(var_item_list, "Randomize BT MAC", 0, NULL, bad_kb);
        if(bad_kb->bt_remember) {
            variable_item_set_locked(item, true, "Remember\nmust be Off!");
        } else if(bad_kb->bad_kb_script->set_bt_id) {
            variable_item_set_locked(item, true, "Script has\nBT_ID cmd!\nLocked to\nset MAC!");
        }
    }

    variable_item_list_set_enter_callback(
        var_item_list, bad_kb_scene_config_var_item_list_callback, bad_kb);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(bad_kb->scene_manager, BadKbSceneConfig));

    view_dispatcher_switch_to_view(bad_kb->view_dispatcher, BadKbAppViewConfig);
}

bool bad_kb_scene_config_on_event(void* context, SceneManagerEvent event) {
    BadKbApp* bad_kb = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(bad_kb->scene_manager, BadKbSceneConfig, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexKeyboardLayout:
            scene_manager_next_scene(bad_kb->scene_manager, BadKbSceneConfigLayout);
            break;
        case VarItemListIndexConnection:
            bad_kb_config_switch_mode(bad_kb);
            break;
        case VarItemListIndexBtRemember:
            bad_kb_config_switch_remember_mode(bad_kb);
            scene_manager_previous_scene(bad_kb->scene_manager);
            scene_manager_next_scene(bad_kb->scene_manager, BadKbSceneConfig);
            break;
        case VarItemListIndexBtDeviceName:
            scene_manager_next_scene(bad_kb->scene_manager, BadKbSceneConfigName);
            break;
        case VarItemListIndexBtMacAddress:
            scene_manager_next_scene(bad_kb->scene_manager, BadKbSceneConfigMac);
            break;
        case VarItemListIndexRandomizeBtMac:
            furi_hal_random_fill_buf(bad_kb->config.bt_mac, BAD_KB_MAC_ADDRESS_LEN);
            bt_set_profile_mac_address(bad_kb->bt, bad_kb->config.bt_mac);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void bad_kb_scene_config_on_exit(void* context) {
    BadKbApp* bad_kb = context;
    VariableItemList* var_item_list = bad_kb->var_item_list;

    variable_item_list_reset(var_item_list);
}
