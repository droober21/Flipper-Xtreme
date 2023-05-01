#include "thread.h"
#include "kernel.h"
#include "memmgr.h"
#include "memmgr_heap.h"
#include "check.h"
#include "common_defines.h"
#include "mutex.h"
#include "string.h"

#include <task.h>
#include "log.h"
#include <furry_hal_rtc.h>
#include <furry_hal_console.h>

#define TAG "FurryThread"

#define THREAD_NOTIFY_INDEX 1 // Index 0 is used for stream buffers

typedef struct FurryThreadStdout FurryThreadStdout;

struct FurryThreadStdout {
    FurryThreadStdoutWriteCallback write_callback;
    FurryString* buffer;
};

struct FurryThread {
    FurryThreadState state;
    int32_t ret;

    FurryThreadCallback callback;
    void* context;

    FurryThreadStateCallback state_callback;
    void* state_context;

    char* name;
    char* appid;

    FurryThreadPriority priority;

    TaskHandle_t task_handle;
    size_t heap_size;

    FurryThreadStdout output;

    // Keep all non-alignable byte types in one place,
    // this ensures that the size of this structure is minimal
    bool is_service;
    bool heap_trace_enabled;

    configSTACK_DEPTH_TYPE stack_size;
};

static size_t __furry_thread_stdout_write(FurryThread* thread, const char* data, size_t size);
static int32_t __furry_thread_stdout_flush(FurryThread* thread);

/** Catch threads that are trying to exit wrong way */
__attribute__((__noreturn__)) void furry_thread_catch() { //-V1082
    asm volatile("nop"); // extra magic
    furry_crash("You are doing it wrong"); //-V779
    __builtin_unreachable();
}

static void furry_thread_set_state(FurryThread* thread, FurryThreadState state) {
    furry_assert(thread);
    thread->state = state;
    if(thread->state_callback) {
        thread->state_callback(state, thread->state_context);
    }
}

static void furry_thread_body(void* context) {
    furry_assert(context);
    FurryThread* thread = context;

    // store thread instance to thread local storage
    furry_assert(pvTaskGetThreadLocalStoragePointer(NULL, 0) == NULL);
    vTaskSetThreadLocalStoragePointer(NULL, 0, thread);

    furry_assert(thread->state == FurryThreadStateStarting);
    furry_thread_set_state(thread, FurryThreadStateRunning);

    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
    if(thread->heap_trace_enabled == true) {
        memmgr_heap_enable_thread_trace((FurryThreadId)task_handle);
    }

    thread->ret = thread->callback(thread->context);

    if(thread->heap_trace_enabled == true) {
        furry_delay_ms(33);
        thread->heap_size = memmgr_heap_get_thread_memory((FurryThreadId)task_handle);
        furry_log_print_format( //-V576
            thread->heap_size ? FurryLogLevelError : FurryLogLevelInfo,
            TAG,
            "%s allocation balance: %u",
            thread->name ? thread->name : "Thread",
            thread->heap_size);
        memmgr_heap_disable_thread_trace((FurryThreadId)task_handle);
    }

    furry_assert(thread->state == FurryThreadStateRunning);

    if(thread->is_service) {
        FURRY_LOG_W(
            TAG,
            "%s service thread TCB memory will not be reclaimed",
            thread->name ? thread->name : "<unknown service>");
    }

    // flush stdout
    __furry_thread_stdout_flush(thread);

    furry_thread_set_state(thread, FurryThreadStateStopped);

    vTaskDelete(NULL);
    furry_thread_catch();
}

FurryThread* furry_thread_alloc() {
    FurryThread* thread = malloc(sizeof(FurryThread));
    thread->output.buffer = furry_string_alloc();
    thread->is_service = false;

    FurryThread* parent = NULL;
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        // TLS is not available, if we called not from thread context
        parent = pvTaskGetThreadLocalStoragePointer(NULL, 0);

        if(parent && parent->appid) {
            furry_thread_set_appid(thread, parent->appid);
        } else {
            furry_thread_set_appid(thread, "unknown");
        }
    } else {
        // if scheduler is not started, we are starting driver thread
        furry_thread_set_appid(thread, "driver");
    }

    FurryHalRtcHeapTrackMode mode = furry_hal_rtc_get_heap_track_mode();
    if(mode == FurryHalRtcHeapTrackModeAll) {
        thread->heap_trace_enabled = true;
    } else if(mode == FurryHalRtcHeapTrackModeTree && furry_thread_get_current_id()) {
        if(parent) thread->heap_trace_enabled = parent->heap_trace_enabled;
    } else {
        thread->heap_trace_enabled = false;
    }

    return thread;
}

FurryThread* furry_thread_alloc_ex(
    const char* name,
    uint32_t stack_size,
    FurryThreadCallback callback,
    void* context) {
    FurryThread* thread = furry_thread_alloc();
    furry_thread_set_name(thread, name);
    furry_thread_set_stack_size(thread, stack_size);
    furry_thread_set_callback(thread, callback);
    furry_thread_set_context(thread, context);
    return thread;
}

void furry_thread_free(FurryThread* thread) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);

    if(thread->name) free((void*)thread->name);
    if(thread->appid) free((void*)thread->appid);
    furry_string_free(thread->output.buffer);

    free(thread);
}

void furry_thread_set_name(FurryThread* thread, const char* name) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    if(thread->name) free((void*)thread->name);
    thread->name = name ? strdup(name) : NULL;
}

void furry_thread_set_appid(FurryThread* thread, const char* appid) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    if(thread->appid) free((void*)thread->appid);
    thread->appid = appid ? strdup(appid) : NULL;
}

void furry_thread_mark_as_service(FurryThread* thread) {
    thread->is_service = true;
}

void furry_thread_set_stack_size(FurryThread* thread, size_t stack_size) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    furry_assert(stack_size % 4 == 0);
    thread->stack_size = stack_size;
}

void furry_thread_set_callback(FurryThread* thread, FurryThreadCallback callback) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    thread->callback = callback;
}

void furry_thread_set_context(FurryThread* thread, void* context) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    thread->context = context;
}

void furry_thread_set_priority(FurryThread* thread, FurryThreadPriority priority) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    furry_assert(priority >= FurryThreadPriorityIdle && priority <= FurryThreadPriorityIsr);
    thread->priority = priority;
}

void furry_thread_set_current_priority(FurryThreadPriority priority) {
    UBaseType_t new_priority = priority ? priority : FurryThreadPriorityNormal;
    vTaskPrioritySet(NULL, new_priority);
}

FurryThreadPriority furry_thread_get_current_priority() {
    return (FurryThreadPriority)uxTaskPriorityGet(NULL);
}

void furry_thread_set_state_callback(FurryThread* thread, FurryThreadStateCallback callback) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    thread->state_callback = callback;
}

void furry_thread_set_state_context(FurryThread* thread, void* context) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    thread->state_context = context;
}

FurryThreadState furry_thread_get_state(FurryThread* thread) {
    furry_assert(thread);
    return thread->state;
}

void furry_thread_start(FurryThread* thread) {
    furry_assert(thread);
    furry_assert(thread->callback);
    furry_assert(thread->state == FurryThreadStateStopped);
    furry_assert(thread->stack_size > 0 && thread->stack_size < (UINT16_MAX * sizeof(StackType_t)));

    furry_thread_set_state(thread, FurryThreadStateStarting);

    uint32_t stack = thread->stack_size / sizeof(StackType_t);
    UBaseType_t priority = thread->priority ? thread->priority : FurryThreadPriorityNormal;
    if(thread->is_service) {
        thread->task_handle = xTaskCreateStatic(
            furry_thread_body,
            thread->name,
            stack,
            thread,
            priority,
            memmgr_alloc_from_pool(sizeof(StackType_t) * stack),
            memmgr_alloc_from_pool(sizeof(StaticTask_t)));
    } else {
        BaseType_t ret = xTaskCreate(
            furry_thread_body, thread->name, stack, thread, priority, &thread->task_handle);
        furry_check(ret == pdPASS);
    }

    furry_check(thread->task_handle);
}

void furry_thread_cleanup_tcb_event(TaskHandle_t task) {
    FurryThread* thread = pvTaskGetThreadLocalStoragePointer(task, 0);
    if(thread) {
        // clear thread local storage
        vTaskSetThreadLocalStoragePointer(task, 0, NULL);

        thread->task_handle = NULL;
    }
}

bool furry_thread_join(FurryThread* thread) {
    furry_assert(thread);

    furry_check(furry_thread_get_current() != thread);

    // !!! IMPORTANT NOTICE !!!
    //
    // If your thread exited, but your app stuck here: some other thread uses
    // all cpu time, which delays kernel from releasing task handle
    while(thread->task_handle) {
        furry_delay_ms(10);
    }

    return true;
}

FurryThreadId furry_thread_get_id(FurryThread* thread) {
    furry_assert(thread);
    return thread->task_handle;
}

void furry_thread_enable_heap_trace(FurryThread* thread) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    thread->heap_trace_enabled = true;
}

void furry_thread_disable_heap_trace(FurryThread* thread) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    thread->heap_trace_enabled = false;
}

size_t furry_thread_get_heap_size(FurryThread* thread) {
    furry_assert(thread);
    furry_assert(thread->heap_trace_enabled == true);
    return thread->heap_size;
}

int32_t furry_thread_get_return_code(FurryThread* thread) {
    furry_assert(thread);
    furry_assert(thread->state == FurryThreadStateStopped);
    return thread->ret;
}

FurryThreadId furry_thread_get_current_id() {
    return xTaskGetCurrentTaskHandle();
}

FurryThread* furry_thread_get_current() {
    FurryThread* thread = pvTaskGetThreadLocalStoragePointer(NULL, 0);
    furry_assert(thread != NULL);
    return thread;
}

void furry_thread_yield() {
    furry_assert(!FURRY_IS_IRQ_MODE());
    taskYIELD();
}

/* Limits */
#define MAX_BITS_TASK_NOTIFY 31U
#define MAX_BITS_EVENT_GROUPS 24U

#define THREAD_FLAGS_INVALID_BITS (~((1UL << MAX_BITS_TASK_NOTIFY) - 1U))
#define EVENT_FLAGS_INVALID_BITS (~((1UL << MAX_BITS_EVENT_GROUPS) - 1U))

uint32_t furry_thread_flags_set(FurryThreadId thread_id, uint32_t flags) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    uint32_t rflags;
    BaseType_t yield;

    if((hTask == NULL) || ((flags & THREAD_FLAGS_INVALID_BITS) != 0U)) {
        rflags = (uint32_t)FurryStatusErrorParameter;
    } else {
        rflags = (uint32_t)FurryStatusError;

        if(FURRY_IS_IRQ_MODE()) {
            yield = pdFALSE;

            (void)xTaskNotifyIndexedFromISR(hTask, THREAD_NOTIFY_INDEX, flags, eSetBits, &yield);
            (void)xTaskNotifyAndQueryIndexedFromISR(
                hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &rflags, NULL);

            portYIELD_FROM_ISR(yield);
        } else {
            (void)xTaskNotifyIndexed(hTask, THREAD_NOTIFY_INDEX, flags, eSetBits);
            (void)xTaskNotifyAndQueryIndexed(hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &rflags);
        }
    }
    /* Return flags after setting */
    return (rflags);
}

uint32_t furry_thread_flags_clear(uint32_t flags) {
    TaskHandle_t hTask;
    uint32_t rflags, cflags;

    if(FURRY_IS_IRQ_MODE()) {
        rflags = (uint32_t)FurryStatusErrorISR;
    } else if((flags & THREAD_FLAGS_INVALID_BITS) != 0U) {
        rflags = (uint32_t)FurryStatusErrorParameter;
    } else {
        hTask = xTaskGetCurrentTaskHandle();

        if(xTaskNotifyAndQueryIndexed(hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &cflags) ==
           pdPASS) {
            rflags = cflags;
            cflags &= ~flags;

            if(xTaskNotifyIndexed(hTask, THREAD_NOTIFY_INDEX, cflags, eSetValueWithOverwrite) !=
               pdPASS) {
                rflags = (uint32_t)FurryStatusError;
            }
        } else {
            rflags = (uint32_t)FurryStatusError;
        }
    }

    /* Return flags before clearing */
    return (rflags);
}

uint32_t furry_thread_flags_get(void) {
    TaskHandle_t hTask;
    uint32_t rflags;

    if(FURRY_IS_IRQ_MODE()) {
        rflags = (uint32_t)FurryStatusErrorISR;
    } else {
        hTask = xTaskGetCurrentTaskHandle();

        if(xTaskNotifyAndQueryIndexed(hTask, THREAD_NOTIFY_INDEX, 0, eNoAction, &rflags) !=
           pdPASS) {
            rflags = (uint32_t)FurryStatusError;
        }
    }

    return (rflags);
}

uint32_t furry_thread_flags_wait(uint32_t flags, uint32_t options, uint32_t timeout) {
    uint32_t rflags, nval;
    uint32_t clear;
    TickType_t t0, td, tout;
    BaseType_t rval;

    if(FURRY_IS_IRQ_MODE()) {
        rflags = (uint32_t)FurryStatusErrorISR;
    } else if((flags & THREAD_FLAGS_INVALID_BITS) != 0U) {
        rflags = (uint32_t)FurryStatusErrorParameter;
    } else {
        if((options & FurryFlagNoClear) == FurryFlagNoClear) {
            clear = 0U;
        } else {
            clear = flags;
        }

        rflags = 0U;
        tout = timeout;

        t0 = xTaskGetTickCount();
        do {
            rval = xTaskNotifyWaitIndexed(THREAD_NOTIFY_INDEX, 0, clear, &nval, tout);

            if(rval == pdPASS) {
                rflags &= flags;
                rflags |= nval;

                if((options & FurryFlagWaitAll) == FurryFlagWaitAll) {
                    if((flags & rflags) == flags) {
                        break;
                    } else {
                        if(timeout == 0U) {
                            rflags = (uint32_t)FurryStatusErrorResource;
                            break;
                        }
                    }
                } else {
                    if((flags & rflags) != 0) {
                        break;
                    } else {
                        if(timeout == 0U) {
                            rflags = (uint32_t)FurryStatusErrorResource;
                            break;
                        }
                    }
                }

                /* Update timeout */
                td = xTaskGetTickCount() - t0;

                if(td > tout) {
                    tout = 0;
                } else {
                    tout -= td;
                }
            } else {
                if(timeout == 0) {
                    rflags = (uint32_t)FurryStatusErrorResource;
                } else {
                    rflags = (uint32_t)FurryStatusErrorTimeout;
                }
            }
        } while(rval != pdFAIL);
    }

    /* Return flags before clearing */
    return (rflags);
}

uint32_t furry_thread_enumerate(FurryThreadId* thread_array, uint32_t array_items) {
    uint32_t i, count;
    TaskStatus_t* task;

    if(FURRY_IS_IRQ_MODE() || (thread_array == NULL) || (array_items == 0U)) {
        count = 0U;
    } else {
        vTaskSuspendAll();

        count = uxTaskGetNumberOfTasks();
        task = pvPortMalloc(count * sizeof(TaskStatus_t));

        if(task != NULL) {
            count = uxTaskGetSystemState(task, count, NULL);

            for(i = 0U; (i < count) && (i < array_items); i++) {
                thread_array[i] = (FurryThreadId)task[i].xHandle;
            }
            count = i;
        }
        (void)xTaskResumeAll();

        vPortFree(task);
    }

    return (count);
}

const char* furry_thread_get_name(FurryThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    const char* name;

    if(FURRY_IS_IRQ_MODE() || (hTask == NULL)) {
        name = NULL;
    } else {
        name = pcTaskGetName(hTask);
    }

    return (name);
}

const char* furry_thread_get_appid(FurryThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    const char* appid = "system";

    if(!FURRY_IS_IRQ_MODE() && (hTask != NULL)) {
        FurryThread* thread = (FurryThread*)pvTaskGetThreadLocalStoragePointer(hTask, 0);
        if(thread) {
            appid = thread->appid;
        }
    }

    return (appid);
}

uint32_t furry_thread_get_stack_space(FurryThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    uint32_t sz;

    if(FURRY_IS_IRQ_MODE() || (hTask == NULL)) {
        sz = 0U;
    } else {
        sz = (uint32_t)(uxTaskGetStackHighWaterMark(hTask) * sizeof(StackType_t));
    }

    return (sz);
}

static size_t __furry_thread_stdout_write(FurryThread* thread, const char* data, size_t size) {
    if(thread->output.write_callback != NULL) {
        thread->output.write_callback(data, size);
    } else {
        furry_hal_console_tx((const uint8_t*)data, size);
    }
    return size;
}

static int32_t __furry_thread_stdout_flush(FurryThread* thread) {
    FurryString* buffer = thread->output.buffer;
    size_t size = furry_string_size(buffer);
    if(size > 0) {
        __furry_thread_stdout_write(thread, furry_string_get_cstr(buffer), size);
        furry_string_reset(buffer);
    }
    return 0;
}

bool furry_thread_set_stdout_callback(FurryThreadStdoutWriteCallback callback) {
    FurryThread* thread = furry_thread_get_current();

    __furry_thread_stdout_flush(thread);
    thread->output.write_callback = callback;

    return true;
}

FurryThreadStdoutWriteCallback furry_thread_get_stdout_callback() {
    FurryThread* thread = furry_thread_get_current();

    return thread->output.write_callback;
}

size_t furry_thread_stdout_write(const char* data, size_t size) {
    FurryThread* thread = furry_thread_get_current();

    if(size == 0 || data == NULL) {
        return __furry_thread_stdout_flush(thread);
    } else {
        if(data[size - 1] == '\n') {
            // if the last character is a newline, we can flush buffer and write data as is, wo buffers
            __furry_thread_stdout_flush(thread);
            __furry_thread_stdout_write(thread, data, size);
        } else {
            // string_cat doesn't work here because we need to write the exact size data
            for(size_t i = 0; i < size; i++) {
                furry_string_push_back(thread->output.buffer, data[i]);
                if(data[i] == '\n') {
                    __furry_thread_stdout_flush(thread);
                }
            }
        }
    }

    return size;
}

int32_t furry_thread_stdout_flush() {
    return __furry_thread_stdout_flush(furry_thread_get_current());
}

void furry_thread_suspend(FurryThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    vTaskSuspend(hTask);
}

void furry_thread_resume(FurryThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    if(FURRY_IS_IRQ_MODE()) {
        xTaskResumeFromISR(hTask);
    } else {
        vTaskResume(hTask);
    }
}

bool furry_thread_is_suspended(FurryThreadId thread_id) {
    TaskHandle_t hTask = (TaskHandle_t)thread_id;
    return eTaskGetState(hTask) == eSuspended;
}
