#include "cli_control.h"

#include <cli/cli.h>
#include <cli/cli_i.h>
#include <cli/cli_vcp.h>
#include "cligui_main_i.h"
#include <FreeRTOS.h>

volatile bool gotCallbackSet = false;

FurryStreamBuffer* tx_stream;
FurryStreamBuffer* rx_stream;
static FurryThread* volatile cliThread = NULL;
static void tx_handler_stdout(const char* buffer, size_t size) {
    furry_stream_buffer_send(tx_stream, buffer, size, FurryWaitForever);
}
static void tx_handler(const uint8_t* buffer, size_t size) {
    furry_thread_set_stdout_callback(tx_handler_stdout);
    cliThread = furry_thread_get_current();
    furry_stream_buffer_send(tx_stream, buffer, size, FurryWaitForever);
}
static size_t real_rx_handler(uint8_t* buffer, size_t size, uint32_t timeout) {
    size_t rx_cnt = 0;
    while(size > 0) {
        size_t batch_size = size;
        if(batch_size > 128) batch_size = 128;
        size_t len = furry_stream_buffer_receive(rx_stream, buffer, batch_size, timeout);
        if(len == 0) break;
        size -= len;
        buffer += len;
        rx_cnt += len;
    }
    return rx_cnt;
}

static CliCommand_internal* getInternalCliCommand(Cli* cli, const char* name) {
    FurryString* target_command = furry_string_alloc();
    furry_string_set_str(target_command, name);
    CliCommand_internal* command =
        CliCommandTree_internal_get(((Cli_internal*)cli)->commands, target_command);
    furry_string_free(target_command);
    return command;
}

static void session_init(void) {
}
static void session_deinit(void) {
}
static bool session_connected(void) {
    return true;
}
static CliSession session;
void latch_tx_handler() {
    Cli* global_cli = furry_record_open(RECORD_CLI);

    CliCommand_internal* help_command = getInternalCliCommand(global_cli, "help");
    cliThread = help_command->context;

    furry_thread_set_stdout_callback(tx_handler_stdout);
    if(cliThread != NULL) {
        ((FurryThread_internal*)cliThread)->output.write_callback = &tx_handler_stdout;
    }

    rx_stream = furry_stream_buffer_alloc(128, 1);
    tx_stream = furry_stream_buffer_alloc(128, 1);

    session.tx = &tx_handler;
    session.rx = &real_rx_handler;
    session.tx_stdout = &tx_handler_stdout;
    session.init = &session_init;
    session.deinit = &session_deinit;
    session.is_connected = &session_connected;
    cli_session_close(global_cli);
    cli_session_open(global_cli, &session);
    // Unlock loader-lock
    Loader* loader = furry_record_open(RECORD_LOADER);
    Loader_internal* loader_i = (Loader_internal*)loader;
    loader_i->lock_count = 0;
    furry_record_close(RECORD_CLI);
    furry_record_close(RECORD_LOADER);
}
void unlatch_tx_handler(bool persist) {
    Cli* global_cli = furry_record_open(RECORD_CLI);
    // Stash cliThread if not null
    if(cliThread != NULL) {
        CliCommand_internal* help_command = getInternalCliCommand(global_cli, "help");
        help_command->context = cliThread;
    }
    // Switch to new session
    if(persist) {
        // Use dummy debug firmware function as is_connected
        cli_vcp.is_connected = &furry_hal_version_do_i_belong_here;
    } else {
        // Send CTRL-C
        char eot = 0x03;
        furry_stream_buffer_send(rx_stream, &eot, 1, FurryWaitForever);
    }
    cli_session_open(global_cli, &cli_vcp);
    furry_record_close(RECORD_CLI);
    // Unblock waiting rx handler
    furry_stream_buffer_send(rx_stream, "_", 1, FurryWaitForever);
    // Reconfigure stdout_callback to cli_vcp
    if(cliThread != NULL) {
        ((FurryThread_internal*)cliThread)->output.write_callback = cli_vcp.tx_stdout;
    }
    // At this point, all cli_vcp functions should be back.
    furry_stream_buffer_free(rx_stream);
    furry_stream_buffer_free(tx_stream);
}