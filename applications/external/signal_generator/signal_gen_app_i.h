#pragma once

#include "scenes/signal_gen_scene.h"

#include <furry_hal_clock.h>
#include <furry_hal_pwm.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include "views/signal_gen_pwm.h"

typedef struct SignalGenApp SignalGenApp;

struct SignalGenApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    VariableItemList* var_item_list;
    Submenu* submenu;
    SignalGenPwm* pwm_view;

    FurryHalClockMcoSourceId mco_src;
    FurryHalClockMcoDivisorId mco_div;

    FurryHalPwmOutputId pwm_ch_prev;
    FurryHalPwmOutputId pwm_ch;
    uint32_t pwm_freq;
    uint8_t pwm_duty;
};

typedef enum {
    SignalGenViewVarItemList,
    SignalGenViewSubmenu,
    SignalGenViewPwm,
} SignalGenAppView;

typedef enum {
    SignalGenMcoEventUpdate,
    SignalGenPwmEventUpdate,
    SignalGenPwmEventChannelChange,
} SignalGenCustomEvent;
