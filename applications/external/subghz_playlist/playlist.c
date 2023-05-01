#include <furry.h>

#include <gui/gui.h>
#include <input/input.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <lib/toolbox/path.h>
#include <SubGHz_Playlist_icons.h>

#include <lib/subghz/protocols/protocol_items.h>
#include <flipper_format/flipper_format_i.h>
#include <applications/main/subghz/subghz_i.h>

#include "flipper_format_stream.h"
#include "flipper_format_stream_i.h"

#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/raw.h>
#include <dolphin/dolphin.h>

#include "playlist_file.h"
#include "canvas_helper.h"

#define PLAYLIST_FOLDER "/ext/subghz/playlist"
#define PLAYLIST_EXT ".txt"
#define TAG "Playlist"

#define STATE_NONE 0
#define STATE_OVERVIEW 1
#define STATE_SENDING 2

#define WIDTH 128
#define HEIGHT 64

typedef struct {
    int current_count; // number of processed files
    int total_count; // number of items in the playlist

    int playlist_repetitions; // number of times to repeat the whole playlist
    int current_playlist_repetition; // current playlist repetition

    // last 3 files
    FurryString* prev_0_path; // current file
    FurryString* prev_1_path; // previous file
    FurryString* prev_2_path; // previous previous file
    FurryString* prev_3_path; // you get the idea

    int state; // current state

    ViewPort* view_port;
} DisplayMeta;

typedef struct {
    FurryThread* thread;
    Storage* storage;
    FlipperFormat* format;

    DisplayMeta* meta;

    FurryString* file_path; // path to the playlist file

    bool ctl_request_exit; // can be set to true if the worker should exit
    bool ctl_pause; // can be set to true if the worker should pause
    bool ctl_request_skip; // can be set to true if the worker should skip the current file
    bool ctl_request_prev; // can be set to true if the worker should go to the previous file

    bool is_running; // indicates if the worker is running
} PlaylistWorker;

typedef struct {
    FurryMutex* mutex;
    FurryMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;

    DisplayMeta* meta;
    PlaylistWorker* worker;

    FurryString* file_path; // Path to the playlist file
} Playlist;

////////////////////////////////////////////////////////////////////////////////

void meta_set_state(DisplayMeta* meta, int state) {
    meta->state = state;
    view_port_update(meta->view_port);
}

static FurryHalSubGhzPreset str_to_preset(FurryString* preset) {
    if(furry_string_cmp_str(preset, "FurryHalSubGhzPresetOok270Async") == 0) {
        return FurryHalSubGhzPresetOok270Async;
    }
    if(furry_string_cmp_str(preset, "FurryHalSubGhzPresetOok650Async") == 0) {
        return FurryHalSubGhzPresetOok650Async;
    }
    if(furry_string_cmp_str(preset, "FurryHalSubGhzPreset2FSKDev238Async") == 0) {
        return FurryHalSubGhzPreset2FSKDev238Async;
    }
    if(furry_string_cmp_str(preset, "FurryHalSubGhzPreset2FSKDev476Async") == 0) {
        return FurryHalSubGhzPreset2FSKDev476Async;
    }
    if(furry_string_cmp_str(preset, "FurryHalSubGhzPresetMSK99_97KbAsync") == 0) {
        return FurryHalSubGhzPresetMSK99_97KbAsync;
    }
    if(furry_string_cmp_str(preset, "FurryHalSubGhzPresetMSK99_97KbAsync") == 0) {
        return FurryHalSubGhzPresetMSK99_97KbAsync;
    }
    return FurryHalSubGhzPresetCustom;
}

// -4: missing protocol
// -3: missing preset
// -2: transmit error
// -1: error
// 0: ok
// 1: resend
// 2: exited
static int playlist_worker_process(
    PlaylistWorker* worker,
    FlipperFormat* fff_file,
    FlipperFormat* fff_data,
    const char* path,
    FurryString* preset,
    FurryString* protocol) {
    // actual sending of .sub file

    if(!flipper_format_file_open_existing(fff_file, path)) {
        FURRY_LOG_E(TAG, "  (TX) Failed to open %s", path);
        return -1;
    }

    // read frequency or default to 433.92MHz
    uint32_t frequency = 0;
    if(!flipper_format_read_uint32(fff_file, "Frequency", &frequency, 1)) {
        FURRY_LOG_W(TAG, "  (TX) Missing Frequency, defaulting to 433.92MHz");
        frequency = 433920000;
    }
    if(!furry_hal_subghz_is_tx_allowed(frequency)) {
        return -2;
    }

    // check if preset is present
    if(!flipper_format_read_string(fff_file, "Preset", preset)) {
        FURRY_LOG_E(TAG, "  (TX) Missing Preset");
        return -3;
    }

    // check if protocol is present
    if(!flipper_format_read_string(fff_file, "Protocol", protocol)) {
        FURRY_LOG_E(TAG, "  (TX) Missing Protocol");
        return -4;
    }

    if(!furry_string_cmp_str(protocol, "RAW")) {
        subghz_protocol_raw_gen_fff_data(fff_data, path);
    } else {
        stream_copy_full(
            flipper_format_get_raw_stream(fff_file), flipper_format_get_raw_stream(fff_data));
    }
    flipper_format_free(fff_file);

    // (try to) send file
    SubGhzEnvironment* environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(environment, (void*)&subghz_protocol_registry);
    SubGhzTransmitter* transmitter =
        subghz_transmitter_alloc_init(environment, furry_string_get_cstr(protocol));

    subghz_transmitter_deserialize(transmitter, fff_data);

    furry_hal_subghz_reset();
    furry_hal_subghz_load_preset(str_to_preset(preset));

    frequency = furry_hal_subghz_set_frequency_and_path(frequency);

    FURRY_LOG_D(TAG, "  (TX) Start sending ...");
    int status = 0;

    furry_hal_subghz_start_async_tx(subghz_transmitter_yield, transmitter);
    while(!furry_hal_subghz_is_async_tx_complete()) {
        if(worker->ctl_request_exit) {
            FURRY_LOG_D(TAG, "    (TX) Requested to exit. Cancelling sending...");
            status = 2;
            break;
        }
        if(worker->ctl_pause) {
            FURRY_LOG_D(TAG, "    (TX) Requested to pause. Cancelling and resending...");
            status = 1;
            break;
        }
        if(worker->ctl_request_skip) {
            worker->ctl_request_skip = false;
            FURRY_LOG_D(TAG, "    (TX) Requested to skip. Cancelling and resending...");
            status = 0;
            break;
        }
        if(worker->ctl_request_prev) {
            worker->ctl_request_prev = false;
            FURRY_LOG_D(TAG, "    (TX) Requested to prev. Cancelling and resending...");
            status = 3;
            break;
        }
        furry_delay_ms(50);
    }

    FURRY_LOG_D(TAG, "  (TX) Done sending.");

    furry_hal_subghz_stop_async_tx();
    furry_hal_subghz_sleep();

    subghz_transmitter_free(transmitter);

    return status;
}

// true - the worker can continue
// false - the worker should exit
static bool playlist_worker_wait_pause(PlaylistWorker* worker) {
    // wait if paused
    while(worker->ctl_pause && !worker->ctl_request_exit) {
        furry_delay_ms(50);
    }
    // exit loop if requested to stop
    if(worker->ctl_request_exit) {
        FURRY_LOG_D(TAG, "Requested to exit. Exiting loop...");
        return false;
    }
    return true;
}

void updatePlayListView(PlaylistWorker* worker, const char* str) {
    furry_string_reset(worker->meta->prev_3_path);
    furry_string_set(worker->meta->prev_3_path, furry_string_get_cstr(worker->meta->prev_2_path));

    furry_string_reset(worker->meta->prev_2_path);
    furry_string_set(worker->meta->prev_2_path, furry_string_get_cstr(worker->meta->prev_1_path));

    furry_string_reset(worker->meta->prev_1_path);
    furry_string_set(worker->meta->prev_1_path, furry_string_get_cstr(worker->meta->prev_0_path));

    furry_string_reset(worker->meta->prev_0_path);
    furry_string_set(worker->meta->prev_0_path, str);

    view_port_update(worker->meta->view_port);
}

static bool playlist_worker_play_playlist_once(
    PlaylistWorker* worker,
    Storage* storage,
    FlipperFormat* fff_head,
    FlipperFormat* fff_data,
    FurryString* data,
    FurryString* preset,
    FurryString* protocol) {
    //
    if(!flipper_format_rewind(fff_head)) {
        FURRY_LOG_E(TAG, "Failed to rewind file");
        return false;
    }

    while(flipper_format_read_string(fff_head, "sub", data)) {
        if(!playlist_worker_wait_pause(worker)) {
            break;
        }

        // update state to sending
        meta_set_state(worker->meta, STATE_SENDING);

        ++worker->meta->current_count;
        const char* str = furry_string_get_cstr(data);

        // it's not fancy, but it works for now :)
        updatePlayListView(worker, str);

        for(int i = 0; i < 1; i++) {
            if(!playlist_worker_wait_pause(worker)) {
                break;
            }

            view_port_update(worker->meta->view_port);

            FURRY_LOG_D(TAG, "(worker) Sending %s", str);

            FlipperFormat* fff_file = flipper_format_file_alloc(storage);

            int status =
                playlist_worker_process(worker, fff_file, fff_data, str, preset, protocol);

            // if there was an error, fff_file is not already freed
            if(status < 0) {
                flipper_format_free(fff_file);
            }

            // re-send file is paused mid-send
            if(status == 1) {
                i -= 1;
                // errored, skip to next file
            } else if(status < 0) {
                break;
                // exited, exit loop
            } else if(status == 2) {
                return false;
            } else if(status == 3) {
                //aqui rebobinamos y avanzamos de nuevo el fichero n-1 veces
                //decrementamos el contador de ficheros enviados
                worker->meta->current_count--;
                if(worker->meta->current_count > 0) {
                    worker->meta->current_count--;
                }
                //rebobinamos el fichero
                if(!flipper_format_rewind(fff_head)) {
                    FURRY_LOG_E(TAG, "Failed to rewind file");
                    return false;
                }
                //avanzamos el fichero n-1 veces
                for(int j = 0; j < worker->meta->current_count; j++) {
                    flipper_format_read_string(fff_head, "sub", data);
                }
                break;
            }
        }
    } // end of loop
    return true;
}

static int32_t playlist_worker_thread(void* ctx) {
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* fff_head = flipper_format_file_alloc(storage);

    PlaylistWorker* worker = ctx;
    if(!flipper_format_file_open_existing(fff_head, furry_string_get_cstr(worker->file_path))) {
        FURRY_LOG_E(TAG, "Failed to open %s", furry_string_get_cstr(worker->file_path));
        worker->is_running = false;

        furry_record_close(RECORD_STORAGE);
        flipper_format_free(fff_head);
        return 0;
    }

    playlist_worker_wait_pause(worker);
    FlipperFormat* fff_data = flipper_format_string_alloc();

    FurryString* data;
    FurryString* preset;
    FurryString* protocol;
    data = furry_string_alloc();
    preset = furry_string_alloc();
    protocol = furry_string_alloc();

    for(int i = 0; i < MAX(1, worker->meta->playlist_repetitions); i++) {
        // infinite repetitions if playlist_repetitions is 0
        if(worker->meta->playlist_repetitions <= 0) {
            --i;
        }
        ++worker->meta->current_playlist_repetition;
        // send playlist
        worker->meta->current_count = 0;
        if(worker->ctl_request_exit) {
            break;
        }

        FURRY_LOG_D(
            TAG,
            "Sending playlist (i %d rep %d b %d)",
            i,
            worker->meta->current_playlist_repetition,
            worker->meta->playlist_repetitions);

        if(!playlist_worker_play_playlist_once(
               worker, storage, fff_head, fff_data, data, preset, protocol)) {
            break;
        }
    }

    furry_record_close(RECORD_STORAGE);
    flipper_format_free(fff_head);

    furry_string_free(data);
    furry_string_free(preset);
    furry_string_free(protocol);

    flipper_format_free(fff_data);

    FURRY_LOG_D(TAG, "Done reading. Read %d data lines.", worker->meta->current_count);
    worker->is_running = false;

    // update state to overview
    meta_set_state(worker->meta, STATE_OVERVIEW);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

void playlist_meta_reset(DisplayMeta* instance) {
    instance->current_count = 0;
    instance->current_playlist_repetition = 0;

    furry_string_reset(instance->prev_0_path);
    furry_string_reset(instance->prev_1_path);
    furry_string_reset(instance->prev_2_path);
    furry_string_reset(instance->prev_3_path);
}

DisplayMeta* playlist_meta_alloc() {
    DisplayMeta* instance = malloc(sizeof(DisplayMeta));
    instance->prev_0_path = furry_string_alloc();
    instance->prev_1_path = furry_string_alloc();
    instance->prev_2_path = furry_string_alloc();
    instance->prev_3_path = furry_string_alloc();
    playlist_meta_reset(instance);
    instance->state = STATE_NONE;
    instance->playlist_repetitions = 1;
    return instance;
}

void playlist_meta_free(DisplayMeta* instance) {
    furry_string_free(instance->prev_0_path);
    furry_string_free(instance->prev_1_path);
    furry_string_free(instance->prev_2_path);
    furry_string_free(instance->prev_3_path);
    free(instance);
}

////////////////////////////////////////////////////////////////////////////////

PlaylistWorker* playlist_worker_alloc(DisplayMeta* meta) {
    PlaylistWorker* instance = malloc(sizeof(PlaylistWorker));

    instance->thread = furry_thread_alloc();
    furry_thread_set_name(instance->thread, "PlaylistWorker");
    furry_thread_set_stack_size(instance->thread, 2048);
    furry_thread_set_context(instance->thread, instance);
    furry_thread_set_callback(instance->thread, playlist_worker_thread);

    instance->meta = meta;
    instance->ctl_pause = true; // require the user to manually start the worker

    instance->file_path = furry_string_alloc();

    return instance;
}

void playlist_worker_free(PlaylistWorker* instance) {
    furry_assert(instance);
    furry_thread_free(instance->thread);
    furry_string_free(instance->file_path);
    free(instance);
}

void playlist_worker_stop(PlaylistWorker* worker) {
    furry_assert(worker);
    furry_assert(worker->is_running);

    worker->ctl_request_exit = true;
    furry_thread_join(worker->thread);
}

bool playlist_worker_running(PlaylistWorker* worker) {
    furry_assert(worker);
    return worker->is_running;
}

void playlist_worker_start(PlaylistWorker* instance, const char* file_path) {
    furry_assert(instance);
    furry_assert(!instance->is_running);

    furry_string_set(instance->file_path, file_path);
    instance->is_running = true;

    // reset meta (current/total)
    playlist_meta_reset(instance->meta);

    FURRY_LOG_D(TAG, "Starting thread...");
    furry_thread_start(instance->thread);
}

////////////////////////////////////////////////////////////////////////////////

static void render_callback(Canvas* canvas, void* ctx) {
    Playlist* app = ctx;
    furry_check(furry_mutex_acquire(app->mutex, FurryWaitForever) == FurryStatusOk);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    FurryString* temp_str;
    temp_str = furry_string_alloc();

    switch(app->meta->state) {
    case STATE_NONE:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, WIDTH / 2, HEIGHT / 2, AlignCenter, AlignCenter, "No playlist loaded");
        break;

    case STATE_OVERVIEW:
        // draw file name
        {
            path_extract_filename(app->file_path, temp_str, true);

            canvas_set_font(canvas, FontPrimary);
            draw_centered_boxed_str(canvas, 1, 1, 15, 6, furry_string_get_cstr(temp_str));
        }

        canvas_set_font(canvas, FontSecondary);

        // draw loaded count
        {
            furry_string_printf(temp_str, "%d Items in playlist", app->meta->total_count);
            canvas_draw_str_aligned(
                canvas, 1, 19, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));

            if(app->meta->playlist_repetitions <= 0) {
                furry_string_set(temp_str, "Repeat: inf");
            } else if(app->meta->playlist_repetitions == 1) {
                furry_string_set(temp_str, "Repeat: no");
            } else {
                furry_string_printf(temp_str, "Repeat: %dx", app->meta->playlist_repetitions);
            }
            canvas_draw_str_aligned(
                canvas, 1, 29, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));
        }

        // draw buttons
        draw_corner_aligned(canvas, 40, 15, AlignCenter, AlignBottom);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, WIDTH / 2 - 7, HEIGHT - 11, AlignLeft, AlignTop, "Start");
        canvas_draw_disc(canvas, WIDTH / 2 - 14, HEIGHT - 8, 3);

        //
        canvas_set_color(canvas, ColorBlack);
        draw_corner_aligned(canvas, 20, 15, AlignLeft, AlignBottom);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 4, HEIGHT - 11, AlignLeft, AlignTop, "R-");

        //
        canvas_set_color(canvas, ColorBlack);
        draw_corner_aligned(canvas, 20, 15, AlignRight, AlignBottom);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, WIDTH - 4, HEIGHT - 11, AlignRight, AlignTop, "R+");

        canvas_set_color(canvas, ColorBlack);

        break;
    case STATE_SENDING:
        canvas_set_color(canvas, ColorBlack);
        if(app->worker->ctl_pause) {
            canvas_draw_icon(canvas, 2, HEIGHT - 8, &I_ButtonRight_4x7);
        } else {
            canvas_draw_box(canvas, 2, HEIGHT - 8, 2, 7);
            canvas_draw_box(canvas, 5, HEIGHT - 8, 2, 7);
        }

        // draw progress text
        {
            canvas_set_font(canvas, FontSecondary);
            furry_string_printf(
                temp_str, "[%d/%d]", app->meta->current_count, app->meta->total_count);
            canvas_draw_str_aligned(
                canvas, 11, HEIGHT - 8, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));

            int h = canvas_string_width(canvas, furry_string_get_cstr(temp_str));
            int xs = 11 + h + 2;
            int w = WIDTH - xs - 1;
            canvas_draw_box(canvas, xs, HEIGHT - 5, w, 1);

            float progress = (float)app->meta->current_count / (float)app->meta->total_count;
            int wp = (int)(progress * w);
            canvas_draw_box(canvas, xs + wp - 1, HEIGHT - 7, 2, 5);
        }

        {
            if(app->meta->playlist_repetitions <= 0) {
                furry_string_printf(temp_str, "[%d/Inf]", app->meta->current_playlist_repetition);
            } else {
                furry_string_printf(
                    temp_str,
                    "[%d/%d]",
                    app->meta->current_playlist_repetition,
                    app->meta->playlist_repetitions);
            }
            canvas_set_color(canvas, ColorBlack);
            int w = canvas_string_width(canvas, furry_string_get_cstr(temp_str));
            draw_corner_aligned(canvas, w + 6, 13, AlignRight, AlignTop);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(
                canvas, WIDTH - 3, 3, AlignRight, AlignTop, furry_string_get_cstr(temp_str));
        }

        // draw last and current file
        {
            canvas_set_color(canvas, ColorBlack);
            canvas_set_font(canvas, FontSecondary);

            // current
            if(!furry_string_empty(app->meta->prev_0_path)) {
                path_extract_filename(app->meta->prev_0_path, temp_str, true);
                int w = canvas_string_width(canvas, furry_string_get_cstr(temp_str));
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rbox(canvas, 1, 1, w + 4, 12, 2);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str_aligned(
                    canvas, 3, 3, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));
            }

            // last 3
            canvas_set_color(canvas, ColorBlack);

            if(!furry_string_empty(app->meta->prev_1_path)) {
                path_extract_filename(app->meta->prev_1_path, temp_str, true);
                canvas_draw_str_aligned(
                    canvas, 3, 15, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));
            }

            if(!furry_string_empty(app->meta->prev_2_path)) {
                path_extract_filename(app->meta->prev_2_path, temp_str, true);
                canvas_draw_str_aligned(
                    canvas, 3, 26, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));
            }

            if(!furry_string_empty(app->meta->prev_3_path)) {
                path_extract_filename(app->meta->prev_3_path, temp_str, true);
                canvas_draw_str_aligned(
                    canvas, 3, 37, AlignLeft, AlignTop, furry_string_get_cstr(temp_str));
            }
        }
        break;
    default:
        break;
    }

    furry_string_free(temp_str);
    furry_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    Playlist* app = ctx;
    furry_message_queue_put(app->input_queue, event, 0);
}

////////////////////////////////////////////////////////////////////////////////

Playlist* playlist_alloc(DisplayMeta* meta) {
    Playlist* app = malloc(sizeof(Playlist));
    app->file_path = furry_string_alloc();
    furry_string_set(app->file_path, PLAYLIST_FOLDER);

    app->meta = meta;
    app->worker = NULL;

    app->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    app->input_queue = furry_message_queue_alloc(32, sizeof(InputEvent));

    // view port
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    // gui
    app->gui = furry_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

void playlist_start_worker(Playlist* app, DisplayMeta* meta) {
    app->worker = playlist_worker_alloc(meta);

    // count playlist items
    Storage* storage = furry_record_open(RECORD_STORAGE);
    app->meta->total_count =
        playlist_count_playlist_items(storage, furry_string_get_cstr(app->file_path));
    furry_record_close(RECORD_STORAGE);

    // start thread
    playlist_worker_start(app->worker, furry_string_get_cstr(app->file_path));
}

void playlist_free(Playlist* app) {
    furry_string_free(app->file_path);

    gui_remove_view_port(app->gui, app->view_port);
    furry_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furry_message_queue_free(app->input_queue);
    furry_mutex_free(app->mutex);

    playlist_meta_free(app->meta);

    free(app);
}

int32_t playlist_app(void* p) {
    UNUSED(p);
    DOLPHIN_DEED(DolphinDeedPluginStart);

    // create playlist folder
    {
        Storage* storage = furry_record_open(RECORD_STORAGE);
        if(!storage_simply_mkdir(storage, PLAYLIST_FOLDER)) {
            FURRY_LOG_E(TAG, "Could not create folder %s", PLAYLIST_FOLDER);
        }
        furry_record_close(RECORD_STORAGE);
    }

    // create app
    DisplayMeta* meta = playlist_meta_alloc();
    Playlist* app = playlist_alloc(meta);
    meta->view_port = app->view_port;

    // Enable power for External CC1101 if it is connected
    furry_hal_subghz_enable_ext_power();
    // Auto switch to internal radio if external radio is not available
    furry_delay_ms(15);
    if(!furry_hal_subghz_check_radio()) {
        furry_hal_subghz_select_radio_type(SubGhzRadioInternal);
        furry_hal_subghz_init_radio_type(SubGhzRadioInternal);
    }

    furry_hal_power_suppress_charge_enter();

    // select playlist file
    {
        DialogsApp* dialogs = furry_record_open(RECORD_DIALOGS);
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, PLAYLIST_EXT, &I_sub1_10px);
        browser_options.base_path = PLAYLIST_FOLDER;

        const bool res =
            dialog_file_browser_show(dialogs, app->file_path, app->file_path, &browser_options);
        furry_record_close(RECORD_DIALOGS);
        // check if a file was selected
        if(!res) {
            FURRY_LOG_E(TAG, "No file selected");
            goto exit_cleanup;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    playlist_start_worker(app, meta);
    meta_set_state(app->meta, STATE_OVERVIEW);

    bool exit_loop = false;
    InputEvent input;
    while(1) { // close application if no file was selected
        furry_check(
            furry_message_queue_get(app->input_queue, &input, FurryWaitForever) == FurryStatusOk);

        switch(input.key) {
        case InputKeyLeft:
            if(app->meta->state == STATE_OVERVIEW) {
                if(input.type == InputTypeShort && app->meta->playlist_repetitions > 0) {
                    --app->meta->playlist_repetitions;
                }
            } else if(app->meta->state == STATE_SENDING) {
                if(input.type == InputTypeShort) {
                    app->worker->ctl_request_prev = true;
                }
            }
            break;

        case InputKeyRight:
            if(app->meta->state == STATE_OVERVIEW) {
                if(input.type == InputTypeShort) {
                    ++app->meta->playlist_repetitions;
                }
            } else if(app->meta->state == STATE_SENDING) {
                if(input.type == InputTypeShort) {
                    app->worker->ctl_request_skip = true;
                }
            }
            break;

        case InputKeyOk:
            if(input.type == InputTypeShort) {
                // toggle pause state
                if(!app->worker->is_running) {
                    app->worker->ctl_pause = false;
                    app->worker->ctl_request_exit = false;
                    playlist_worker_start(app->worker, furry_string_get_cstr(app->file_path));
                } else {
                    app->worker->ctl_pause = !app->worker->ctl_pause;
                }
            }
            break;
        case InputKeyBack:
            FURRY_LOG_D(TAG, "Pressed Back button. Application will exit");
            exit_loop = true;
            break;
        default:
            break;
        }

        furry_mutex_release(app->mutex);

        // exit application
        if(exit_loop == true) {
            break;
        }

        view_port_update(app->view_port);
    }

exit_cleanup:

    furry_hal_power_suppress_charge_exit();
    // Disable power for External CC1101 if it was enabled and module is connected
    furry_hal_subghz_disable_ext_power();
    // Reinit SPI handles for internal radio / nfc
    furry_hal_subghz_init_radio_type(SubGhzRadioInternal);

    if(app->worker != NULL) {
        if(playlist_worker_running(app->worker)) {
            FURRY_LOG_D(TAG, "Thread is still running. Requesting thread to finish ...");
            playlist_worker_stop(app->worker);
        }
        FURRY_LOG_D(TAG, "Freeing Worker ...");
        playlist_worker_free(app->worker);
    }

    FURRY_LOG_D(TAG, "Freeing Playlist ...");
    playlist_free(app);
    return 0;
}
