#include "check.h"
#include "common_defines.h"

#include <stm32wbxx.h>
#include <furry_hal_console.h>
#include <furry_hal_power.h>
#include <furry_hal_rtc.h>
#include <furry_hal_debug.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdlib.h>

PLACE_IN_SECTION("MB_MEM2") const char* __furry_check_message = NULL;
PLACE_IN_SECTION("MB_MEM2") uint32_t __furry_check_registers[13] = {0};

/** Load r12 value to __furry_check_message and store registers to __furry_check_registers */
#define GET_MESSAGE_AND_STORE_REGISTERS()               \
    asm volatile("ldr r11, =__furry_check_message    \n" \
                 "str r12, [r11]                    \n" \
                 "ldr r12, =__furry_check_registers  \n" \
                 "stm r12, {r0-r11}                 \n" \
                 "str lr, [r12, #48]                \n" \
                 :                                      \
                 :                                      \
                 : "memory");

/** Restore registers and halt MCU
 * 
 * - Always use it with GET_MESSAGE_AND_STORE_REGISTERS
 * - If debugger is(was) connected this routine will raise bkpt
 * - If debugger is not connected then endless loop
 * 
 */
#define RESTORE_REGISTERS_AND_HALT_MCU(debug)           \
    register const bool r0 asm("r0") = debug;           \
    asm volatile("cbnz  r0, with_debugger%=         \n" \
                 "ldr   r12, =__furry_check_registers\n" \
                 "ldm   r12, {r0-r11}               \n" \
                 "loop%=:                           \n" \
                 "wfi                               \n" \
                 "b     loop%=                      \n" \
                 "with_debugger%=:                  \n" \
                 "ldr   r12, =__furry_check_registers\n" \
                 "ldm   r12, {r0-r11}               \n" \
                 "debug_loop%=:                     \n" \
                 "bkpt  0x00                        \n" \
                 "wfi                               \n" \
                 "b     debug_loop%=                \n" \
                 :                                      \
                 : "r"(r0)                              \
                 : "memory");

extern size_t xPortGetTotalHeapSize(void);
extern size_t xPortGetFreeHeapSize(void);
extern size_t xPortGetMinimumEverFreeHeapSize(void);

static void __furry_put_uint32_as_text(uint32_t data) {
    char tmp_str[] = "-2147483648";
    itoa(data, tmp_str, 10);
    furry_hal_console_puts(tmp_str);
}

static void __furry_put_uint32_as_hex(uint32_t data) {
    char tmp_str[] = "0xFFFFFFFF";
    itoa(data, tmp_str, 16);
    furry_hal_console_puts(tmp_str);
}

static void __furry_print_register_info() {
    // Print registers
    for(uint8_t i = 0; i < 12; i++) {
        furry_hal_console_puts("\r\n\tr");
        __furry_put_uint32_as_text(i);
        furry_hal_console_puts(" : ");
        __furry_put_uint32_as_hex(__furry_check_registers[i]);
    }

    furry_hal_console_puts("\r\n\tlr : ");
    __furry_put_uint32_as_hex(__furry_check_registers[12]);
}

static void __furry_print_stack_info() {
    furry_hal_console_puts("\r\n\tstack watermark: ");
    __furry_put_uint32_as_text(uxTaskGetStackHighWaterMark(NULL) * 4);
}

static void __furry_print_heap_info() {
    furry_hal_console_puts("\r\n\t     heap total: ");
    __furry_put_uint32_as_text(xPortGetTotalHeapSize());
    furry_hal_console_puts("\r\n\t      heap free: ");
    __furry_put_uint32_as_text(xPortGetFreeHeapSize());
    furry_hal_console_puts("\r\n\t heap watermark: ");
    __furry_put_uint32_as_text(xPortGetMinimumEverFreeHeapSize());
}

static void __furry_print_name(bool isr) {
    if(isr) {
        furry_hal_console_puts("[ISR ");
        __furry_put_uint32_as_text(__get_IPSR());
        furry_hal_console_puts("] ");
    } else {
        const char* name = pcTaskGetName(NULL);
        if(name == NULL) {
            furry_hal_console_puts("[main] ");
        } else {
            furry_hal_console_puts("[");
            furry_hal_console_puts(name);
            furry_hal_console_puts("] ");
        }
    }
}

FURRY_NORETURN void __furry_crash() {
    __disable_irq();
    GET_MESSAGE_AND_STORE_REGISTERS();

    bool isr = FURRY_IS_IRQ_MODE();

    if(__furry_check_message == NULL) {
        __furry_check_message = "Fatal Error";
    } else if(__furry_check_message == (void*)__FURRY_ASSERT_MESSAGE_FLAG) {
        __furry_check_message = "furry_assert failed";
    } else if(__furry_check_message == (void*)__FURRY_CHECK_MESSAGE_FLAG) {
        __furry_check_message = "furry_check failed";
    }

    furry_hal_console_puts("\r\n\033[0;31m[CRASH]");
    __furry_print_name(isr);
    furry_hal_console_puts(__furry_check_message);

    __furry_print_register_info();
    if(!isr) {
        __furry_print_stack_info();
    }
    __furry_print_heap_info();

#ifndef FURRY_DEBUG
    // Check if debug enabled by DAP
    // https://developer.arm.com/documentation/ddi0403/d/Debug-Architecture/ARMv7-M-Debug/Debug-register-support-in-the-SCS/Debug-Halting-Control-and-Status-Register--DHCSR?lang=en
    bool debug = CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
    if(debug) {
#endif
        furry_hal_console_puts("\r\nSystem halted. Connect debugger for more info\r\n");
        furry_hal_console_puts("\033[0m\r\n");
        furry_hal_debug_enable();

        RESTORE_REGISTERS_AND_HALT_MCU(true);
#ifndef FURRY_DEBUG
    } else {
        furry_hal_rtc_set_fault_data((uint32_t)__furry_check_message);
        furry_hal_console_puts("\r\nRebooting system.\r\n");
        furry_hal_console_puts("\033[0m\r\n");
        furry_hal_power_reset();
    }
#endif
    __builtin_unreachable();
}

FURRY_NORETURN void __furry_halt() {
    __disable_irq();
    GET_MESSAGE_AND_STORE_REGISTERS();

    bool isr = FURRY_IS_IRQ_MODE();

    if(__furry_check_message == NULL) {
        __furry_check_message = "System halt requested.";
    }

    furry_hal_console_puts("\r\n\033[0;31m[HALT]");
    __furry_print_name(isr);
    furry_hal_console_puts(__furry_check_message);
    furry_hal_console_puts("\r\nSystem halted. Bye-bye!\r\n");
    furry_hal_console_puts("\033[0m\r\n");

    // Check if debug enabled by DAP
    // https://developer.arm.com/documentation/ddi0403/d/Debug-Architecture/ARMv7-M-Debug/Debug-register-support-in-the-SCS/Debug-Halting-Control-and-Status-Register--DHCSR?lang=en
    bool debug = CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
    RESTORE_REGISTERS_AND_HALT_MCU(debug);

    __builtin_unreachable();
}
