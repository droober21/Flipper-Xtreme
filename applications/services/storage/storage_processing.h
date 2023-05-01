#pragma once
#include <furry.h>
#include "storage.h"
#include "storage_i.h"
#include "storage_message.h"
#include "storage_glue.h"

#ifdef __cplusplus
extern "C" {
#endif

FS_Error storage_get_data(Storage* app, FurryString* path, StorageData** storage);

void storage_process_message(Storage* app, StorageMessage* message);

#ifdef __cplusplus
}
#endif
