#include "input_i.h"

#include <furry.h>
#include <cli/cli.h>
#include <toolbox/args.h>

static void input_cli_usage() {
    printf("Usage:\r\n");
    printf("input <cmd> <args>\r\n");
    printf("Cmd list:\r\n");
    printf("\tdump\t\t\t - dump input events\r\n");
    printf("\tsend <key> <type>\t - send input event\r\n");
}

static void input_cli_dump_events_callback(const void* value, void* ctx) {
    furry_assert(value);
    furry_assert(ctx);
    FurryMessageQueue* input_queue = ctx;
    furry_message_queue_put(input_queue, value, FurryWaitForever);
}

static void input_cli_dump(Cli* cli, FurryString* args, Input* input) {
    UNUSED(args);
    FurryMessageQueue* input_queue = furry_message_queue_alloc(8, sizeof(InputEvent));
    FurryPubSubSubscription* input_subscription =
        furry_pubsub_subscribe(input->event_pubsub, input_cli_dump_events_callback, input_queue);

    InputEvent input_event;
    printf("Press CTRL+C to stop\r\n");
    while(!cli_cmd_interrupt_received(cli)) {
        if(furry_message_queue_get(input_queue, &input_event, 100) == FurryStatusOk) {
            printf(
                "key: %s type: %s\r\n",
                input_get_key_name(input_event.key),
                input_get_type_name(input_event.type));
        }
    }

    furry_pubsub_unsubscribe(input->event_pubsub, input_subscription);
    furry_message_queue_free(input_queue);
}

static void input_cli_send_print_usage() {
    printf("Invalid arguments. Usage:\r\n");
    printf("\tinput send <key> <type>\r\n");
    printf("\t\t <key>\t - one of 'up', 'down', 'left', 'right', 'back', 'ok'\r\n");
    printf("\t\t <type>\t - one of 'press', 'release', 'short', 'long'\r\n");
}

static void input_cli_send(Cli* cli, FurryString* args, Input* input) {
    UNUSED(cli);
    InputEvent event;
    FurryString* key_str;
    key_str = furry_string_alloc();
    bool parsed = false;

    do {
        // Parse Key
        if(!args_read_string_and_trim(args, key_str)) {
            break;
        }
        if(!furry_string_cmp(key_str, "up")) {
            event.key = InputKeyUp;
        } else if(!furry_string_cmp(key_str, "down")) {
            event.key = InputKeyDown;
        } else if(!furry_string_cmp(key_str, "left")) {
            event.key = InputKeyLeft;
        } else if(!furry_string_cmp(key_str, "right")) {
            event.key = InputKeyRight;
        } else if(!furry_string_cmp(key_str, "ok")) {
            event.key = InputKeyOk;
        } else if(!furry_string_cmp(key_str, "back")) {
            event.key = InputKeyBack;
        } else {
            break;
        }
        // Parse Type
        if(!furry_string_cmp(args, "press")) {
            event.type = InputTypePress;
        } else if(!furry_string_cmp(args, "release")) {
            event.type = InputTypeRelease;
        } else if(!furry_string_cmp(args, "short")) {
            event.type = InputTypeShort;
        } else if(!furry_string_cmp(args, "long")) {
            event.type = InputTypeLong;
        } else {
            break;
        }
        parsed = true;
    } while(false);

    if(parsed) { //-V547
        furry_pubsub_publish(input->event_pubsub, &event);
    } else {
        input_cli_send_print_usage();
    }
    furry_string_free(key_str);
}

void input_cli(Cli* cli, FurryString* args, void* context) {
    furry_assert(cli);
    furry_assert(context);
    Input* input = context;
    FurryString* cmd;
    cmd = furry_string_alloc();

    do {
        if(!args_read_string_and_trim(args, cmd)) {
            input_cli_usage();
            break;
        }
        if(furry_string_cmp_str(cmd, "dump") == 0) {
            input_cli_dump(cli, args, input);
            break;
        }
        if(furry_string_cmp_str(cmd, "send") == 0) {
            input_cli_send(cli, args, input);
            break;
        }

        input_cli_usage();
    } while(false);

    furry_string_free(cmd);
}
