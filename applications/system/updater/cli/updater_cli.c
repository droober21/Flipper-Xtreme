
#include <furry.h>
#include <furry_hal.h>
#include <cli/cli.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <toolbox/path.h>
#include <toolbox/tar/tar_archive.h>
#include <toolbox/args.h>
#include <update_util/update_manifest.h>
#include <update_util/lfs_backup.h>
#include <update_util/update_operation.h>

typedef void (*cmd_handler)(FurryString* args);
typedef struct {
    const char* command;
    const cmd_handler handler;
} CliSubcommand;

static void updater_cli_install(FurryString* manifest_path) {
    printf("Verifying update package at '%s'\r\n", furry_string_get_cstr(manifest_path));

    UpdatePrepareResult result = update_operation_prepare(furry_string_get_cstr(manifest_path));
    if(result != UpdatePrepareResultOK) {
        printf(
            "Error: %s. Stopping update.\r\n",
            update_operation_describe_preparation_result(result));
        return;
    }
    printf("OK.\r\nRestarting to apply update. BRB\r\n");
    furry_delay_ms(100);
    furry_hal_power_reset();
}

static void updater_cli_backup(FurryString* args) {
    printf("Backup /int to '%s'\r\n", furry_string_get_cstr(args));
    Storage* storage = furry_record_open(RECORD_STORAGE);
    bool success = lfs_backup_create(storage, furry_string_get_cstr(args));
    furry_record_close(RECORD_STORAGE);
    printf("Result: %s\r\n", success ? "OK" : "FAIL");
}

static void updater_cli_restore(FurryString* args) {
    printf("Restore /int from '%s'\r\n", furry_string_get_cstr(args));
    Storage* storage = furry_record_open(RECORD_STORAGE);
    bool success = lfs_backup_unpack(storage, furry_string_get_cstr(args));
    furry_record_close(RECORD_STORAGE);
    printf("Result: %s\r\n", success ? "OK" : "FAIL");
}

static void updater_cli_help(FurryString* args) {
    UNUSED(args);
    printf("Commands:\r\n"
           "\tinstall /ext/path/to/update.fuf - verify & apply update package\r\n"
           "\tbackup /ext/path/to/backup.tar - create internal storage backup\r\n"
           "\trestore /ext/path/to/backup.tar - restore internal storage backup\r\n");
}

static const CliSubcommand update_cli_subcommands[] = {
    {.command = "install", .handler = updater_cli_install},
    {.command = "backup", .handler = updater_cli_backup},
    {.command = "restore", .handler = updater_cli_restore},
    {.command = "help", .handler = updater_cli_help},
};

static void updater_cli_ep(Cli* cli, FurryString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);
    FurryString* subcommand;
    subcommand = furry_string_alloc();
    if(!args_read_string_and_trim(args, subcommand) || furry_string_empty(args)) {
        updater_cli_help(args);
        furry_string_free(subcommand);
        return;
    }
    for(size_t idx = 0; idx < COUNT_OF(update_cli_subcommands); ++idx) {
        const CliSubcommand* subcmd_def = &update_cli_subcommands[idx];
        if(furry_string_cmp_str(subcommand, subcmd_def->command) == 0) {
            subcmd_def->handler(args);
            furry_string_free(subcommand);
            return;
        }
    }
    furry_string_free(subcommand);
    updater_cli_help(args);
}

static int32_t updater_spawner_thread_worker(void* arg) {
    UNUSED(arg);
    Loader* loader = furry_record_open(RECORD_LOADER);
    loader_start(loader, "UpdaterApp", NULL);
    furry_record_close(RECORD_LOADER);
    return 0;
}

static void updater_spawner_thread_cleanup(FurryThreadState state, void* context) {
    FurryThread* thread = context;
    if(state == FurryThreadStateStopped) {
        furry_thread_free(thread);
    }
}

static void updater_start_app() {
    FurryHalRtcBootMode mode = furry_hal_rtc_get_boot_mode();
    if((mode != FurryHalRtcBootModePreUpdate) && (mode != FurryHalRtcBootModePostUpdate)) {
        return;
    }

    /* We need to spawn a separate thread, because these callbacks are executed 
     * inside loader process, at startup. 
     * So, accessing its record would cause a deadlock 
     */
    FurryThread* thread =
        furry_thread_alloc_ex("UpdateAppSpawner", 768, updater_spawner_thread_worker, NULL);
    furry_thread_set_state_callback(thread, updater_spawner_thread_cleanup);
    furry_thread_set_state_context(thread, thread);
    furry_thread_start(thread);
}

void updater_on_system_start() {
#ifdef SRV_CLI
    Cli* cli = (Cli*)furry_record_open(RECORD_CLI);
    cli_add_command(cli, "update", CliCommandFlagDefault, updater_cli_ep, NULL);
    furry_record_close(RECORD_CLI);
#else
    UNUSED(updater_cli_ep);
#endif
#ifndef FURRY_RAM_EXEC
    updater_start_app();
#else
    UNUSED(updater_start_app);
#endif
}
