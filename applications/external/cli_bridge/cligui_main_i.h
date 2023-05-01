#pragma once

#include <furry.h>
#include <furry_hal.h>
#include <furry_hal_version.h>
#include <furry_hal_usb_cdc.h>
#include <furry_hal_usb.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <m-dict.h>
#include <loader/loader.h>
#include "internal_defs.h"

#define TEXT_BOX_STORE_SIZE (4096)
#define TEXT_INPUT_STORE_SIZE (512)

typedef enum {
    ViewTextInput,
    ViewConsoleOutput,
} CliguiState;

typedef struct {
    CliguiState state;
    struct {
        FurryStreamBuffer* app_tx;
        FurryStreamBuffer* app_rx;
    } streams;
} CliguiData;

typedef struct {
    CliguiData* data;
    Gui* gui;
    TextBox* text_box;
    FurryString* text_box_store;
    char text_input_store[TEXT_INPUT_STORE_SIZE + 1];
    TextInput* text_input;
    ViewDispatcher* view_dispatcher;
    ViewDispatcher_internal* view_dispatcher_i;
} CliguiApp;