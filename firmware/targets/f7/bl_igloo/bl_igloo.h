#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BlIglooC2ModeUnknown = 0,
    BlIglooC2ModeFUS,
    BlIglooC2ModeStack,
} BlIglooC2Mode;

#define BL_IGLOO_MAX_VERSION_STRING_LEN 20
typedef struct {
    BlIglooC2Mode mode;
    /**
     * Wireless Info
     */
    uint8_t VersionMajor;
    uint8_t VersionMinor;
    uint8_t VersionSub;
    uint8_t VersionBranch;
    uint8_t VersionReleaseType;
    uint8_t MemorySizeSram2B; /*< Multiple of 1K */
    uint8_t MemorySizeSram2A; /*< Multiple of 1K */
    uint8_t MemorySizeSram1; /*< Multiple of 1K */
    uint8_t MemorySizeFlash; /*< Multiple of 4K */
    uint8_t StackType;
    char StackTypeString[BL_IGLOO_MAX_VERSION_STRING_LEN];
    /**
     * Fus Info
     */
    uint8_t FusVersionMajor;
    uint8_t FusVersionMinor;
    uint8_t FusVersionSub;
    uint8_t FusMemorySizeSram2B; /*< Multiple of 1K */
    uint8_t FusMemorySizeSram2A; /*< Multiple of 1K */
    uint8_t FusMemorySizeFlash; /*< Multiple of 4K */
} BlIglooC2Info;

typedef enum {
    // Stage 1: core2 startup and FUS
    BlIglooStatusStartup,
    BlIglooStatusBroken,
    BlIglooStatusC2Started,
    // Stage 2: radio stack
    BlIglooStatusRadioStackRunning,
    BlIglooStatusRadioStackMissing
} BlIglooStatus;

typedef void (
    *BlIglooKeyStorageChangedCallback)(uint8_t* change_addr_start, uint16_t size, void* context);

/** Initialize start core2 and initialize transport */
void bl_igloo_init();

/** Start Core2 Radio stack
 *
 * @return     true on success
 */
bool bl_igloo_start();

/** Is core2 alive and at least FUS is running
 * 
 * @return     true if core2 is alive
 */
bool bl_igloo_is_alive();

/** Waits for C2 to reports its mode to callback
 *
 * @return     true if it reported before reaching timeout
 */
bool bl_igloo_wait_for_c2_start(int32_t timeout);

BlIglooStatus bl_igloo_get_c2_status();

const BlIglooC2Info* bl_igloo_get_c2_info();

/** Is core2 radio stack present and ready
 *
 * @return     true if present and ready
 */
bool bl_igloo_is_radio_stack_ready();

/** Set callback for NVM in RAM changes
 *
 * @param[in]  callback  The callback to call on NVM change
 * @param      context   The context for callback
 */
void bl_igloo_set_key_storage_changed_callback(
    BlIglooKeyStorageChangedCallback callback,
    void* context);

/** Stop SHCI thread */
void bl_igloo_thread_stop();

bool bl_igloo_reinit_c2();

typedef enum {
    BlIglooCommandResultUnknown,
    BlIglooCommandResultOK,
    BlIglooCommandResultError,
    BlIglooCommandResultRestartPending,
    BlIglooCommandResultOperationOngoing,
} BlIglooCommandResult;

/** Restart MCU to launch radio stack firmware if necessary
 *
 * @return      true on radio stack start command
 */
BlIglooCommandResult bl_igloo_force_c2_mode(BlIglooC2Mode mode);

BlIglooCommandResult bl_igloo_fus_stack_delete();

BlIglooCommandResult bl_igloo_fus_stack_install(uint32_t src_addr, uint32_t dst_addr);

BlIglooCommandResult bl_igloo_fus_get_status();

BlIglooCommandResult bl_igloo_fus_wait_operation();

#ifdef __cplusplus
}
#endif
