#include "power_cli.h"

#include <furry_hal.h>
#include <cli/cli.h>
#include <lib/toolbox/args.h>
#include <power/power_service/power.h>

void power_cli_off(Cli* cli, FurryString* args) {
    UNUSED(cli);
    UNUSED(args);
    Power* power = furry_record_open(RECORD_POWER);
    printf("It's now safe to disconnect USB from your flipper\r\n");
    furry_delay_ms(666);
    power_off(power);
}

void power_cli_reboot(Cli* cli, FurryString* args) {
    UNUSED(cli);
    UNUSED(args);
    power_reboot(PowerBootModeNormal);
}

void power_cli_reboot2dfu(Cli* cli, FurryString* args) {
    UNUSED(cli);
    UNUSED(args);
    power_reboot(PowerBootModeDfu);
}

void power_cli_5v(Cli* cli, FurryString* args) {
    UNUSED(cli);
    if(!furry_string_cmp(args, "0")) {
        furry_hal_power_disable_otg();
    } else if(!furry_string_cmp(args, "1")) {
        furry_hal_power_enable_otg();
    } else {
        cli_print_usage("power_otg", "<1|0>", furry_string_get_cstr(args));
    }
}

void power_cli_3v3(Cli* cli, FurryString* args) {
    UNUSED(cli);
    if(!furry_string_cmp(args, "0")) {
        furry_hal_power_disable_external_3_3v();
    } else if(!furry_string_cmp(args, "1")) {
        furry_hal_power_enable_external_3_3v();
    } else {
        cli_print_usage("power_ext", "<1|0>", furry_string_get_cstr(args));
    }
}

static void power_cli_command_print_usage() {
    printf("Usage:\r\n");
    printf("power <cmd> <args>\r\n");
    printf("Cmd list:\r\n");

    printf("\toff\t - shutdown power\r\n");
    printf("\treboot\t - reboot\r\n");
    printf("\treboot2dfu\t - reboot to dfu bootloader\r\n");
    printf("\t5v <0 or 1>\t - enable or disable 5v ext\r\n");
    if(furry_hal_rtc_is_flag_set(FurryHalRtcFlagDebug)) {
        printf("\t3v3 <0 or 1>\t - enable or disable 3v3 ext\r\n");
    }
}

void power_cli(Cli* cli, FurryString* args, void* context) {
    UNUSED(context);
    FurryString* cmd;
    cmd = furry_string_alloc();

    do {
        if(!args_read_string_and_trim(args, cmd)) {
            power_cli_command_print_usage();
            break;
        }

        if(furry_string_cmp_str(cmd, "off") == 0) {
            power_cli_off(cli, args);
            break;
        }

        if(furry_string_cmp_str(cmd, "reboot") == 0) {
            power_cli_reboot(cli, args);
            break;
        }

        if(furry_string_cmp_str(cmd, "reboot2dfu") == 0) {
            power_cli_reboot2dfu(cli, args);
            break;
        }

        if(furry_string_cmp_str(cmd, "5v") == 0) {
            power_cli_5v(cli, args);
            break;
        }

        if(furry_hal_rtc_is_flag_set(FurryHalRtcFlagDebug)) {
            if(furry_string_cmp_str(cmd, "3v3") == 0) {
                power_cli_3v3(cli, args);
                break;
            }
        }

        power_cli_command_print_usage();
    } while(false);

    furry_string_free(cmd);
}

void power_on_system_start() {
#ifdef SRV_CLI
    Cli* cli = furry_record_open(RECORD_CLI);

    cli_add_command(cli, "power", CliCommandFlagParallelSafe, power_cli, NULL);

    furry_record_close(RECORD_CLI);
#else
    UNUSED(power_cli);
#endif
}
