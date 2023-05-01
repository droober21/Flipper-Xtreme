#include "flipper.h"
#include <applications.h>
#include <furry.h>
#include <furry_hal_version.h>
#include <furry_hal_memory.h>
#include <furry_hal_rtc.h>

#define TAG "Flipper"

static void flipper_print_version(const char* target, const Version* version) {
    if(version) {
        FURRY_LOG_I(
            TAG,
            "\r\n\t%s version:\t%s\r\n"
            "\tBuild date:\t\t%s\r\n"
            "\tGit Commit:\t\t%s (%s)%s\r\n"
            "\tGit Branch:\t\t%s",
            target,
            version_get_version(version),
            version_get_builddate(version),
            version_get_githash(version),
            version_get_gitbranchnum(version),
            version_get_dirty_flag(version) ? " (dirty)" : "",
            version_get_gitbranch(version));
    } else {
        FURRY_LOG_I(TAG, "No build info for %s", target);
    }
}

void flipper_init() {
    flipper_print_version("Firmware", furry_hal_version_get_firmware_version());

    FURRY_LOG_I(TAG, "Boot mode %d, starting services", furry_hal_rtc_get_boot_mode());

    for(size_t i = 0; i < FLIPPER_SERVICES_COUNT; i++) {
        FURRY_LOG_D(TAG, "Starting service %s", FLIPPER_SERVICES[i].name);

        FurryThread* thread = furry_thread_alloc_ex(
            FLIPPER_SERVICES[i].name,
            FLIPPER_SERVICES[i].stack_size,
            FLIPPER_SERVICES[i].app,
            NULL);
        furry_thread_mark_as_service(thread);
        furry_thread_set_appid(thread, FLIPPER_SERVICES[i].appid);

        furry_thread_start(thread);
    }

    FURRY_LOG_I(TAG, "Startup complete");
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t** tcb_ptr,
    StackType_t** stack_ptr,
    uint32_t* stack_size) {
    *tcb_ptr = memmgr_alloc_from_pool(sizeof(StaticTask_t));
    *stack_ptr = memmgr_alloc_from_pool(sizeof(StackType_t) * configIDLE_TASK_STACK_DEPTH);
    *stack_size = configIDLE_TASK_STACK_DEPTH;
}

void vApplicationGetTimerTaskMemory(
    StaticTask_t** tcb_ptr,
    StackType_t** stack_ptr,
    uint32_t* stack_size) {
    *tcb_ptr = memmgr_alloc_from_pool(sizeof(StaticTask_t));
    *stack_ptr = memmgr_alloc_from_pool(sizeof(StackType_t) * configTIMER_TASK_STACK_DEPTH);
    *stack_size = configTIMER_TASK_STACK_DEPTH;
}