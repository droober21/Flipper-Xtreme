/**
 * @file thread.h
 * Furry: Furry Thread API
 */

#pragma once

#include "base.h"
#include "common_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/** FurryThreadState */
typedef enum {
    FurryThreadStateStopped,
    FurryThreadStateStarting,
    FurryThreadStateRunning,
} FurryThreadState;

/** FurryThreadPriority */
typedef enum {
    FurryThreadPriorityNone = 0, /**< Uninitialized, choose system default */
    FurryThreadPriorityIdle = 1, /**< Idle priority */
    FurryThreadPriorityLowest = 14, /**< Lowest */
    FurryThreadPriorityLow = 15, /**< Low */
    FurryThreadPriorityNormal = 16, /**< Normal */
    FurryThreadPriorityHigh = 17, /**< High */
    FurryThreadPriorityHighest = 18, /**< Highest */
    FurryThreadPriorityIsr = 32, /**< Deffered Isr (highest possible) */
} FurryThreadPriority;

/** FurryThread anonymous structure */
typedef struct FurryThread FurryThread;

/** FurryThreadId proxy type to OS low level functions */
typedef void* FurryThreadId;

/** FurryThreadCallback Your callback to run in new thread
 * @warning    never use osThreadExit in FurryThread
 */
typedef int32_t (*FurryThreadCallback)(void* context);

/** Write to stdout callback
 * @param      data     pointer to data
 * @param      size     data size @warning your handler must consume everything
 */
typedef void (*FurryThreadStdoutWriteCallback)(const char* data, size_t size);

/** FurryThread state change callback called upon thread state change
 * @param      state    new thread state
 * @param      context  callback context
 */
typedef void (*FurryThreadStateCallback)(FurryThreadState state, void* context);

/** Allocate FurryThread
 *
 * @return     FurryThread instance
 */
FurryThread* furry_thread_alloc();

/** Allocate FurryThread, shortcut version
 * 
 * @param name 
 * @param stack_size 
 * @param callback 
 * @param context 
 * @return FurryThread* 
 */
FurryThread* furry_thread_alloc_ex(
    const char* name,
    uint32_t stack_size,
    FurryThreadCallback callback,
    void* context);

/** Release FurryThread
 *
 * @warning    see furry_thread_join
 *
 * @param      thread  FurryThread instance
 */
void furry_thread_free(FurryThread* thread);

/** Set FurryThread name
 *
 * @param      thread  FurryThread instance
 * @param      name    string
 */
void furry_thread_set_name(FurryThread* thread, const char* name);

/**
 * @brief Set FurryThread appid
 * Technically, it is like a "process id", but it is not a system-wide unique identifier.
 * All threads spawned by the same app will have the same appid.
 * 
 * @param thread 
 * @param appid 
 */
void furry_thread_set_appid(FurryThread* thread, const char* appid);

/** Mark thread as service
 * The service cannot be stopped or removed, and cannot exit from the thread body
 * 
 * @param thread 
 */
void furry_thread_mark_as_service(FurryThread* thread);

/** Set FurryThread stack size
 *
 * @param      thread      FurryThread instance
 * @param      stack_size  stack size in bytes
 */
void furry_thread_set_stack_size(FurryThread* thread, size_t stack_size);

/** Set FurryThread callback
 *
 * @param      thread    FurryThread instance
 * @param      callback  FurryThreadCallback, called upon thread run
 */
void furry_thread_set_callback(FurryThread* thread, FurryThreadCallback callback);

/** Set FurryThread context
 *
 * @param      thread   FurryThread instance
 * @param      context  pointer to context for thread callback
 */
void furry_thread_set_context(FurryThread* thread, void* context);

/** Set FurryThread priority
 *
 * @param      thread   FurryThread instance
 * @param      priority FurryThreadPriority value
 */
void furry_thread_set_priority(FurryThread* thread, FurryThreadPriority priority);

/** Set current thread priority
 *
 * @param      priority FurryThreadPriority value
 */
void furry_thread_set_current_priority(FurryThreadPriority priority);

/** Get current thread priority
 *
 * @return     FurryThreadPriority value
 */
FurryThreadPriority furry_thread_get_current_priority();

/** Set FurryThread state change callback
 *
 * @param      thread    FurryThread instance
 * @param      callback  state change callback
 */
void furry_thread_set_state_callback(FurryThread* thread, FurryThreadStateCallback callback);

/** Set FurryThread state change context
 *
 * @param      thread   FurryThread instance
 * @param      context  pointer to context
 */
void furry_thread_set_state_context(FurryThread* thread, void* context);

/** Get FurryThread state
 *
 * @param      thread  FurryThread instance
 *
 * @return     thread state from FurryThreadState
 */
FurryThreadState furry_thread_get_state(FurryThread* thread);

/** Start FurryThread
 *
 * @param      thread  FurryThread instance
 */
void furry_thread_start(FurryThread* thread);

/** Join FurryThread
 *
 * @warning    Use this method only when CPU is not busy(Idle task receives
 *             control), otherwise it will wait forever.
 *
 * @param      thread  FurryThread instance
 *
 * @return     bool
 */
bool furry_thread_join(FurryThread* thread);

/** Get FreeRTOS FurryThreadId for FurryThread instance
 *
 * @param      thread  FurryThread instance
 *
 * @return     FurryThreadId or NULL
 */
FurryThreadId furry_thread_get_id(FurryThread* thread);

/** Enable heap tracing
 *
 * @param      thread  FurryThread instance
 */
void furry_thread_enable_heap_trace(FurryThread* thread);

/** Disable heap tracing
 *
 * @param      thread  FurryThread instance
 */
void furry_thread_disable_heap_trace(FurryThread* thread);

/** Get thread heap size
 *
 * @param      thread  FurryThread instance
 *
 * @return     size in bytes
 */
size_t furry_thread_get_heap_size(FurryThread* thread);

/** Get thread return code
 *
 * @param      thread  FurryThread instance
 *
 * @return     return code
 */
int32_t furry_thread_get_return_code(FurryThread* thread);

/** Thread related methods that doesn't involve FurryThread directly */

/** Get FreeRTOS FurryThreadId for current thread
 *
 * @param      thread  FurryThread instance
 *
 * @return     FurryThreadId or NULL
 */
FurryThreadId furry_thread_get_current_id();

/** Get FurryThread instance for current thread
 * 
 * @return FurryThread* 
 */
FurryThread* furry_thread_get_current();

/** Return control to scheduler */
void furry_thread_yield();

uint32_t furry_thread_flags_set(FurryThreadId thread_id, uint32_t flags);

uint32_t furry_thread_flags_clear(uint32_t flags);

uint32_t furry_thread_flags_get(void);

uint32_t furry_thread_flags_wait(uint32_t flags, uint32_t options, uint32_t timeout);

/**
 * @brief Enumerate threads
 * 
 * @param thread_array array of FurryThreadId, where thread ids will be stored
 * @param array_items array size
 * @return uint32_t threads count
 */
uint32_t furry_thread_enumerate(FurryThreadId* thread_array, uint32_t array_items);

/**
 * @brief Get thread name
 * 
 * @param thread_id 
 * @return const char* name or NULL
 */
const char* furry_thread_get_name(FurryThreadId thread_id);

/**
 * @brief Get thread appid
 * 
 * @param thread_id 
 * @return const char* appid
 */
const char* furry_thread_get_appid(FurryThreadId thread_id);

/**
 * @brief Get thread stack watermark
 * 
 * @param thread_id 
 * @return uint32_t 
 */
uint32_t furry_thread_get_stack_space(FurryThreadId thread_id);

/** Get STDOUT callback for thead
 *
 * @return STDOUT callback
 */
FurryThreadStdoutWriteCallback furry_thread_get_stdout_callback();

/** Set STDOUT callback for thread
 * 
 * @param      callback  callback or NULL to clear
 * 
 * @return     true on success, otherwise fail
 */
bool furry_thread_set_stdout_callback(FurryThreadStdoutWriteCallback callback);

/** Write data to buffered STDOUT
 * 
 * @param data input data
 * @param size input data size
 * 
 * @return size_t written data size
 */
size_t furry_thread_stdout_write(const char* data, size_t size);

/** Flush data to STDOUT
 * 
 * @return int32_t error code
 */
int32_t furry_thread_stdout_flush();

/** Suspend thread
 * 
 * @param thread_id thread id
 */
void furry_thread_suspend(FurryThreadId thread_id);

/** Resume thread
 * 
 * @param thread_id thread id
 */
void furry_thread_resume(FurryThreadId thread_id);

/** Get thread suspended state
 * 
 * @param thread_id thread id
 * @return true if thread is suspended
 */
bool furry_thread_is_suspended(FurryThreadId thread_id);

#ifdef __cplusplus
}
#endif
