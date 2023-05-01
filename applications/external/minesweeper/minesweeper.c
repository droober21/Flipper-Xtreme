#include <furry.h>
#include <furry_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <m-string.h>

#include "assets.h"

#define PLAYFIELD_WIDTH 16
#define PLAYFIELD_HEIGHT 7
#define TILE_WIDTH 8
#define TILE_HEIGHT 8

#define MINECOUNT 20

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef enum {
    TileType0, // this HAS to be in order, for hint assignment to be ez pz
    TileType1,
    TileType2,
    TileType3,
    TileType4,
    TileType5,
    TileType6,
    TileType7,
    TileType8,
    TileTypeUncleared,
    TileTypeFlag,
    TileTypeMine
} TileType;

typedef enum { FieldEmpty, FieldMine } Field;

typedef struct {
    FurryMutex* mutex;
    DialogsApp* dialogs;
    NotificationApp* notifications;
    Field minefield[PLAYFIELD_WIDTH][PLAYFIELD_HEIGHT];
    TileType playfield[PLAYFIELD_WIDTH][PLAYFIELD_HEIGHT];
    int cursor_x;
    int cursor_y;
    int mines_left;
    int fields_cleared;
    int flags_set;
    bool game_started;
    uint32_t game_started_tick;
} Minesweeper;

static void input_callback(InputEvent* input_event, FurryMessageQueue* event_queue) {
    furry_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furry_message_queue_put(event_queue, &event, FurryWaitForever);
}

static void render_callback(Canvas* const canvas, void* ctx) {
    furry_assert(ctx);
    const Minesweeper* minesweeper_state = ctx;
    furry_mutex_acquire(minesweeper_state->mutex, FurryWaitForever);

    FurryString* mineStr;
    FurryString* timeStr;
    mineStr = furry_string_alloc();
    timeStr = furry_string_alloc();

    furry_string_printf(mineStr, "Mines: %d", MINECOUNT - minesweeper_state->flags_set);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, furry_string_get_cstr(mineStr));

    int seconds = 0;
    int minutes = 0;
    if(minesweeper_state->game_started) {
        uint32_t ticks_elapsed = furry_get_tick() - minesweeper_state->game_started_tick;
        seconds = (int)ticks_elapsed / furry_kernel_get_tick_frequency();
        minutes = (int)seconds / 60;
        seconds = seconds % 60;
    }
    furry_string_printf(timeStr, "%01d:%02d", minutes, seconds);
    canvas_draw_str_aligned(canvas, 128, 0, AlignRight, AlignTop, furry_string_get_cstr(timeStr));

    uint8_t* tile_to_draw;

    for(int y = 0; y < PLAYFIELD_HEIGHT; y++) {
        for(int x = 0; x < PLAYFIELD_WIDTH; x++) {
            if(x == minesweeper_state->cursor_x && y == minesweeper_state->cursor_y) {
                canvas_invert_color(canvas);
            }
            switch(minesweeper_state->playfield[x][y]) {
            case TileType0:
                tile_to_draw = tile_0_bits;
                break;
            case TileType1:
                tile_to_draw = tile_1_bits;
                break;
            case TileType2:
                tile_to_draw = tile_2_bits;
                break;
            case TileType3:
                tile_to_draw = tile_3_bits;
                break;
            case TileType4:
                tile_to_draw = tile_4_bits;
                break;
            case TileType5:
                tile_to_draw = tile_5_bits;
                break;
            case TileType6:
                tile_to_draw = tile_6_bits;
                break;
            case TileType7:
                tile_to_draw = tile_7_bits;
                break;
            case TileType8:
                tile_to_draw = tile_8_bits;
                break;
            case TileTypeFlag:
                tile_to_draw = tile_flag_bits;
                break;
            case TileTypeUncleared:
                tile_to_draw = tile_uncleared_bits;
                break;
            case TileTypeMine:
                tile_to_draw = tile_mine_bits;
                break;
            default:
                // this should never happen
                tile_to_draw = tile_mine_bits;
                break;
            }
            canvas_draw_xbm(
                canvas,
                x * TILE_HEIGHT, // x
                8 + (y * TILE_WIDTH), // y
                TILE_WIDTH,
                TILE_HEIGHT,
                tile_to_draw);
            if(x == minesweeper_state->cursor_x && y == minesweeper_state->cursor_y) {
                canvas_invert_color(canvas);
            }
        }
    }

    furry_string_free(mineStr);
    furry_string_free(timeStr);
    furry_mutex_release(minesweeper_state->mutex);
}

static void setup_playfield(Minesweeper* minesweeper_state) {
    int mines_left = MINECOUNT;
    for(int y = 0; y < PLAYFIELD_HEIGHT; y++) {
        for(int x = 0; x < PLAYFIELD_WIDTH; x++) {
            minesweeper_state->minefield[x][y] = FieldEmpty;
            minesweeper_state->playfield[x][y] = TileTypeUncleared;
        }
    }
    while(mines_left > 0) {
        int rand_x = rand() % PLAYFIELD_WIDTH;
        int rand_y = rand() % PLAYFIELD_HEIGHT;
        // make sure first guess isn't a mine
        if(minesweeper_state->minefield[rand_x][rand_y] == FieldEmpty &&
           (minesweeper_state->cursor_x != rand_x || minesweeper_state->cursor_y != rand_y)) {
            minesweeper_state->minefield[rand_x][rand_y] = FieldMine;
            mines_left--;
        }
    }
    minesweeper_state->mines_left = MINECOUNT;
    minesweeper_state->fields_cleared = 0;
    minesweeper_state->flags_set = 0;
    minesweeper_state->game_started_tick = furry_get_tick();
    minesweeper_state->game_started = false;
}

static void place_flag(Minesweeper* minesweeper_state) {
    if(minesweeper_state->playfield[minesweeper_state->cursor_x][minesweeper_state->cursor_y] ==
       TileTypeUncleared) {
        minesweeper_state->playfield[minesweeper_state->cursor_x][minesweeper_state->cursor_y] =
            TileTypeFlag;
        minesweeper_state->flags_set++;
    } else if(
        minesweeper_state->playfield[minesweeper_state->cursor_x][minesweeper_state->cursor_y] ==
        TileTypeFlag) {
        minesweeper_state->playfield[minesweeper_state->cursor_x][minesweeper_state->cursor_y] =
            TileTypeUncleared;
        minesweeper_state->flags_set--;
    }
}

static bool game_lost(Minesweeper* minesweeper_state) {
    // returns true if the player wants to restart, otherwise false
    DialogMessage* message = dialog_message_alloc();

    dialog_message_set_header(message, "Game Over", 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(message, "You hit a mine!", 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "Play again", NULL);

    // Set cursor to initial position
    minesweeper_state->cursor_x = 0;
    minesweeper_state->cursor_y = 0;

    notification_message(minesweeper_state->notifications, &sequence_single_vibro);

    DialogMessageButton choice = dialog_message_show(minesweeper_state->dialogs, message);
    dialog_message_free(message);

    return choice == DialogMessageButtonCenter;
}

static bool game_won(Minesweeper* minesweeper_state) {
    FurryString* tempStr;
    tempStr = furry_string_alloc();

    int seconds = 0;
    int minutes = 0;
    uint32_t ticks_elapsed = furry_get_tick() - minesweeper_state->game_started_tick;
    seconds = (int)ticks_elapsed / furry_kernel_get_tick_frequency();
    minutes = (int)seconds / 60;
    seconds = seconds % 60;

    DialogMessage* message = dialog_message_alloc();
    const char* header_text = "Game won!";
    furry_string_cat_printf(tempStr, "Minefield cleared in %01d:%02d", minutes, seconds);
    dialog_message_set_header(message, header_text, 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(
        message, furry_string_get_cstr(tempStr), 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "Play again", NULL);

    DialogMessageButton choice = dialog_message_show(minesweeper_state->dialogs, message);
    dialog_message_free(message);
    furry_string_free(tempStr);
    return choice == DialogMessageButtonCenter;
}

// returns false if the move loses the game - otherwise true
static bool play_move(Minesweeper* minesweeper_state, int cursor_x, int cursor_y) {
    if(minesweeper_state->playfield[cursor_x][cursor_y] == TileTypeFlag) {
        // we're on a flagged field, do nothing
        return true;
    }
    if(minesweeper_state->minefield[cursor_x][cursor_y] == FieldMine) {
        // player loses - draw mine
        minesweeper_state->playfield[cursor_x][cursor_y] = TileTypeMine;
        return false;
    }

    if(minesweeper_state->playfield[cursor_x][cursor_y] >= TileType1 &&
       minesweeper_state->playfield[cursor_x][cursor_y] <= TileType8) {
        // click on a cleared cell with a number
        // count the flags around
        int flags = 0;
        for(int y = cursor_y - 1; y <= cursor_y + 1; y++) {
            for(int x = cursor_x - 1; x <= cursor_x + 1; x++) {
                if(x == cursor_x && y == cursor_y) {
                    // we're on the cell the user selected, so ignore.
                    continue;
                }
                // make sure we don't go OOB
                if(x >= 0 && x < PLAYFIELD_WIDTH && y >= 0 && y < PLAYFIELD_HEIGHT) {
                    if(minesweeper_state->playfield[x][y] == TileTypeFlag) {
                        flags++;
                    }
                }
            }
        }
        int mines = minesweeper_state->playfield[cursor_x][cursor_y]; // ¯\_(ツ)_/¯
        if(flags == mines) {
            // auto uncover all non-flags around (to win faster ;)
            for(int auto_y = cursor_y - 1; auto_y <= cursor_y + 1; auto_y++) {
                for(int auto_x = cursor_x - 1; auto_x <= cursor_x + 1; auto_x++) {
                    if(auto_x == cursor_x && auto_y == cursor_y) {
                        continue;
                    }
                    if(auto_x >= 0 && auto_x < PLAYFIELD_WIDTH && auto_y >= 0 &&
                       auto_y < PLAYFIELD_HEIGHT) {
                        if(minesweeper_state->playfield[auto_x][auto_y] == TileTypeUncleared) {
                            if(!play_move(minesweeper_state, auto_x, auto_y)) {
                                // flags were wrong, we got a mine!
                                return false;
                            }
                        }
                    }
                }
            }
            // we're done without hitting a mine - so return
            return true;
        }
    }

    // calculate number of surrounding mines.
    int hint = 0;
    for(int y = cursor_y - 1; y <= cursor_y + 1; y++) {
        for(int x = cursor_x - 1; x <= cursor_x + 1; x++) {
            if(x == cursor_x && y == cursor_y) {
                // we're on the cell the user selected, so ignore.
                continue;
            }
            // make sure we don't go OOB
            if(x >= 0 && x < PLAYFIELD_WIDTH && y >= 0 && y < PLAYFIELD_HEIGHT) {
                if(minesweeper_state->minefield[x][y] == FieldMine) {
                    hint++;
                }
            }
        }
    }
    // 〜(￣▽￣〜) don't judge me (〜￣▽￣)〜
    minesweeper_state->playfield[cursor_x][cursor_y] = hint;
    minesweeper_state->fields_cleared++;
    FURRY_LOG_D("Minesweeper", "Setting %d,%d to %d", cursor_x, cursor_y, hint);
    if(hint == 0) {
        // the field is "empty"
        // auto open surrounding fields.
        for(int auto_y = cursor_y - 1; auto_y <= cursor_y + 1; auto_y++) {
            for(int auto_x = cursor_x - 1; auto_x <= cursor_x + 1; auto_x++) {
                if(auto_x == cursor_x && auto_y == cursor_y) {
                    continue;
                }
                if(auto_x >= 0 && auto_x < PLAYFIELD_WIDTH && auto_y >= 0 &&
                   auto_y < PLAYFIELD_HEIGHT) {
                    if(minesweeper_state->playfield[auto_x][auto_y] == TileTypeUncleared) {
                        play_move(minesweeper_state, auto_x, auto_y);
                    }
                }
            }
        }
    }
    return true;
}

static void minesweeper_state_init(Minesweeper* const minesweeper_state) {
    minesweeper_state->cursor_x = minesweeper_state->cursor_y = 0;
    minesweeper_state->game_started = false;
    for(int y = 0; y < PLAYFIELD_HEIGHT; y++) {
        for(int x = 0; x < PLAYFIELD_WIDTH; x++) {
            minesweeper_state->playfield[x][y] = TileTypeUncleared;
        }
    }
}

int32_t minesweeper_app(void* p) {
    UNUSED(p);
    FurryMessageQueue* event_queue = furry_message_queue_alloc(8, sizeof(PluginEvent));

    Minesweeper* minesweeper_state = malloc(sizeof(Minesweeper));
    // setup
    minesweeper_state_init(minesweeper_state);

    minesweeper_state->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    if(!minesweeper_state->mutex) {
        FURRY_LOG_E("Minesweeper", "cannot create mutex\r\n");
        free(minesweeper_state);
        return 255;
    }
    // BEGIN IMPLEMENTATION

    minesweeper_state->dialogs = furry_record_open(RECORD_DIALOGS);
    minesweeper_state->notifications = furry_record_open(RECORD_NOTIFICATION);

    DialogMessage* message = dialog_message_alloc();

    dialog_message_set_header(message, "Minesweeper", 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(
        message,
        "Hold OK pressed to toggle flags.\ngithub.com/panki27",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_message_set_buttons(message, NULL, "Play", NULL);

    dialog_message_show(minesweeper_state->dialogs, message);
    dialog_message_free(message);

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, minesweeper_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    PluginEvent event;
    for(bool processing = true; processing;) {
        FurryStatus event_status = furry_message_queue_get(event_queue, &event, 100);
        if(event_status == FurryStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeShort) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        furry_mutex_acquire(minesweeper_state->mutex, FurryWaitForever);
                        minesweeper_state->cursor_y--;
                        if(minesweeper_state->cursor_y < 0) {
                            minesweeper_state->cursor_y = PLAYFIELD_HEIGHT - 1;
                        }
                        furry_mutex_release(minesweeper_state->mutex);
                        break;
                    case InputKeyDown:
                        furry_mutex_acquire(minesweeper_state->mutex, FurryWaitForever);
                        minesweeper_state->cursor_y++;
                        if(minesweeper_state->cursor_y >= PLAYFIELD_HEIGHT) {
                            minesweeper_state->cursor_y = 0;
                        }
                        furry_mutex_release(minesweeper_state->mutex);
                        break;
                    case InputKeyRight:
                        furry_mutex_acquire(minesweeper_state->mutex, FurryWaitForever);
                        minesweeper_state->cursor_x++;
                        if(minesweeper_state->cursor_x >= PLAYFIELD_WIDTH) {
                            minesweeper_state->cursor_x = 0;
                        }
                        furry_mutex_release(minesweeper_state->mutex);
                        break;
                    case InputKeyLeft:
                        furry_mutex_acquire(minesweeper_state->mutex, FurryWaitForever);
                        minesweeper_state->cursor_x--;
                        if(minesweeper_state->cursor_x < 0) {
                            minesweeper_state->cursor_x = PLAYFIELD_WIDTH - 1;
                        }
                        furry_mutex_release(minesweeper_state->mutex);
                        break;
                    case InputKeyOk:
                        if(!minesweeper_state->game_started) {
                            setup_playfield(minesweeper_state);
                            minesweeper_state->game_started = true;
                        }
                        if(!play_move(
                               minesweeper_state,
                               minesweeper_state->cursor_x,
                               minesweeper_state->cursor_y)) {
                            // ooops. looks like we hit a mine!
                            if(game_lost(minesweeper_state)) {
                                // player wants to restart.
                                setup_playfield(minesweeper_state);
                            } else {
                                // player wants to exit :(
                                processing = false;
                            }
                        } else {
                            // check win condition.
                            if(minesweeper_state->fields_cleared ==
                               (PLAYFIELD_HEIGHT * PLAYFIELD_WIDTH) - MINECOUNT) {
                                if(game_won(minesweeper_state)) {
                                    //player wants to restart
                                    setup_playfield(minesweeper_state);
                                } else {
                                    processing = false;
                                }
                            }
                        }
                        break;
                    case InputKeyBack:
                        // Exit the plugin
                        processing = false;
                        break;
                    default:
                        break;
                    }
                } else if(event.input.type == InputTypeLong) {
                    // hold events
                    FURRY_LOG_D("Minesweeper", "Got a long press!");
                    switch(event.input.key) {
                    case InputKeyUp:
                    case InputKeyDown:
                    case InputKeyRight:
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:
                        FURRY_LOG_D("Minesweeper", "Toggling flag");
                        furry_mutex_acquire(minesweeper_state->mutex, FurryWaitForever);
                        place_flag(minesweeper_state);
                        furry_mutex_release(minesweeper_state->mutex);
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        view_port_update(view_port);
    }
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furry_record_close(RECORD_GUI);
    furry_record_close(RECORD_DIALOGS);
    furry_record_close(RECORD_NOTIFICATION);
    view_port_free(view_port);
    furry_message_queue_free(event_queue);
    furry_mutex_free(minesweeper_state->mutex);
    free(minesweeper_state);

    return 0;
}
