#include <gui/view_stack.h>
#include <stdint.h>
#include <furry.h>
#include <furry_hal.h>
#include <portmacro.h>
#include <dolphin/dolphin.h>
#include <power/power_service/power.h>
#include <storage/storage.h>
#include <assets_icons.h>
#include <assets_dolphin_internal.h>

#include "views/bubble_animation_view.h"
#include "views/one_shot_animation_view.h"
#include "animation_storage.h"
#include "animation_manager.h"

#include <xtreme.h>

#define TAG "AnimationManager"

#define HARDCODED_ANIMATION_NAME "thank_you_128x64"
#define NO_SD_ANIMATION_NAME "L1_NoSd_128x49"
#define BAD_BATTERY_ANIMATION_NAME "L1_BadBattery_128x47"

#define NO_DB_ANIMATION_NAME "L0_NoDb_128x51"
#define BAD_SD_ANIMATION_NAME "L0_SdBad_128x51"
#define SD_OK_ANIMATION_NAME "L0_SdOk_128x51"
#define URL_ANIMATION_NAME "L0_Url_128x51"
#define NEW_MAIL_ANIMATION_NAME "L0_NewMail_128x51"

typedef enum {
    AnimationManagerStateIdle,
    AnimationManagerStateBlocked,
    AnimationManagerStateFreezedIdle,
    AnimationManagerStateFreezedBlocked,
} AnimationManagerState;

struct AnimationManager {
    bool sd_show_url;
    bool sd_shown_no_db;
    bool sd_shown_sd_ok;
    bool levelup_pending;
    bool levelup_active;
    AnimationManagerState state;
    FurryPubSubSubscription* pubsub_subscription_storage;
    FurryPubSubSubscription* pubsub_subscription_dolphin;
    BubbleAnimationView* animation_view;
    OneShotView* one_shot_view;
    FurryTimer* idle_animation_timer;
    StorageAnimation* current_animation;
    AnimationManagerInteractCallback interact_callback;
    AnimationManagerSetNewIdleAnimationCallback new_idle_callback;
    AnimationManagerSetNewIdleAnimationCallback check_blocking_callback;
    void* context;
    FurryString* freezed_animation_name;
    int32_t freezed_animation_time_left;
    ViewStack* view_stack;
};

static StorageAnimation*
    animation_manager_select_idle_animation(AnimationManager* animation_manager);
static void animation_manager_replace_current_animation(
    AnimationManager* animation_manager,
    StorageAnimation* storage_animation);
static void animation_manager_start_new_idle(AnimationManager* animation_manager);
static bool animation_manager_check_blocking(AnimationManager* animation_manager);
static bool animation_manager_is_valid_idle_animation(
    const StorageAnimationManifestInfo* info,
    const DolphinStats* stats,
    const bool unlock);
static void animation_manager_switch_to_one_shot_view(AnimationManager* animation_manager);
static void animation_manager_switch_to_animation_view(AnimationManager* animation_manager);

void animation_manager_set_context(AnimationManager* animation_manager, void* context) {
    furry_assert(animation_manager);
    animation_manager->context = context;
}

void animation_manager_set_new_idle_callback(
    AnimationManager* animation_manager,
    AnimationManagerSetNewIdleAnimationCallback callback) {
    furry_assert(animation_manager);
    animation_manager->new_idle_callback = callback;
}

void animation_manager_set_check_callback(
    AnimationManager* animation_manager,
    AnimationManagerCheckBlockingCallback callback) {
    furry_assert(animation_manager);
    animation_manager->check_blocking_callback = callback;
}

void animation_manager_set_interact_callback(
    AnimationManager* animation_manager,
    AnimationManagerInteractCallback callback) {
    furry_assert(animation_manager);
    animation_manager->interact_callback = callback;
}

static void animation_manager_check_blocking_callback(const void* message, void* context) {
    const StorageEvent* storage_event = message;

    switch(storage_event->type) {
    case StorageEventTypeCardMount:
    case StorageEventTypeCardUnmount:
    case StorageEventTypeCardMountError:
        furry_assert(context);
        AnimationManager* animation_manager = context;
        if(animation_manager->check_blocking_callback) {
            animation_manager->check_blocking_callback(animation_manager->context);
        }
        break;

    default:
        break;
    }
}

static void animation_manager_timer_callback(void* context) {
    furry_assert(context);
    AnimationManager* animation_manager = context;
    if(animation_manager->new_idle_callback) {
        animation_manager->new_idle_callback(animation_manager->context);
    }
}

static void animation_manager_interact_callback(void* context) {
    furry_assert(context);
    AnimationManager* animation_manager = context;
    if(animation_manager->interact_callback) {
        animation_manager->interact_callback(animation_manager->context);
    }
}

/* reaction to animation_manager->check_blocking_callback() */
void animation_manager_check_blocking_process(AnimationManager* animation_manager) {
    furry_assert(animation_manager);

    if(animation_manager->state == AnimationManagerStateIdle) {
        bool blocked = animation_manager_check_blocking(animation_manager);

        if(!blocked) {
            Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
            DolphinStats stats = dolphin_stats(dolphin);
            furry_record_close(RECORD_DOLPHIN);

            const StorageAnimationManifestInfo* manifest_info =
                animation_storage_get_meta(animation_manager->current_animation);
            bool valid = animation_manager_is_valid_idle_animation(
                manifest_info, &stats, XTREME_SETTINGS()->unlock_anims);

            if(!valid) {
                animation_manager_start_new_idle(animation_manager);
            }
        }
    }
}

/* reaction to animation_manager->new_idle_callback() */
void animation_manager_new_idle_process(AnimationManager* animation_manager) {
    furry_assert(animation_manager);

    if(animation_manager->state == AnimationManagerStateIdle) {
        animation_manager_start_new_idle(animation_manager);
    }
}

/* reaction to animation_manager->interact_callback() */
bool animation_manager_interact_process(AnimationManager* animation_manager) {
    furry_assert(animation_manager);
    bool consumed = true;

    if(animation_manager->levelup_pending) {
        animation_manager->levelup_pending = false;
        animation_manager->levelup_active = true;
        animation_manager_switch_to_one_shot_view(animation_manager);
        Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
        dolphin_upgrade_level(dolphin);
        furry_record_close(RECORD_DOLPHIN);
    } else if(animation_manager->levelup_active) {
        animation_manager->levelup_active = false;
        animation_manager_start_new_idle(animation_manager);
        animation_manager_switch_to_animation_view(animation_manager);
    } else if(animation_manager->state == AnimationManagerStateBlocked) {
        bool blocked = animation_manager_check_blocking(animation_manager);

        if(!blocked) {
            animation_manager_start_new_idle(animation_manager);
        }
    } else {
        consumed = false;
    }

    return consumed;
}

static void animation_manager_start_new_idle(AnimationManager* animation_manager) {
    furry_assert(animation_manager);

    StorageAnimation* new_animation = animation_manager_select_idle_animation(animation_manager);
    animation_manager_replace_current_animation(animation_manager, new_animation);
    const BubbleAnimation* bubble_animation =
        animation_storage_get_bubble_animation(animation_manager->current_animation);
    animation_manager->state = AnimationManagerStateIdle;
    XtremeSettings* xtreme_settings = XTREME_SETTINGS();
    int32_t duration = (xtreme_settings->cycle_anims == 0) ? (bubble_animation->duration) :
                                                             (xtreme_settings->cycle_anims);
    furry_timer_start(
        animation_manager->idle_animation_timer, (duration > 0) ? (duration * 1000) : 0);
}

static bool animation_manager_check_blocking(AnimationManager* animation_manager) {
    furry_assert(animation_manager);

    StorageAnimation* blocking_animation = NULL;
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FS_Error sd_status = storage_sd_status(storage);

    if(sd_status == FSE_INTERNAL) {
        blocking_animation = animation_storage_find_animation(BAD_SD_ANIMATION_NAME, false);
        furry_assert(blocking_animation);
    } else if(sd_status == FSE_NOT_READY) {
        animation_manager->sd_shown_sd_ok = false;
        animation_manager->sd_shown_no_db = false;
    } else if(sd_status == FSE_OK) {
        if(!animation_manager->sd_shown_sd_ok) {
            blocking_animation = animation_storage_find_animation(SD_OK_ANIMATION_NAME, false);
            furry_assert(blocking_animation);
            animation_manager->sd_shown_sd_ok = true;
        } else if(!animation_manager->sd_shown_no_db) {
            if(!storage_file_exists(storage, EXT_PATH("Manifest"))) {
                blocking_animation = animation_storage_find_animation(NO_DB_ANIMATION_NAME, false);
                furry_assert(blocking_animation);
                animation_manager->sd_shown_no_db = true;
                animation_manager->sd_show_url = true;
            }
        } else if(animation_manager->sd_show_url) {
            blocking_animation = animation_storage_find_animation(URL_ANIMATION_NAME, false);
            furry_assert(blocking_animation);
            animation_manager->sd_show_url = false;
        }
    }

    Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
    DolphinStats stats = dolphin_stats(dolphin);
    furry_record_close(RECORD_DOLPHIN);
    if(!blocking_animation && stats.level_up_is_pending) {
        blocking_animation = animation_storage_find_animation(NEW_MAIL_ANIMATION_NAME, false);
        furry_check(blocking_animation);
        animation_manager->levelup_pending = true;
    }

    if(blocking_animation) {
        furry_timer_stop(animation_manager->idle_animation_timer);
        animation_manager_replace_current_animation(animation_manager, blocking_animation);
        /* no timer starting because this is blocking animation */
        animation_manager->state = AnimationManagerStateBlocked;
    }

    furry_record_close(RECORD_STORAGE);

    return !!blocking_animation;
}

static void animation_manager_replace_current_animation(
    AnimationManager* animation_manager,
    StorageAnimation* storage_animation) {
    furry_assert(storage_animation);
    StorageAnimation* previous_animation = animation_manager->current_animation;

    const BubbleAnimation* animation = animation_storage_get_bubble_animation(storage_animation);
    bubble_animation_view_set_animation(animation_manager->animation_view, animation);
    const char* new_name = animation_storage_get_meta(storage_animation)->name;
    FURRY_LOG_I(TAG, "Select \'%s\' animation", new_name);
    animation_manager->current_animation = storage_animation;

    if(previous_animation) {
        animation_storage_free_storage_animation(&previous_animation);
    }
}

AnimationManager* animation_manager_alloc(void) {
    AnimationManager* animation_manager = malloc(sizeof(AnimationManager));
    animation_manager->animation_view = bubble_animation_view_alloc();
    animation_manager->view_stack = view_stack_alloc();
    View* animation_view = bubble_animation_get_view(animation_manager->animation_view);
    view_stack_add_view(animation_manager->view_stack, animation_view);
    animation_manager->freezed_animation_name = furry_string_alloc();

    animation_manager->idle_animation_timer =
        furry_timer_alloc(animation_manager_timer_callback, FurryTimerTypeOnce, animation_manager);
    bubble_animation_view_set_interact_callback(
        animation_manager->animation_view, animation_manager_interact_callback, animation_manager);

    Storage* storage = furry_record_open(RECORD_STORAGE);
    animation_manager->pubsub_subscription_storage = furry_pubsub_subscribe(
        storage_get_pubsub(storage), animation_manager_check_blocking_callback, animation_manager);
    furry_record_close(RECORD_STORAGE);

    Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
    animation_manager->pubsub_subscription_dolphin = furry_pubsub_subscribe(
        dolphin_get_pubsub(dolphin), animation_manager_check_blocking_callback, animation_manager);
    furry_record_close(RECORD_DOLPHIN);

    animation_manager->sd_shown_sd_ok = true;
    if(!animation_manager_check_blocking(animation_manager)) {
        animation_manager_start_new_idle(animation_manager);
    }

    return animation_manager;
}

void animation_manager_free(AnimationManager* animation_manager) {
    furry_assert(animation_manager);

    Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
    furry_pubsub_unsubscribe(
        dolphin_get_pubsub(dolphin), animation_manager->pubsub_subscription_dolphin);
    furry_record_close(RECORD_DOLPHIN);

    Storage* storage = furry_record_open(RECORD_STORAGE);
    furry_pubsub_unsubscribe(
        storage_get_pubsub(storage), animation_manager->pubsub_subscription_storage);
    furry_record_close(RECORD_STORAGE);

    furry_string_free(animation_manager->freezed_animation_name);
    View* animation_view = bubble_animation_get_view(animation_manager->animation_view);
    view_stack_remove_view(animation_manager->view_stack, animation_view);
    bubble_animation_view_free(animation_manager->animation_view);
    furry_timer_free(animation_manager->idle_animation_timer);
}

View* animation_manager_get_animation_view(AnimationManager* animation_manager) {
    furry_assert(animation_manager);

    return view_stack_get_view(animation_manager->view_stack);
}

static bool animation_manager_is_valid_idle_animation(
    const StorageAnimationManifestInfo* info,
    const DolphinStats* stats,
    const bool unlock) {
    furry_assert(info);
    furry_assert(info->name);

    bool result = true;

    if(!strcmp(info->name, BAD_BATTERY_ANIMATION_NAME)) {
        Power* power = furry_record_open(RECORD_POWER);
        bool battery_is_well = power_is_battery_healthy(power);
        furry_record_close(RECORD_POWER);

        result = !battery_is_well;
    }
    if(!strcmp(info->name, NO_SD_ANIMATION_NAME)) {
        Storage* storage = furry_record_open(RECORD_STORAGE);
        FS_Error sd_status = storage_sd_status(storage);
        furry_record_close(RECORD_STORAGE);

        result = (sd_status == FSE_NOT_READY);
    }
    if(!unlock) {
        if((stats->butthurt < info->min_butthurt) || (stats->butthurt > info->max_butthurt)) {
            result = false;
        }
        if((stats->level < info->min_level) || (stats->level > info->max_level)) {
            result = false;
        }
    }

    return result;
}

static StorageAnimation*
    animation_manager_select_idle_animation(AnimationManager* animation_manager) {
    const char* avoid_animation = NULL;
    if(animation_manager->current_animation) {
        avoid_animation = animation_storage_get_meta(animation_manager->current_animation)->name;
    }
    UNUSED(animation_manager);

    StorageAnimationList_t animation_list;
    StorageAnimationList_init(animation_list);
    animation_storage_fill_animation_list(&animation_list);

    Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
    DolphinStats stats = dolphin_stats(dolphin);
    furry_record_close(RECORD_DOLPHIN);
    uint32_t whole_weight = 0;

    bool fallback = XTREME_SETTINGS()->fallback_anim;
    if(StorageAnimationList_size(animation_list) == dolphin_internal_size + 1 && !fallback) {
        // One ext anim and fallback disabled, dont skip current anim (current = only ext one)
        avoid_animation = NULL;
    }

    StorageAnimationList_it_t it;
    bool unlock = XTREME_SETTINGS()->unlock_anims;
    for(StorageAnimationList_it(it, animation_list); !StorageAnimationList_end_p(it);) {
        StorageAnimation* storage_animation = *StorageAnimationList_ref(it);
        const StorageAnimationManifestInfo* manifest_info =
            animation_storage_get_meta(storage_animation);
        bool valid = animation_manager_is_valid_idle_animation(manifest_info, &stats, unlock);

        if(avoid_animation != NULL && strcmp(manifest_info->name, avoid_animation) == 0) {
            // Avoid repeating same animation twice
            valid = false;
        }
        if(strcmp(manifest_info->name, HARDCODED_ANIMATION_NAME) == 0 && !fallback) {
            // Skip fallback animation
            valid = false;
        }

        if(valid) {
            whole_weight += manifest_info->weight;
            StorageAnimationList_next(it);
        } else {
            animation_storage_free_storage_animation(&storage_animation);
            /* remove and increase iterator */
            StorageAnimationList_remove(animation_list, it);
        }
    }

    uint32_t lucky_number = furry_hal_random_get() % whole_weight;
    uint32_t weight = 0;

    StorageAnimation* selected = NULL;
    for
        M_EACH(item, animation_list, StorageAnimationList_t) {
            if(lucky_number < weight) {
                break;
            }
            weight += animation_storage_get_meta(*item)->weight;
            selected = *item;
        }

    for
        M_EACH(item, animation_list, StorageAnimationList_t) {
            if(*item != selected) {
                animation_storage_free_storage_animation(item);
            }
        }

    StorageAnimationList_clear(animation_list);

    /* cache animation, if failed - choose reliable animation */
    if(selected == NULL) {
        FURRY_LOG_E(TAG, "Can't find valid animation in manifest");
        selected = animation_storage_find_animation(HARDCODED_ANIMATION_NAME, false);
    } else if(!animation_storage_get_bubble_animation(selected)) {
        const char* name = animation_storage_get_meta(selected)->name;
        FURRY_LOG_E(TAG, "Can't upload animation described in manifest: \'%s\'", name);
        animation_storage_free_storage_animation(&selected);
        selected = animation_storage_find_animation(HARDCODED_ANIMATION_NAME, false);
    } else {
        FurryHalRtcDateTime date;
        furry_hal_rtc_get_datetime(&date);
        if(date.month == 4 && date.day == 1 && furry_hal_random_get() % 2) {
            animation_storage_free_storage_animation(&selected);
            selected = animation_storage_find_animation("L3_Sunflower_128x64", true);
            if(selected == NULL) {
                selected = animation_storage_find_animation(HARDCODED_ANIMATION_NAME, false);
            }
        }
    }

    furry_assert(selected);
    return selected;
}

bool animation_manager_is_animation_loaded(AnimationManager* animation_manager) {
    furry_assert(animation_manager);
    return animation_manager->current_animation;
}

void animation_manager_unload_and_stall_animation(AnimationManager* animation_manager) {
    furry_assert(animation_manager);
    furry_assert(animation_manager->current_animation);
    furry_assert(!furry_string_size(animation_manager->freezed_animation_name));
    furry_assert(
        (animation_manager->state == AnimationManagerStateIdle) ||
        (animation_manager->state == AnimationManagerStateBlocked));

    if(animation_manager->state == AnimationManagerStateBlocked) {
        animation_manager->state = AnimationManagerStateFreezedBlocked;
    } else if(animation_manager->state == AnimationManagerStateIdle) { //-V547
        animation_manager->state = AnimationManagerStateFreezedIdle;

        animation_manager->freezed_animation_time_left =
            xTimerGetExpiryTime(animation_manager->idle_animation_timer) - xTaskGetTickCount();
        if(animation_manager->freezed_animation_time_left < 0) {
            animation_manager->freezed_animation_time_left = 0;
        }
        furry_timer_stop(animation_manager->idle_animation_timer);
    } else {
        furry_assert(0);
    }

    FURRY_LOG_I(
        TAG,
        "Unload animation \'%s\'",
        animation_storage_get_meta(animation_manager->current_animation)->name);

    StorageAnimationManifestInfo* meta =
        animation_storage_get_meta(animation_manager->current_animation);
    /* copy str, not move, because it can be internal animation */
    furry_string_set(animation_manager->freezed_animation_name, meta->name);

    bubble_animation_freeze(animation_manager->animation_view);
    animation_storage_free_storage_animation(&animation_manager->current_animation);
}

void animation_manager_load_and_continue_animation(AnimationManager* animation_manager) {
    furry_assert(animation_manager);
    furry_assert(!animation_manager->current_animation);
    furry_assert(furry_string_size(animation_manager->freezed_animation_name));
    furry_assert(
        (animation_manager->state == AnimationManagerStateFreezedIdle) ||
        (animation_manager->state == AnimationManagerStateFreezedBlocked));

    if(animation_manager->state == AnimationManagerStateFreezedBlocked) {
        StorageAnimation* restore_animation = animation_storage_find_animation(
            furry_string_get_cstr(animation_manager->freezed_animation_name), false);
        /* all blocked animations must be in flipper -> we can
         * always find blocking animation */
        furry_assert(restore_animation);
        animation_manager_replace_current_animation(animation_manager, restore_animation);
        animation_manager->state = AnimationManagerStateBlocked;
    } else if(animation_manager->state == AnimationManagerStateFreezedIdle) { //-V547
        /* check if we missed some system notifications, and set current_animation */
        bool blocked = animation_manager_check_blocking(animation_manager);
        if(!blocked) {
            /* if no blocking - try restore last one idle */
            StorageAnimation* restore_animation = animation_storage_find_animation(
                furry_string_get_cstr(animation_manager->freezed_animation_name), false);
            if(restore_animation) {
                Dolphin* dolphin = furry_record_open(RECORD_DOLPHIN);
                DolphinStats stats = dolphin_stats(dolphin);
                furry_record_close(RECORD_DOLPHIN);
                const StorageAnimationManifestInfo* manifest_info =
                    animation_storage_get_meta(restore_animation);
                bool valid = animation_manager_is_valid_idle_animation(
                    manifest_info, &stats, XTREME_SETTINGS()->unlock_anims);
                if(valid) {
                    animation_manager_replace_current_animation(
                        animation_manager, restore_animation);
                    animation_manager->state = AnimationManagerStateIdle;

                    if(animation_manager->freezed_animation_time_left) {
                        furry_timer_start(
                            animation_manager->idle_animation_timer,
                            animation_manager->freezed_animation_time_left);
                    } else {
                        const BubbleAnimation* bubble_animation =
                            animation_storage_get_bubble_animation(
                                animation_manager->current_animation);
                        XtremeSettings* xtreme_settings = XTREME_SETTINGS();
                        int32_t duration = (xtreme_settings->cycle_anims == 0) ?
                                               (bubble_animation->duration) :
                                               (xtreme_settings->cycle_anims);
                        furry_timer_start(
                            animation_manager->idle_animation_timer,
                            (duration > 0) ? (duration * 1000) : 0);
                    }
                }
            } else {
                FURRY_LOG_E(
                    TAG,
                    "Failed to restore \'%s\'",
                    furry_string_get_cstr(animation_manager->freezed_animation_name));
            }
        }
    } else {
        /* Unknown state is an error. But not in release version.*/
        furry_assert(0);
    }

    /* if can't restore previous animation - select new */
    if(!animation_manager->current_animation) {
        animation_manager_start_new_idle(animation_manager);
    }
    FURRY_LOG_I(
        TAG,
        "Load animation \'%s\'",
        animation_storage_get_meta(animation_manager->current_animation)->name);

    bubble_animation_unfreeze(animation_manager->animation_view);
    furry_string_reset(animation_manager->freezed_animation_name);
    furry_assert(animation_manager->current_animation);
}

static void animation_manager_switch_to_one_shot_view(AnimationManager* animation_manager) {
    furry_assert(animation_manager);
    furry_assert(!animation_manager->one_shot_view);

    animation_manager->one_shot_view = one_shot_view_alloc();
    one_shot_view_set_interact_callback(
        animation_manager->one_shot_view, animation_manager_interact_callback, animation_manager);
    View* prev_view = bubble_animation_get_view(animation_manager->animation_view);
    View* next_view = one_shot_view_get_view(animation_manager->one_shot_view);
    view_stack_remove_view(animation_manager->view_stack, prev_view);
    view_stack_add_view(animation_manager->view_stack, next_view);
    one_shot_view_start_animation(
        animation_manager->one_shot_view, XTREME_ASSETS()->A_Levelup_128x64);
}

static void animation_manager_switch_to_animation_view(AnimationManager* animation_manager) {
    furry_assert(animation_manager);
    furry_assert(animation_manager->one_shot_view);

    View* prev_view = one_shot_view_get_view(animation_manager->one_shot_view);
    View* next_view = bubble_animation_get_view(animation_manager->animation_view);
    view_stack_remove_view(animation_manager->view_stack, prev_view);
    view_stack_add_view(animation_manager->view_stack, next_view);
    one_shot_view_free(animation_manager->one_shot_view);
    animation_manager->one_shot_view = NULL;
}
