#pragma once
#include <furry.h>
#include <furry_hal.h>
#include <m-dict.h>
#include <m-bptree.h>
#include <m-array.h>
#include <cli/cli.h>

typedef struct {
    FurryThreadStdoutWriteCallback write_callback;
    FurryString* buffer;
} FurryThreadStdout_internal;

typedef struct {
    bool is_service;
    FurryThreadState state;
    int32_t ret;

    FurryThreadCallback callback;
    void* context;

    FurryThreadStateCallback state_callback;
    void* state_context;

    char* name;
    configSTACK_DEPTH_TYPE stack_size;
    FurryThreadPriority priority;

    TaskHandle_t task_handle;
    bool heap_trace_enabled;
    size_t heap_size;

    FurryThreadStdout_internal output;
} FurryThread_internal;

DICT_DEF2(ViewDict, uint32_t, M_DEFAULT_OPLIST, View*, M_PTR_OPLIST)
typedef struct {
    FurryMessageQueue* queue;
    Gui* gui;
    ViewPort* view_port;
    ViewDict_t views;

    View* current_view;

    View* ongoing_input_view;
    uint8_t ongoing_input;

    ViewDispatcherCustomEventCallback custom_event_callback;
    ViewDispatcherNavigationEventCallback navigation_event_callback;
    ViewDispatcherTickEventCallback tick_event_callback;
    uint32_t tick_period;
    void* event_context;
} ViewDispatcher_internal;

typedef struct {
    Gui* gui;
    bool is_enabled;
    ViewPortOrientation orientation;

    uint8_t width;
    uint8_t height;

    ViewPortDrawCallback draw_callback;
    void* draw_callback_context;

    ViewPortInputCallback input_callback;
    void* input_callback_context;
} ViewPort_internal;

typedef struct {
    FurryThreadId loader_thread;

    const void* application;
    FurryThread* application_thread;
    char* application_arguments;

    void* cli;
    void* gui;

    void* view_dispatcher;
    void* primary_menu;
    void* plugins_menu;
    void* debug_menu;
    void* settings_menu;

    volatile uint8_t lock_count;

    void* pubsub;
} Loader_internal;

typedef struct {
    CliCallback callback;
    void* context;
    uint32_t flags;
} CliCommand_internal;

#define CLI_COMMANDS_TREE_RANK 4
BPTREE_DEF2(
    CliCommandTree_internal,
    CLI_COMMANDS_TREE_RANK,
    FurryString*,
    FURRY_STRING_OPLIST,
    CliCommand_internal,
    M_POD_OPLIST)

#define M_OPL_CliCommandTree_internal_t() BPTREE_OPLIST(CliCommandTree_internal, M_POD_OPLIST)

typedef struct {
    CliCommandTree_internal_t commands;
    void* mutex;
    void* idle_sem;
    void* last_line;
    void* line;
    void* session;

    size_t cursor_position;
} Cli_internal;