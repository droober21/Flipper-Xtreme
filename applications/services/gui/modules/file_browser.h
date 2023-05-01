/**
 * @file file_browser.h
 * GUI: FileBrowser view module API
 */

#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CUSTOM_ICON_MAX_SIZE 32

typedef struct FileBrowser FileBrowser;
typedef void (*FileBrowserCallback)(void* context);

typedef bool (*FileBrowserLoadItemCallback)(
    FurryString* path,
    void* context,
    uint8_t** icon,
    FurryString* item_name);

FileBrowser* file_browser_alloc(FurryString* result_path);

void file_browser_free(FileBrowser* browser);

View* file_browser_get_view(FileBrowser* browser);

void file_browser_configure(
    FileBrowser* browser,
    const char* extension,
    const char* base_path,
    bool skip_assets,
    bool hide_dot_files,
    const Icon* file_icon,
    bool hide_ext);

void file_browser_start(FileBrowser* browser, FurryString* path);

void file_browser_stop(FileBrowser* browser);

void file_browser_set_callback(FileBrowser* browser, FileBrowserCallback callback, void* context);

void file_browser_set_item_callback(
    FileBrowser* browser,
    FileBrowserLoadItemCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
