#include "applications.h"
#include <furry.h>
#include <storage/storage.h>
#include "loader/loader.h"
#include "loader_i.h"
#include "applications/main/fap_loader/fap_loader_app.h"
#include <flipper_application/application_manifest.h>
#include <gui/icon_i.h>
#include <core/dangerous_defines.h>
#include <toolbox/stream/file_stream.h>
#include <xtreme.h>
#include <gui/modules/file_browser.h>

#define TAG "LoaderSrv"

#define LOADER_THREAD_FLAG_SHOW_MENU (1 << 0)
#define LOADER_THREAD_FLAG_SHOW_SETTINGS (1 << 1)
#define LOADER_THREAD_FLAG_ALL (LOADER_THREAD_FLAG_SHOW_MENU | LOADER_THREAD_FLAG_SHOW_SETTINGS)

static Loader* loader_instance = NULL;

static FlipperApplication const* loader_find_application_by_name_in_list(
    const char* name,
    const FlipperApplication* list,
    const uint32_t n_apps) {
    for(size_t i = 0; i < n_apps; i++) {
        if(strcmp(name, list[i].name) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

static bool
    loader_start_application(const FlipperApplication* application, const char* arguments) {
    if(application->app == NULL) {
        arguments = application->appid;
        application = loader_find_application_by_name_in_list(
            FAP_LOADER_APP_NAME, FLIPPER_APPS, FLIPPER_APPS_COUNT);
    }
    loader_instance->application = application;

    furry_assert(loader_instance->application_arguments == NULL);
    if(arguments && strlen(arguments) > 0) {
        loader_instance->application_arguments = strdup(arguments);
    }

    FURRY_LOG_I(TAG, "Starting: %s", loader_instance->application->name);

    FurryHalRtcHeapTrackMode mode = furry_hal_rtc_get_heap_track_mode();
    if(mode > FurryHalRtcHeapTrackModeNone) {
        furry_thread_enable_heap_trace(loader_instance->application_thread);
    } else {
        furry_thread_disable_heap_trace(loader_instance->application_thread);
    }

    furry_thread_set_name(loader_instance->application_thread, loader_instance->application->name);
    furry_thread_set_appid(
        loader_instance->application_thread, loader_instance->application->appid);
    furry_thread_set_stack_size(
        loader_instance->application_thread, loader_instance->application->stack_size);
    furry_thread_set_context(
        loader_instance->application_thread, loader_instance->application_arguments);
    furry_thread_set_callback(
        loader_instance->application_thread, loader_instance->application->app);

    furry_thread_start(loader_instance->application_thread);

    return true;
}

const FlipperApplication* loader_find_application_by_name(const char* name) {
    const FlipperApplication* application = NULL;
    application = loader_find_application_by_name_in_list(name, FLIPPER_APPS, FLIPPER_APPS_COUNT);
    if(!application) {
        application = loader_find_application_by_name_in_list(
            name, FLIPPER_SETTINGS_APPS, FLIPPER_SETTINGS_APPS_COUNT);
    }
    if(!application) {
        application = loader_find_application_by_name_in_list(
            name, FLIPPER_SYSTEM_APPS, FLIPPER_SYSTEM_APPS_COUNT);
    }

    return application;
}

static void loader_menu_callback(void* _ctx, uint32_t index) {
    UNUSED(index);
    const FlipperApplication* application = _ctx;

    furry_assert(application->app);
    furry_assert(application->name);

    if(!loader_lock(loader_instance)) {
        FURRY_LOG_E(TAG, "Loader is locked");
        return;
    }

    loader_start_application(application, NULL);
}

static void loader_external_callback(void* _ctx, uint32_t index) {
    UNUSED(index);
    const char* path = _ctx;
    const FlipperApplication* app = loader_find_application_by_name_in_list(
        FAP_LOADER_APP_NAME, FLIPPER_APPS, FLIPPER_APPS_COUNT);

    furry_assert(path);

    if(!loader_lock(loader_instance)) {
        FURRY_LOG_E(TAG, "Loader is locked");
        return;
    }

    loader_start_application(app, path);
}

static void loader_submenu_callback(void* context, uint32_t index) {
    UNUSED(index);
    uint32_t view_id = (uint32_t)context;
    view_dispatcher_switch_to_view(loader_instance->view_dispatcher, view_id);
}

static void loader_cli_print_usage() {
    printf("Usage:\r\n");
    printf("loader <cmd> <args>\r\n");
    printf("Cmd list:\r\n");
    printf("\tlist\t - List available applications\r\n");
    printf("\topen <Application Name:string>\t - Open application by name\r\n");
    printf("\tinfo\t - Show loader state\r\n");
}

static void loader_cli_open(Cli* cli, FurryString* args, Loader* instance) {
    UNUSED(cli);
    if(loader_is_locked(instance)) {
        if(instance->application) {
            furry_assert(instance->application->name);
            printf("Can't start, %s application is running", instance->application->name);
        } else {
            printf("Can't start, furry application is running");
        }
        return;
    }

    FurryString* application_name;
    application_name = furry_string_alloc();

    do {
        if(!args_read_probably_quoted_string_and_trim(args, application_name)) {
            printf("No application provided\r\n");
            break;
        }

        const FlipperApplication* application =
            loader_find_application_by_name(furry_string_get_cstr(application_name));
        if(!application) {
            printf("%s doesn't exists\r\n", furry_string_get_cstr(application_name));
            break;
        }

        furry_string_trim(args);
        if(!loader_start_application(application, furry_string_get_cstr(args))) {
            printf("Can't start, furry application is running");
            return;
        } else {
            // We must to increment lock counter to keep balance
            // TODO: rewrite whole thing, it's complex as hell
            FURRY_CRITICAL_ENTER();
            instance->lock_count++;
            FURRY_CRITICAL_EXIT();
        }
    } while(false);

    furry_string_free(application_name);
}

static void loader_cli_list(Cli* cli, FurryString* args, Loader* instance) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(instance);
    printf("Applications:\r\n");
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        printf("\t%s\r\n", FLIPPER_APPS[i].name);
    }
}

static void loader_cli_info(Cli* cli, FurryString* args, Loader* instance) {
    UNUSED(cli);
    UNUSED(args);
    if(!loader_is_locked(instance)) {
        printf("No application is running\r\n");
    } else {
        printf("Running application: ");
        if(instance->application) {
            furry_assert(instance->application->name);
            printf("%s\r\n", instance->application->name);
        } else {
            printf("unknown\r\n");
        }
    }
}

static void loader_cli(Cli* cli, FurryString* args, void* _ctx) {
    furry_assert(_ctx);
    Loader* instance = _ctx;

    FurryString* cmd;
    cmd = furry_string_alloc();

    do {
        if(!args_read_string_and_trim(args, cmd)) {
            loader_cli_print_usage();
            break;
        }

        if(furry_string_cmp_str(cmd, "list") == 0) {
            loader_cli_list(cli, args, instance);
            break;
        }

        if(furry_string_cmp_str(cmd, "open") == 0) {
            loader_cli_open(cli, args, instance);
            break;
        }

        if(furry_string_cmp_str(cmd, "info") == 0) {
            loader_cli_info(cli, args, instance);
            break;
        }

        loader_cli_print_usage();
    } while(false);

    furry_string_free(cmd);
}

LoaderStatus loader_start(Loader* instance, const char* name, const char* args) {
    UNUSED(instance);
    furry_assert(name);

    const FlipperApplication* application = loader_find_application_by_name(name);

    if(!application) {
        FURRY_LOG_E(TAG, "Can't find application with name %s", name);
        return LoaderStatusErrorUnknownApp;
    }

    if(!loader_lock(loader_instance)) {
        FURRY_LOG_E(TAG, "Loader is locked");
        return LoaderStatusErrorAppStarted;
    }

    if(!loader_start_application(application, args)) {
        return LoaderStatusErrorInternal;
    }

    return LoaderStatusOk;
}

bool loader_lock(Loader* instance) {
    FURRY_CRITICAL_ENTER();
    bool result = false;
    if(instance->lock_count == 0) {
        instance->lock_count++;
        result = true;
    }
    FURRY_CRITICAL_EXIT();
    return result;
}

void loader_unlock(Loader* instance) {
    FURRY_CRITICAL_ENTER();
    if(instance->lock_count > 0) instance->lock_count--;
    FURRY_CRITICAL_EXIT();
}

bool loader_is_locked(const Loader* instance) {
    return instance->lock_count > 0;
}

static void loader_thread_state_callback(FurryThreadState thread_state, void* context) {
    furry_assert(context);

    Loader* instance = context;
    LoaderEvent event;

    if(thread_state == FurryThreadStateRunning) {
        event.type = LoaderEventTypeApplicationStarted;
        furry_pubsub_publish(loader_instance->pubsub, &event);

        if(!(loader_instance->application->flags & FlipperApplicationFlagInsomniaSafe)) {
            furry_hal_power_insomnia_enter();
        }
    } else if(thread_state == FurryThreadStateStopped) {
        FURRY_LOG_I(TAG, "Application stopped. Free heap: %zu", memmgr_get_free_heap());

        if(loader_instance->application_arguments) {
            free(loader_instance->application_arguments);
            loader_instance->application_arguments = NULL;
        }

        if(!(loader_instance->application->flags & FlipperApplicationFlagInsomniaSafe)) {
            furry_hal_power_insomnia_exit();
        }
        loader_unlock(instance);

        event.type = LoaderEventTypeApplicationStopped;
        furry_pubsub_publish(loader_instance->pubsub, &event);
    }
}

static uint32_t loader_hide_menu(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t loader_back_to_primary_menu(void* context) {
    furry_assert(context);
    Submenu* submenu = context;
    submenu_set_selected_item(submenu, 0);
    return LoaderMenuViewPrimary;
}

static Loader* loader_alloc() {
    Loader* instance = malloc(sizeof(Loader));

    instance->application_thread = furry_thread_alloc();

    furry_thread_set_state_context(instance->application_thread, instance);
    furry_thread_set_state_callback(instance->application_thread, loader_thread_state_callback);

    instance->pubsub = furry_pubsub_alloc();

#ifdef SRV_CLI
    instance->cli = furry_record_open(RECORD_CLI);
    cli_add_command(
        instance->cli, RECORD_LOADER, CliCommandFlagParallelSafe, loader_cli, instance);
#else
    UNUSED(loader_cli);
#endif

    instance->loader_thread = furry_thread_get_current_id();

    // Gui
    instance->gui = furry_record_open(RECORD_GUI);
    instance->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);
    // Primary menu
    instance->primary_menu = menu_alloc();
    view_set_previous_callback(menu_get_view(instance->primary_menu), loader_hide_menu);
    view_dispatcher_add_view(
        instance->view_dispatcher, LoaderMenuViewPrimary, menu_get_view(instance->primary_menu));
    // Settings menu
    instance->settings_menu = submenu_alloc();
    view_set_context(submenu_get_view(instance->settings_menu), instance->settings_menu);
    view_set_previous_callback(
        submenu_get_view(instance->settings_menu), loader_back_to_primary_menu);
    view_dispatcher_add_view(
        instance->view_dispatcher,
        LoaderMenuViewSettings,
        submenu_get_view(instance->settings_menu));

    view_dispatcher_enable_queue(instance->view_dispatcher);

    return instance;
}

static void loader_free(Loader* instance) {
    furry_assert(instance);

    if(instance->cli) {
        furry_record_close(RECORD_CLI);
    }

    furry_pubsub_free(instance->pubsub);

    furry_thread_free(instance->application_thread);

    menu_free(loader_instance->primary_menu);
    view_dispatcher_remove_view(loader_instance->view_dispatcher, LoaderMenuViewPrimary);
    submenu_free(loader_instance->settings_menu);
    view_dispatcher_remove_view(loader_instance->view_dispatcher, LoaderMenuViewSettings);
    view_dispatcher_free(loader_instance->view_dispatcher);

    furry_record_close(RECORD_GUI);

    free(instance);
    instance = NULL;
}

bool loader_load_fap_meta(Storage* storage, FurryString* path, FurryString* name, const Icon** icon) {
    *icon = NULL;
    uint8_t* icon_buf = malloc(CUSTOM_ICON_MAX_SIZE);
    if(!fap_loader_load_name_and_icon(path, storage, &icon_buf, name)) {
        free(icon_buf);
        icon_buf = NULL;
        return false;
    }
    *icon = malloc(sizeof(Icon));
    FURRY_CONST_ASSIGN((*icon)->frame_count, 1);
    FURRY_CONST_ASSIGN((*icon)->frame_rate, 0);
    FURRY_CONST_ASSIGN((*icon)->width, 10);
    FURRY_CONST_ASSIGN((*icon)->height, 10);
    FURRY_CONST_ASSIGN_PTR((*icon)->frames, malloc(sizeof(const uint8_t*)));
    FURRY_CONST_ASSIGN_PTR((*icon)->frames[0], icon_buf);
    return true;
}

static void loader_build_menu() {
    FURRY_LOG_I(TAG, "Building main menu");
    size_t i;
    for(i = 0; i < FLIPPER_APPS_COUNT; i++) {
        menu_add_item(
            loader_instance->primary_menu,
            FLIPPER_APPS[i].name,
            FLIPPER_APPS[i].icon,
            i,
            loader_menu_callback,
            (void*)&FLIPPER_APPS[i]);
    }
    menu_add_item(
        loader_instance->primary_menu,
        "Settings",
        &A_Settings_14,
        i++,
        loader_submenu_callback,
        (void*)LoaderMenuViewSettings);

    Storage* storage = furry_record_open(RECORD_STORAGE);
    FurryString* path = furry_string_alloc();
    FurryString* name = furry_string_alloc();
    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, XTREME_APPS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, path)) {
            furry_string_replace_all(path, "\r", "");
            furry_string_replace_all(path, "\n", "");
            const Icon* icon;
            if(!loader_load_fap_meta(storage, path, name, &icon)) continue;
            menu_add_item(
                loader_instance->primary_menu,
                strdup(furry_string_get_cstr(name)),
                icon,
                i++,
                loader_external_callback,
                (void*)strdup(furry_string_get_cstr(path)));
        }
    }
    file_stream_close(stream);
    stream_free(stream);
    furry_string_free(name);
    furry_string_free(path);
    furry_record_close(RECORD_STORAGE);
}

static void loader_build_submenu() {
    FURRY_LOG_I(TAG, "Building settings menu");
    for(size_t i = 0; i < FLIPPER_SETTINGS_APPS_COUNT; i++) {
        submenu_add_item(
            loader_instance->settings_menu,
            FLIPPER_SETTINGS_APPS[i].name,
            i,
            loader_menu_callback,
            (void*)&FLIPPER_SETTINGS_APPS[i]);
    }
}

void loader_show_menu() {
    furry_assert(loader_instance);
    furry_thread_flags_set(loader_instance->loader_thread, LOADER_THREAD_FLAG_SHOW_MENU);
}

void loader_show_settings() {
    furry_assert(loader_instance);
    furry_thread_flags_set(loader_instance->loader_thread, LOADER_THREAD_FLAG_SHOW_SETTINGS);
}

void loader_update_menu() {
    menu_reset(loader_instance->primary_menu);
    loader_build_menu();
}

int32_t loader_srv(void* p) {
    UNUSED(p);
    FURRY_LOG_I(TAG, "Executing system start hooks");
    for(size_t i = 0; i < FLIPPER_ON_SYSTEM_START_COUNT; i++) {
        FLIPPER_ON_SYSTEM_START[i]();
    }

    FURRY_LOG_I(TAG, "Starting");
    loader_instance = loader_alloc();

    loader_build_menu();
    loader_build_submenu();

    FURRY_LOG_I(TAG, "Started");

    furry_record_create(RECORD_LOADER, loader_instance);

    if(FLIPPER_AUTORUN_APP_NAME && strlen(FLIPPER_AUTORUN_APP_NAME)) {
        loader_start(loader_instance, FLIPPER_AUTORUN_APP_NAME, NULL);
    }

    while(1) {
        uint32_t flags =
            furry_thread_flags_wait(LOADER_THREAD_FLAG_ALL, FurryFlagWaitAny, FurryWaitForever);
        if(flags & LOADER_THREAD_FLAG_SHOW_MENU) {
            menu_set_selected_item(loader_instance->primary_menu, 0);
            view_dispatcher_switch_to_view(
                loader_instance->view_dispatcher, LoaderMenuViewPrimary);
            view_dispatcher_run(loader_instance->view_dispatcher);
        } else if(flags & LOADER_THREAD_FLAG_SHOW_SETTINGS) {
            submenu_set_selected_item(loader_instance->settings_menu, 0);
            view_set_previous_callback(
                submenu_get_view(loader_instance->settings_menu), loader_hide_menu);
            view_dispatcher_switch_to_view(
                loader_instance->view_dispatcher, LoaderMenuViewSettings);
            view_dispatcher_run(loader_instance->view_dispatcher);
            view_set_previous_callback(
                submenu_get_view(loader_instance->settings_menu), loader_back_to_primary_menu);
        }
    }

    furry_record_destroy(RECORD_LOADER);
    loader_free(loader_instance);

    return 0;
}

FurryPubSub* loader_get_pubsub(Loader* instance) {
    return instance->pubsub;
}
