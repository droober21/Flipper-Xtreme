#include <furry.h>
#include <furry_hal.h>

#include <gui/gui.h>
#include <input/input.h>
#include <dialogs/dialogs.h>
#include <ir_remote_icons.h>
#include <dolphin/dolphin.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "infrared_signal.h"
#include "infrared_remote.h"
#include "infrared_remote_button.h"
#define TAG "IR_Remote"
#define MENU_BTN_TXT_X 36

#include <flipper_format/flipper_format.h>

typedef struct {
    int status;
    ViewPort* view_port;
    FurryString* up_button;
    FurryString* down_button;
    FurryString* left_button;
    FurryString* right_button;
    FurryString* ok_button;
    FurryString* back_button;
    FurryString* up_hold_button;
    FurryString* down_hold_button;
    FurryString* left_hold_button;
    FurryString* right_hold_button;
    FurryString* ok_hold_button;
} IRApp;

// Screen is 128x64 px
static void app_draw_callback(Canvas* canvas, void* ctx) {
    // Show config is incorrect when cannot read the remote file
    // Showing button string in the screen, upper part is short press, lower part is long press
    IRApp* app = ctx;
    if(app->status) {
        canvas_clear(canvas);
        view_port_set_orientation(app->view_port, ViewPortOrientationHorizontal);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 62, 5, AlignCenter, AlignTop, "Config is incorrect.");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 62, 30, AlignCenter, AlignTop, "Please configure map.");
        canvas_draw_str_aligned(canvas, 62, 60, AlignCenter, AlignBottom, "Press Back to Exit.");
    } else {
        canvas_clear(canvas);
        view_port_set_orientation(app->view_port, ViewPortOrientationVertical);
        canvas_draw_icon(canvas, 1, 5, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 1, 15, &I_ButtonDown_7x4);
        canvas_draw_icon(canvas, 2, 23, &I_ButtonLeft_4x7);
        canvas_draw_icon(canvas, 2, 33, &I_ButtonRight_4x7);
        canvas_draw_icon(canvas, 0, 42, &I_Ok_btn_9x9);
        canvas_draw_icon(canvas, 0, 53, &I_back_10px);

        //Labels
        canvas_set_font(canvas, FontSecondary);

        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            8,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->up_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            18,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->down_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            28,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->left_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            38,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->right_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            48,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->ok_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            58,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->back_button));

        canvas_draw_line(canvas, 0, 65, 64, 65);

        canvas_draw_icon(canvas, 1, 70, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 1, 80, &I_ButtonDown_7x4);
        canvas_draw_icon(canvas, 2, 88, &I_ButtonLeft_4x7);
        canvas_draw_icon(canvas, 2, 98, &I_ButtonRight_4x7);
        canvas_draw_icon(canvas, 0, 107, &I_Ok_btn_9x9);
        canvas_draw_icon(canvas, 0, 118, &I_back_10px);

        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            73,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->up_hold_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            83,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->down_hold_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            93,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->left_hold_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            103,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->right_hold_button));
        canvas_draw_str_aligned(
            canvas,
            MENU_BTN_TXT_X,
            113,
            AlignCenter,
            AlignCenter,
            furry_string_get_cstr(app->ok_hold_button));
        canvas_draw_str_aligned(canvas, MENU_BTN_TXT_X, 123, AlignCenter, AlignCenter, "Exit App");
    }
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furry_assert(ctx);

    FurryMessageQueue* event_queue = ctx;
    furry_message_queue_put(event_queue, input_event, FurryWaitForever);
}

int32_t infrared_remote_app(void* p) {
    UNUSED(p);
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(InputEvent));
    DOLPHIN_DEED(DolphinDeedPluginStart);

    // App button string
    IRApp* app = malloc(sizeof(IRApp));
    app->up_button = furry_string_alloc();
    app->down_button = furry_string_alloc();
    app->left_button = furry_string_alloc();
    app->right_button = furry_string_alloc();
    app->ok_button = furry_string_alloc();
    app->back_button = furry_string_alloc();
    app->up_hold_button = furry_string_alloc();
    app->down_hold_button = furry_string_alloc();
    app->left_hold_button = furry_string_alloc();
    app->right_hold_button = furry_string_alloc();
    app->ok_hold_button = furry_string_alloc();
    app->view_port = view_port_alloc();

    // Configure view port
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;

    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    DialogsApp* dialogs = furry_record_open(RECORD_DIALOGS);
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".txt", &I_sub1_10px);
    FurryString* map_file = furry_string_alloc();
    furry_string_set(map_file, "/ext/infrared/remote");
    if(!storage_file_exists(storage, ANY_PATH("infrared/remote"))) {
        storage_common_mkdir(storage, ANY_PATH("infrared/remote")); //Make Folder If dir not exist
    }

    bool res = dialog_file_browser_show(dialogs, map_file, map_file, &browser_options);

    furry_record_close(RECORD_DIALOGS);

    // if user didn't choose anything, free everything and exit
    if(!res) {
        FURRY_LOG_I(TAG, "exit");
        flipper_format_free(ff);
        furry_record_close(RECORD_STORAGE);

        furry_string_free(app->up_button);
        furry_string_free(app->down_button);
        furry_string_free(app->left_button);
        furry_string_free(app->right_button);
        furry_string_free(app->ok_button);
        furry_string_free(app->back_button);
        furry_string_free(app->up_hold_button);
        furry_string_free(app->down_hold_button);
        furry_string_free(app->left_hold_button);
        furry_string_free(app->right_hold_button);
        furry_string_free(app->ok_hold_button);

        view_port_enabled_set(app->view_port, false);
        gui_remove_view_port(gui, app->view_port);
        view_port_free(app->view_port);
        free(app);
        furry_message_queue_free(event_queue);

        furry_record_close(RECORD_GUI);
        return 255;
    }

    InfraredRemote* remote = infrared_remote_alloc();
    FurryString* remote_path = furry_string_alloc();

    InfraredSignal* up_signal = infrared_signal_alloc();
    InfraredSignal* down_signal = infrared_signal_alloc();
    InfraredSignal* left_signal = infrared_signal_alloc();
    InfraredSignal* right_signal = infrared_signal_alloc();
    InfraredSignal* ok_signal = infrared_signal_alloc();
    InfraredSignal* back_signal = infrared_signal_alloc();
    InfraredSignal* up_hold_signal = infrared_signal_alloc();
    InfraredSignal* down_hold_signal = infrared_signal_alloc();
    InfraredSignal* left_hold_signal = infrared_signal_alloc();
    InfraredSignal* right_hold_signal = infrared_signal_alloc();
    InfraredSignal* ok_hold_signal = infrared_signal_alloc();

    bool up_enabled = false;
    bool down_enabled = false;
    bool left_enabled = false;
    bool right_enabled = false;
    bool ok_enabled = false;
    bool back_enabled = false;
    bool up_hold_enabled = false;
    bool down_hold_enabled = false;
    bool left_hold_enabled = false;
    bool right_hold_enabled = false;
    bool ok_hold_enabled = false;

    if(!flipper_format_file_open_existing(ff, furry_string_get_cstr(map_file))) {
        FURRY_LOG_E(TAG, "Could not open MAP file %s", furry_string_get_cstr(map_file));
        app->status = 1;
    } else {
        //Filename Assignment/Check Start

        if(!flipper_format_read_string(ff, "REMOTE", remote_path)) {
            FURRY_LOG_E(TAG, "Could not read REMOTE string");
            app->status = 1;
        } else {
            if(!infrared_remote_load(remote, remote_path)) {
                FURRY_LOG_E(TAG, "Could not load ir file: %s", furry_string_get_cstr(remote_path));
                app->status = 1;
            } else {
                FURRY_LOG_I(TAG, "Loaded REMOTE file: %s", furry_string_get_cstr(remote_path));
            }
        }

        //assign variables to values within map file
        //set missing filenames to N/A
        //assign button signals
        size_t index = 0;

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "UP", app->up_button)) {
            FURRY_LOG_W(TAG, "Could not read UP string");
            furry_string_set(app->up_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->up_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                up_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                up_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "DOWN", app->down_button)) {
            FURRY_LOG_W(TAG, "Could not read DOWN string");
            furry_string_set(app->down_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->down_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                down_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                down_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "LEFT", app->left_button)) {
            FURRY_LOG_W(TAG, "Could not read LEFT string");
            furry_string_set(app->left_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->left_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                left_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                left_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "RIGHT", app->right_button)) {
            FURRY_LOG_W(TAG, "Could not read RIGHT string");
            furry_string_set(app->right_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->right_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                right_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                right_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "OK", app->ok_button)) {
            FURRY_LOG_W(TAG, "Could not read OK string");
            furry_string_set(app->ok_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->ok_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                ok_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                ok_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "BACK", app->back_button)) {
            FURRY_LOG_W(TAG, "Could not read BACK string");
            furry_string_set(app->back_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->back_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                back_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                back_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "UPHOLD", app->up_hold_button)) {
            FURRY_LOG_W(TAG, "Could not read UPHOLD string");
            furry_string_set(app->up_hold_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->up_hold_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                up_hold_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                up_hold_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "DOWNHOLD", app->down_hold_button)) {
            FURRY_LOG_W(TAG, "Could not read DOWNHOLD string");
            furry_string_set(app->down_hold_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->down_hold_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                down_hold_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                down_hold_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "LEFTHOLD", app->left_hold_button)) {
            FURRY_LOG_W(TAG, "Could not read LEFTHOLD string");
            furry_string_set(app->left_hold_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->left_hold_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                left_hold_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                left_hold_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "RIGHTHOLD", app->right_hold_button)) {
            FURRY_LOG_W(TAG, "Could not read RIGHTHOLD string");
            furry_string_set(app->right_hold_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->right_hold_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                right_hold_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                right_hold_enabled = true;
            }
        }

        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "OKHOLD", app->ok_hold_button)) {
            FURRY_LOG_W(TAG, "Could not read OKHOLD string");
            furry_string_set(app->ok_hold_button, "N/A");
        } else {
            if(!infrared_remote_find_button_by_name(
                   remote, furry_string_get_cstr(app->ok_hold_button), &index)) {
                FURRY_LOG_W(TAG, "Error");
            } else {
                ok_hold_signal =
                    infrared_remote_button_get_signal(infrared_remote_get_button(remote, index));
                ok_hold_enabled = true;
            }
        }
    }

    furry_string_free(remote_path);

    flipper_format_free(ff);
    furry_record_close(RECORD_STORAGE);

    bool running = true;
    NotificationApp* notification = furry_record_open(RECORD_NOTIFICATION);

    if(app->status) {
        view_port_update(app->view_port);
        while(running) {
            if(furry_message_queue_get(event_queue, &event, 100) == FurryStatusOk) {
                if(event.type == InputTypeShort) {
                    switch(event.key) {
                    case InputKeyBack:
                        running = false;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    } else {
        view_port_update(app->view_port);
        while(running) {
            if(furry_message_queue_get(event_queue, &event, 100) == FurryStatusOk) {
                // short press signal
                if(event.type == InputTypeShort) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(up_enabled) {
                            infrared_signal_transmit(up_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "up");
                        }
                        break;
                    case InputKeyDown:
                        if(down_enabled) {
                            infrared_signal_transmit(down_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "down");
                        }
                        break;
                    case InputKeyRight:
                        if(right_enabled) {
                            infrared_signal_transmit(right_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "right");
                        }
                        break;
                    case InputKeyLeft:
                        if(left_enabled) {
                            infrared_signal_transmit(left_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "left");
                        }
                        break;
                    case InputKeyOk:
                        if(ok_enabled) {
                            infrared_signal_transmit(ok_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "ok");
                        }
                        break;
                    case InputKeyBack:
                        if(back_enabled) {
                            infrared_signal_transmit(back_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "back");
                        }
                        break;
                    default:
                        running = false;
                        break;
                    }
                    // long press signal
                } else if(event.type == InputTypeLong) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(up_hold_enabled) {
                            infrared_signal_transmit(up_hold_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "up!");
                        }
                        break;
                    case InputKeyDown:
                        if(down_hold_enabled) {
                            infrared_signal_transmit(down_hold_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "down!");
                        }
                        break;
                    case InputKeyRight:
                        if(right_hold_enabled) {
                            infrared_signal_transmit(right_hold_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "right!");
                        }
                        break;
                    case InputKeyLeft:
                        if(left_hold_enabled) {
                            infrared_signal_transmit(left_hold_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "left!");
                        }
                        break;
                    case InputKeyOk:
                        if(ok_hold_enabled) {
                            infrared_signal_transmit(ok_hold_signal);
                            notification_message(notification, &sequence_blink_start_magenta);
                            FURRY_LOG_I(TAG, "ok!");
                        }
                        break;
                    default:
                        running = false;
                        break;
                    }
                } else if(event.type == InputTypeRelease) {
                    notification_message(notification, &sequence_blink_stop);
                }
            }
        }
    }

    // Free all things
    furry_string_free(app->up_button);
    furry_string_free(app->down_button);
    furry_string_free(app->left_button);
    furry_string_free(app->right_button);
    furry_string_free(app->ok_button);
    furry_string_free(app->back_button);
    furry_string_free(app->up_hold_button);
    furry_string_free(app->down_hold_button);
    furry_string_free(app->left_hold_button);
    furry_string_free(app->right_hold_button);
    furry_string_free(app->ok_hold_button);

    infrared_remote_free(remote);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    free(app);
    furry_message_queue_free(event_queue);

    furry_record_close(RECORD_NOTIFICATION);
    furry_record_close(RECORD_GUI);

    return 0;
}
