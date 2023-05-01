
#include <stdint.h>
#include <flipper_format/flipper_format.h>
#include <furry.h>
#include <core/dangerous_defines.h>
#include <storage/storage.h>
#include <gui/icon_i.h>

#include "animation_manager.h"
#include "animation_storage.h"
#include "animation_storage_i.h"
#include <assets_dolphin_internal.h>
#include <assets_dolphin_blocking.h>
#include <xtreme.h>
#define ANIMATION_META_FILE "meta.txt"
#define BASE_ANIMATION_DIR EXT_PATH("dolphin")
#define TAG "AnimationStorage"
/* Unused old code, for safe-keeping

#define ANIMATION_MANIFEST_FILE ANIMATION_DIR "/manifest.txt"

*/
// 59 Max length = strlen("/ext/dolphin_custom//Anims") + XTREME_ASSETS_PACK_NAME_LEN + 1 (Null terminator)
char ANIMATION_DIR[59];
// 72 Max length = ANIMATION_DIR + strlen("/manifest.txt")
char ANIMATION_MANIFEST_FILE[72];

static void animation_storage_free_bubbles(BubbleAnimation* animation);
static void animation_storage_free_frames(BubbleAnimation* animation);
static void animation_storage_free_animation(BubbleAnimation** storage_animation);
static BubbleAnimation* animation_storage_load_animation(const char* name);

void animation_handler_select_manifest(bool force_stock) {
    XtremeSettings* xtreme_settings = XTREME_SETTINGS();
    FurryString* anim_dir = furry_string_alloc();
    FurryString* manifest = furry_string_alloc();
    bool use_asset_pack = !force_stock && xtreme_settings->asset_pack[0] != '\0';
    if(use_asset_pack) {
        furry_string_printf(
            anim_dir, "%s/%s/Anims", XTREME_ASSETS_PATH, xtreme_settings->asset_pack);
        furry_string_printf(manifest, "%s/manifest.txt", furry_string_get_cstr(anim_dir));
        Storage* storage = furry_record_open(RECORD_STORAGE);
        if(storage_common_stat(storage, furry_string_get_cstr(manifest), NULL) == FSE_OK) {
            FURRY_LOG_I(TAG, "Custom manifest selected");
        } else {
            use_asset_pack = false;
        }
        furry_record_close(RECORD_STORAGE);
    }
    if(!use_asset_pack) {
        furry_string_set(anim_dir, BASE_ANIMATION_DIR);
        furry_string_printf(manifest, "%s/manifest.txt", furry_string_get_cstr(anim_dir));
        FURRY_LOG_I(TAG, "Base manifest selected");
    }
    strlcpy(ANIMATION_DIR, furry_string_get_cstr(anim_dir), sizeof(ANIMATION_DIR));
    strlcpy(
        ANIMATION_MANIFEST_FILE, furry_string_get_cstr(manifest), sizeof(ANIMATION_MANIFEST_FILE));
    furry_string_free(manifest);
    furry_string_free(anim_dir);
}

static bool animation_storage_load_single_manifest_info(
    StorageAnimationManifestInfo* manifest_info,
    const char* name,
    bool force_stock) {
    furry_assert(manifest_info);
    animation_handler_select_manifest(force_stock);
    bool result = false;
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    flipper_format_set_strict_mode(file, true);
    FurryString* read_string;
    read_string = furry_string_alloc();

    do {
        uint32_t u32value;
        if(FSE_OK != storage_sd_status(storage)) break;
        if(!flipper_format_file_open_existing(file, ANIMATION_MANIFEST_FILE)) break;

        if(!flipper_format_read_header(file, read_string, &u32value)) break;
        if(furry_string_cmp_str(read_string, "Flipper Animation Manifest")) break;

        manifest_info->name = NULL;

        /* skip other animation names */
        flipper_format_set_strict_mode(file, false);
        while(flipper_format_read_string(file, "Name", read_string) &&
              furry_string_cmp_str(read_string, name))
            ;
        if(furry_string_cmp_str(read_string, name)) break;
        flipper_format_set_strict_mode(file, true);

        manifest_info->name = malloc(furry_string_size(read_string) + 1);
        strcpy((char*)manifest_info->name, furry_string_get_cstr(read_string));

        if(!flipper_format_read_uint32(file, "Min butthurt", &u32value, 1)) break;
        manifest_info->min_butthurt = u32value;
        if(!flipper_format_read_uint32(file, "Max butthurt", &u32value, 1)) break;
        manifest_info->max_butthurt = u32value;
        if(!flipper_format_read_uint32(file, "Min level", &u32value, 1)) break;
        manifest_info->min_level = u32value;
        if(!flipper_format_read_uint32(file, "Max level", &u32value, 1)) break;
        manifest_info->max_level = u32value;
        if(!flipper_format_read_uint32(file, "Weight", &u32value, 1)) break;
        manifest_info->weight = u32value;
        result = true;
    } while(0);

    if(!result && manifest_info->name) {
        free((void*)manifest_info->name);
    }
    furry_string_free(read_string);
    flipper_format_free(file);

    furry_record_close(RECORD_STORAGE);

    return result;
}

void animation_storage_fill_animation_list(StorageAnimationList_t* animation_list) {
    furry_assert(sizeof(StorageAnimationList_t) == sizeof(void*));
    furry_assert(!StorageAnimationList_size(*animation_list));
    animation_handler_select_manifest(false);

    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    /* Forbid skipping fields */
    flipper_format_set_strict_mode(file, true);
    FurryString* read_string;
    read_string = furry_string_alloc();

    do {
        uint32_t u32value;
        StorageAnimation* storage_animation = NULL;

        if(FSE_OK != storage_sd_status(storage)) break;
        if(!flipper_format_file_open_existing(file, ANIMATION_MANIFEST_FILE)) break;
        if(!flipper_format_read_header(file, read_string, &u32value)) break;
        if(furry_string_cmp_str(read_string, "Flipper Animation Manifest")) break;
        do {
            storage_animation = malloc(sizeof(StorageAnimation));
            storage_animation->external = true;
            storage_animation->animation = NULL;
            storage_animation->manifest_info.name = NULL;

            if(!flipper_format_read_string(file, "Name", read_string)) break;
            storage_animation->manifest_info.name = malloc(furry_string_size(read_string) + 1);
            strcpy(
                (char*)storage_animation->manifest_info.name, furry_string_get_cstr(read_string));

            if(!flipper_format_read_uint32(file, "Min butthurt", &u32value, 1)) break;
            storage_animation->manifest_info.min_butthurt = u32value;
            if(!flipper_format_read_uint32(file, "Max butthurt", &u32value, 1)) break;
            storage_animation->manifest_info.max_butthurt = u32value;
            if(!flipper_format_read_uint32(file, "Min level", &u32value, 1)) break;
            storage_animation->manifest_info.min_level = u32value;
            if(!flipper_format_read_uint32(file, "Max level", &u32value, 1)) break;
            storage_animation->manifest_info.max_level = u32value;
            if(!flipper_format_read_uint32(file, "Weight", &u32value, 1)) break;
            storage_animation->manifest_info.weight = u32value;

            StorageAnimationList_push_back(*animation_list, storage_animation);
        } while(1);

        animation_storage_free_storage_animation(&storage_animation);
    } while(0);

    furry_string_free(read_string);
    flipper_format_free(file);

    // add hard-coded animations
    for(size_t i = 0; i < dolphin_internal_size; ++i) {
        StorageAnimationList_push_back(*animation_list, (StorageAnimation*)&dolphin_internal[i]);
    }

    furry_record_close(RECORD_STORAGE);
}

StorageAnimation* animation_storage_find_animation(const char* name, bool force_stock) {
    furry_assert(name);
    furry_assert(strlen(name));
    StorageAnimation* storage_animation = NULL;

    for(size_t i = 0; i < dolphin_blocking_size; ++i) {
        if(!strcmp(dolphin_blocking[i].manifest_info.name, name)) {
            storage_animation = (StorageAnimation*)&dolphin_blocking[i];
            break;
        }
    }

    if(!storage_animation) {
        for(size_t i = 0; i < dolphin_internal_size; ++i) {
            if(!strcmp(dolphin_internal[i].manifest_info.name, name)) {
                storage_animation = (StorageAnimation*)&dolphin_internal[i];
                break;
            }
        }
    }

    /* look through external animations */
    if(!storage_animation) {
        storage_animation = malloc(sizeof(StorageAnimation));
        storage_animation->external = true;

        bool result = false;
        result = animation_storage_load_single_manifest_info(
            &storage_animation->manifest_info, name, force_stock);
        if(result) {
            storage_animation->animation = animation_storage_load_animation(name);
            result = !!storage_animation->animation;
        }
        if(!result) {
            animation_storage_free_storage_animation(&storage_animation);
        }
    }

    return storage_animation;
}

StorageAnimationManifestInfo* animation_storage_get_meta(StorageAnimation* storage_animation) {
    furry_assert(storage_animation);
    return &storage_animation->manifest_info;
}

const BubbleAnimation*
    animation_storage_get_bubble_animation(StorageAnimation* storage_animation) {
    furry_assert(storage_animation);
    animation_storage_cache_animation(storage_animation);
    return storage_animation->animation;
}

void animation_storage_cache_animation(StorageAnimation* storage_animation) {
    furry_assert(storage_animation);

    if(storage_animation->external) {
        if(!storage_animation->animation) {
            storage_animation->animation =
                animation_storage_load_animation(storage_animation->manifest_info.name);
        }
    }
}

static void animation_storage_free_animation(BubbleAnimation** animation) {
    furry_assert(animation);

    if(*animation) {
        animation_storage_free_bubbles(*animation);
        animation_storage_free_frames(*animation);
        if((*animation)->frame_order) {
            free((void*)(*animation)->frame_order);
        }
        free(*animation);
        *animation = NULL;
    }
}

void animation_storage_free_storage_animation(StorageAnimation** storage_animation) {
    furry_assert(storage_animation);
    furry_assert(*storage_animation);

    if((*storage_animation)->external) {
        animation_storage_free_animation((BubbleAnimation**)&(*storage_animation)->animation);

        if((*storage_animation)->manifest_info.name) {
            free((void*)(*storage_animation)->manifest_info.name);
        }
        free(*storage_animation);
    }

    *storage_animation = NULL;
}

static bool animation_storage_cast_align(FurryString* align_str, Align* align) {
    if(!furry_string_cmp(align_str, "Bottom")) {
        *align = AlignBottom;
    } else if(!furry_string_cmp(align_str, "Top")) {
        *align = AlignTop;
    } else if(!furry_string_cmp(align_str, "Left")) {
        *align = AlignLeft;
    } else if(!furry_string_cmp(align_str, "Right")) {
        *align = AlignRight;
    } else if(!furry_string_cmp(align_str, "Center")) {
        *align = AlignCenter;
    } else {
        return false;
    }

    return true;
}

static void animation_storage_free_frames(BubbleAnimation* animation) {
    furry_assert(animation);

    const Icon* icon = &animation->icon_animation;
    for(int i = 0; i < icon->frame_count; ++i) {
        if(icon->frames[i]) {
            free((void*)icon->frames[i]);
        }
    }

    free((void*)icon->frames);
}

static bool animation_storage_load_frames(
    Storage* storage,
    const char* name,
    BubbleAnimation* animation,
    uint32_t* frame_order,
    uint8_t width,
    uint8_t height) {
    uint16_t frame_order_count = animation->passive_frames + animation->active_frames;

    /* The frames should go in order (0...N), without omissions */
    size_t max_frame_count = 0;
    for(int i = 0; i < frame_order_count; ++i) {
        max_frame_count = MAX(max_frame_count, frame_order[i]);
    }

    if((max_frame_count >= frame_order_count) || (max_frame_count >= 256 /* max uint8_t */)) {
        return false;
    }

    Icon* icon = (Icon*)&animation->icon_animation;
    FURRY_CONST_ASSIGN(icon->frame_count, max_frame_count + 1);
    FURRY_CONST_ASSIGN(icon->frame_rate, 0);
    FURRY_CONST_ASSIGN(icon->height, height);
    FURRY_CONST_ASSIGN(icon->width, width);
    icon->frames = malloc(sizeof(const uint8_t*) * icon->frame_count);

    bool frames_ok = false;
    File* file = storage_file_alloc(storage);
    FileInfo file_info;
    FurryString* filename;
    filename = furry_string_alloc();
    size_t max_filesize = ROUND_UP_TO(width, 8) * height + 2;

    for(int i = 0; i < icon->frame_count; ++i) {
        frames_ok = false;
        furry_string_printf(filename, "%s/%s/frame_%d.bm", ANIMATION_DIR, name, i);

        if(storage_common_stat(storage, furry_string_get_cstr(filename), &file_info) != FSE_OK)
            break;
        if(file_info.size > max_filesize) {
            FURRY_LOG_E(
                TAG,
                "Filesize %lld, max: %d (width %d, height %d)",
                file_info.size,
                max_filesize,
                width,
                height);
            break;
        }
        if(!storage_file_open(
               file, furry_string_get_cstr(filename), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURRY_LOG_E(TAG, "Can't open file \'%s\'", furry_string_get_cstr(filename));
            break;
        }

        FURRY_CONST_ASSIGN_PTR(icon->frames[i], malloc(file_info.size));
        if(storage_file_read(file, (void*)icon->frames[i], file_info.size) != file_info.size) {
            FURRY_LOG_E(TAG, "Read failed: \'%s\'", furry_string_get_cstr(filename));
            break;
        }
        storage_file_close(file);
        frames_ok = true;
    }

    if(!frames_ok) {
        FURRY_LOG_E(
            TAG,
            "Load \'%s\' failed, %dx%d, size: %lld",
            furry_string_get_cstr(filename),
            width,
            height,
            file_info.size);
        animation_storage_free_frames(animation);
    } else {
        furry_check(animation->icon_animation.frames);
        for(int i = 0; i < animation->icon_animation.frame_count; ++i) {
            furry_check(animation->icon_animation.frames[i]);
        }
    }

    storage_file_free(file);
    furry_string_free(filename);

    return frames_ok;
}

static bool animation_storage_load_bubbles(BubbleAnimation* animation, FlipperFormat* ff) {
    uint32_t u32value;
    FurryString* str;
    str = furry_string_alloc();
    bool success = false;
    furry_assert(!animation->frame_bubble_sequences);

    do {
        if(!flipper_format_read_uint32(ff, "Bubble slots", &u32value, 1)) break;
        if(u32value > 20) break;
        animation->frame_bubble_sequences_count = u32value;
        if(animation->frame_bubble_sequences_count == 0) {
            success = true;
            break;
        }
        animation->frame_bubble_sequences =
            malloc(sizeof(FrameBubble*) * animation->frame_bubble_sequences_count);

        int32_t current_slot = 0;
        for(int i = 0; i < animation->frame_bubble_sequences_count; ++i) {
            FURRY_CONST_ASSIGN_PTR(
                animation->frame_bubble_sequences[i], malloc(sizeof(FrameBubble)));
        }

        const FrameBubble* bubble = animation->frame_bubble_sequences[0];
        int8_t index = -1;
        for(;;) {
            if(!flipper_format_read_int32(ff, "Slot", &current_slot, 1)) break;
            if((current_slot != 0) && (index == -1)) break;

            if(current_slot == index) {
                FURRY_CONST_ASSIGN_PTR(bubble->next_bubble, malloc(sizeof(FrameBubble)));
                bubble = bubble->next_bubble;
            } else if(current_slot == index + 1) {
                ++index;
                bubble = animation->frame_bubble_sequences[index];
            } else {
                /* slots have to start from 0, be ascending sorted, and
                 * have exact number of slots as specified in "Bubble slots" */
                break;
            }
            if(index >= animation->frame_bubble_sequences_count) break;

            if(!flipper_format_read_uint32(ff, "X", &u32value, 1)) break;
            FURRY_CONST_ASSIGN(bubble->bubble.x, u32value);
            if(!flipper_format_read_uint32(ff, "Y", &u32value, 1)) break;
            FURRY_CONST_ASSIGN(bubble->bubble.y, u32value);

            if(!flipper_format_read_string(ff, "Text", str)) break;
            if(furry_string_size(str) > 100) break;

            furry_string_replace_all(str, "\\n", "\n");

            FURRY_CONST_ASSIGN_PTR(bubble->bubble.text, malloc(furry_string_size(str) + 1));
            strcpy((char*)bubble->bubble.text, furry_string_get_cstr(str));

            if(!flipper_format_read_string(ff, "AlignH", str)) break;
            if(!animation_storage_cast_align(str, (Align*)&bubble->bubble.align_h)) break;
            if(!flipper_format_read_string(ff, "AlignV", str)) break;
            if(!animation_storage_cast_align(str, (Align*)&bubble->bubble.align_v)) break;

            if(!flipper_format_read_uint32(ff, "StartFrame", &u32value, 1)) break;
            FURRY_CONST_ASSIGN(bubble->start_frame, u32value);
            if(!flipper_format_read_uint32(ff, "EndFrame", &u32value, 1)) break;
            FURRY_CONST_ASSIGN(bubble->end_frame, u32value);
        }
        success = (index + 1) == animation->frame_bubble_sequences_count;
    } while(0);

    if(!success) {
        if(animation->frame_bubble_sequences) {
            FURRY_LOG_E(TAG, "Failed to load animation bubbles");
            animation_storage_free_bubbles(animation);
        }
    }

    furry_string_free(str);
    return success;
}

static BubbleAnimation* animation_storage_load_animation(const char* name) {
    furry_assert(name);
    BubbleAnimation* animation = malloc(sizeof(BubbleAnimation));

    uint32_t height = 0;
    uint32_t width = 0;
    uint32_t* u32array = NULL;
    Storage* storage = furry_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    /* Forbid skipping fields */
    flipper_format_set_strict_mode(ff, true);
    FurryString* str;
    str = furry_string_alloc();
    animation->frame_bubble_sequences = NULL;

    bool success = false;
    do {
        uint32_t u32value;

        if(FSE_OK != storage_sd_status(storage)) break;

        furry_string_printf(str, "%s/%s/" ANIMATION_META_FILE, ANIMATION_DIR, name);
        if(!flipper_format_file_open_existing(ff, furry_string_get_cstr(str))) break;
        if(!flipper_format_read_header(ff, str, &u32value)) break;
        if(furry_string_cmp_str(str, "Flipper Animation")) break;

        if(!flipper_format_read_uint32(ff, "Width", &width, 1)) break;
        if(!flipper_format_read_uint32(ff, "Height", &height, 1)) break;

        if(!flipper_format_read_uint32(ff, "Passive frames", &u32value, 1)) break;
        animation->passive_frames = u32value;
        if(!flipper_format_read_uint32(ff, "Active frames", &u32value, 1)) break;
        animation->active_frames = u32value;

        uint8_t frames = animation->passive_frames + animation->active_frames;
        uint32_t count = 0;
        if(!flipper_format_get_value_count(ff, "Frames order", &count)) break;
        if(count != frames) {
            FURRY_LOG_E(TAG, "Error loading animation: frames order");
            break;
        }
        u32array = malloc(sizeof(uint32_t) * frames);
        if(!flipper_format_read_uint32(ff, "Frames order", u32array, frames)) break;
        animation->frame_order = malloc(sizeof(uint8_t) * frames);
        for(int i = 0; i < frames; ++i) {
            FURRY_CONST_ASSIGN(animation->frame_order[i], u32array[i]);
        }

        /* passive and active frames must be loaded up to this point */
        if(!animation_storage_load_frames(storage, name, animation, u32array, width, height))
            break;

        if(!flipper_format_read_uint32(ff, "Active cycles", &u32value, 1)) break; //-V779
        animation->active_cycles = u32value;
        if(!flipper_format_read_uint32(ff, "Frame rate", &u32value, 1)) break;
        uint16_t anim_speed = XTREME_SETTINGS()->anim_speed;
        u32value = (u32value * anim_speed) / 100;
        FURRY_CONST_ASSIGN(animation->icon_animation.frame_rate, u32value < 1 ? 1 : u32value);
        if(!flipper_format_read_uint32(ff, "Duration", &u32value, 1)) break;
        animation->duration = u32value;
        if(!flipper_format_read_uint32(ff, "Active cooldown", &u32value, 1)) break;
        animation->active_cooldown = u32value;

        if(!animation_storage_load_bubbles(animation, ff)) break;
        success = true;
    } while(0);

    furry_string_free(str);
    flipper_format_free(ff);
    if(u32array) {
        free(u32array);
    }

    if(!success) { //-V547
        if(animation->frame_order) {
            free((void*)animation->frame_order);
        }
        free(animation);
        animation = NULL;
    }

    return animation;
}

static void animation_storage_free_bubbles(BubbleAnimation* animation) {
    if(!animation->frame_bubble_sequences) return;

    for(int i = 0; i < animation->frame_bubble_sequences_count;) {
        const FrameBubble* const* bubble = &animation->frame_bubble_sequences[i];

        if((*bubble) == NULL) break;

        while((*bubble)->next_bubble != NULL) {
            bubble = &(*bubble)->next_bubble;
        }

        if((*bubble)->bubble.text) {
            free((void*)(*bubble)->bubble.text);
        }
        if((*bubble) == animation->frame_bubble_sequences[i]) {
            ++i;
        }
        free((void*)*bubble);
        FURRY_CONST_ASSIGN_PTR(*bubble, NULL);
    }
    free((void*)animation->frame_bubble_sequences);
    animation->frame_bubble_sequences = NULL;
}
