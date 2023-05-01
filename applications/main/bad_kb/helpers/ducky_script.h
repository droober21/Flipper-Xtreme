#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furry.h>
#include <furry_hal.h>
#include <bt/bt_service/bt_i.h>

#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include "../views/bad_kb_view.h"

#define FILE_BUFFER_LEN 16

typedef enum {
    LevelRssi122_100,
    LevelRssi99_80,
    LevelRssi79_60,
    LevelRssi59_40,
    LevelRssi39_0,
    LevelRssiNum,
    LevelRssiError = 0xFF,
} LevelRssiRange;

extern const uint8_t bt_hid_delays[LevelRssiNum];

extern uint8_t bt_timeout;

typedef enum {
    BadKbStateInit,
    BadKbStateNotConnected,
    BadKbStateIdle,
    BadKbStateWillRun,
    BadKbStateRunning,
    BadKbStateDelay,
    BadKbStateStringDelay,
    BadKbStateWaitForBtn,
    BadKbStateDone,
    BadKbStateScriptError,
    BadKbStateFileError,
} BadKbWorkerState;

struct BadKbState {
    BadKbWorkerState state;
    bool is_bt;
    uint32_t pin;
    uint16_t line_cur;
    uint16_t line_nb;
    uint32_t delay_remain;
    uint16_t error_line;
    char error[64];
};

typedef struct BadKbApp BadKbApp;

typedef struct {
    FurryHalUsbHidConfig hid_cfg;
    FurryThread* thread;
    BadKbState st;

    FurryString* file_path;
    FurryString* keyboard_layout;
    uint8_t file_buf[FILE_BUFFER_LEN + 1];
    uint8_t buf_start;
    uint8_t buf_len;
    bool file_end;

    uint32_t defdelay;
    uint32_t stringdelay;
    uint16_t layout[128];

    FurryString* line;
    FurryString* line_prev;
    uint32_t repeat_cnt;
    uint8_t key_hold_nb;

    FurryString* string_print;
    size_t string_print_pos;

    Bt* bt;
    BadKbApp* app;
} BadKbScript;

BadKbScript* bad_kb_script_open(FurryString* file_path, Bt* bt, BadKbApp* app);

void bad_kb_script_close(BadKbScript* bad_kb);

void bad_kb_script_set_keyboard_layout(BadKbScript* bad_kb, FurryString* layout_path);

void bad_kb_script_start(BadKbScript* bad_kb);

void bad_kb_script_stop(BadKbScript* bad_kb);

void bad_kb_script_toggle(BadKbScript* bad_kb);

BadKbState* bad_kb_script_get_state(BadKbScript* bad_kb);

#define BAD_KB_ADV_NAME_MAX_LEN FURRY_HAL_BT_ADV_NAME_LENGTH
#define BAD_KB_MAC_ADDRESS_LEN GAP_MAC_ADDR_SIZE

typedef enum {
    BadKbAppErrorNoFiles,
    BadKbAppErrorCloseRpc,
} BadKbAppError;

typedef struct {
    char bt_name[BAD_KB_ADV_NAME_MAX_LEN + 1];
    uint8_t bt_mac[BAD_KB_MAC_ADDRESS_LEN];
    FurryHalUsbInterface* usb_mode;
    GapPairing bt_mode;
} BadKbConfig;

struct BadKbApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Widget* widget;
    VariableItemList* var_item_list;
    TextInput* text_input;
    ByteInput* byte_input;

    BadKbAppError error;
    FurryString* file_path;
    FurryString* keyboard_layout;
    BadKb* bad_kb_view;
    BadKbScript* bad_kb_script;

    Bt* bt;
    bool is_bt;
    bool bt_remember;
    BadKbConfig config;
    BadKbConfig prev_config;
    FurryThread* conn_init_thread;
    FurryThread* switch_mode_thread;
};

int32_t bad_kb_config_switch_mode(BadKbApp* app);

#ifdef __cplusplus
}
#endif
