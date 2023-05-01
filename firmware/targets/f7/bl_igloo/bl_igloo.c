#include "bl_igloo.h"
#include "app_common.h"
#include "ble_app.h"
#include <ble/ble.h>

#include <interface/patterns/ble_thread/tl/tl.h>
#include <interface/patterns/ble_thread/shci/shci.h>
#include <interface/patterns/ble_thread/tl/shci_tl.h>
#include "app_debug.h"

#include <furry_hal.h>

#define TAG "Core2"

#define BL_IGLOO_FLAG_SHCI_EVENT (1UL << 0)
#define BL_IGLOO_FLAG_KILL_THREAD (1UL << 1)
#define BL_IGLOO_FLAG_ALL (BL_IGLOO_FLAG_SHCI_EVENT | BL_IGLOO_FLAG_KILL_THREAD)

#define POOL_SIZE                      \
    (CFG_TLBLE_EVT_QUEUE_LENGTH * 4U * \
     DIVC((sizeof(TL_PacketHeader_t) + TL_BLE_EVENT_FRAME_SIZE), 4U))

PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t bl_igloo_event_pool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t bl_igloo_system_cmd_buff;
PLACE_IN_SECTION("MB_MEM2")
ALIGN(4)
static uint8_t bl_igloo_system_spare_event_buff[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
PLACE_IN_SECTION("MB_MEM2")
ALIGN(4)
static uint8_t bl_igloo_ble_spare_event_buff[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];

typedef struct {
    FurryMutex* shci_mtx;
    FurrySemaphore* shci_sem;
    FurryThread* thread;
    BlIglooStatus status;
    BlIglooKeyStorageChangedCallback callback;
    BlIglooC2Info c2_info;
    void* context;
} BlIgloo;

static BlIgloo* bl_igloo = NULL;

static int32_t bl_igloo_shci_thread(void* argument);
static void bl_igloo_sys_status_not_callback(SHCI_TL_CmdStatus_t status);
static void bl_igloo_sys_user_event_callback(void* pPayload);

void bl_igloo_set_key_storage_changed_callback(
    BlIglooKeyStorageChangedCallback callback,
    void* context) {
    furry_assert(bl_igloo);
    furry_assert(callback);
    bl_igloo->callback = callback;
    bl_igloo->context = context;
}

void bl_igloo_init() {
    bl_igloo = malloc(sizeof(BlIgloo));
    bl_igloo->status = BlIglooStatusStartup;

#ifdef BL_IGLOO_DEBUG
    APPD_Init();
#endif

    // Initialize all transport layers
    TL_MM_Config_t tl_mm_config;
    SHCI_TL_HciInitConf_t SHci_Tl_Init_Conf;
    // Reference table initialization
    TL_Init();

    bl_igloo->shci_mtx = furry_mutex_alloc(FurryMutexTypeNormal);
    bl_igloo->shci_sem = furry_semaphore_alloc(1, 0);

    // FreeRTOS system task creation
    bl_igloo->thread = furry_thread_alloc_ex("BleShciDriver", 1024, bl_igloo_shci_thread, bl_igloo);
    furry_thread_start(bl_igloo->thread);

    // System channel initialization
    SHci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&bl_igloo_system_cmd_buff;
    SHci_Tl_Init_Conf.StatusNotCallBack = bl_igloo_sys_status_not_callback;
    shci_init(bl_igloo_sys_user_event_callback, (void*)&SHci_Tl_Init_Conf);

    /**< Memory Manager channel initialization */
    tl_mm_config.p_BleSpareEvtBuffer = bl_igloo_ble_spare_event_buff;
    tl_mm_config.p_SystemSpareEvtBuffer = bl_igloo_system_spare_event_buff;
    tl_mm_config.p_AsynchEvtPool = bl_igloo_event_pool;
    tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
    TL_MM_Init(&tl_mm_config);
    TL_Enable();

    /*
     * From now, the application is waiting for the ready event ( VS_HCI_C2_Ready )
     * received on the system channel before starting the Stack
     * This system event is received with bl_igloo_sys_user_event_callback()
     */
}

const BlIglooC2Info* bl_igloo_get_c2_info() {
    return &bl_igloo->c2_info;
}

BlIglooStatus bl_igloo_get_c2_status() {
    return bl_igloo->status;
}

static const char* bl_igloo_get_reltype_str(const uint8_t reltype) {
    static char relcode[3] = {0};
    switch(reltype) {
    case INFO_STACK_TYPE_BLE_FULL:
        return "F";
    case INFO_STACK_TYPE_BLE_HCI:
        return "H";
    case INFO_STACK_TYPE_BLE_LIGHT:
        return "L";
    case INFO_STACK_TYPE_BLE_BEACON:
        return "Be";
    case INFO_STACK_TYPE_BLE_BASIC:
        return "Ba";
    case INFO_STACK_TYPE_BLE_FULL_EXT_ADV:
        return "F+";
    case INFO_STACK_TYPE_BLE_HCI_EXT_ADV:
        return "H+";
    default:
        snprintf(relcode, sizeof(relcode), "%X", reltype);
        return relcode;
    }
}

static void bl_igloo_update_c2_fw_info() {
    WirelessFwInfo_t wireless_info;
    SHCI_GetWirelessFwInfo(&wireless_info);
    BlIglooC2Info* local_info = &bl_igloo->c2_info;

    local_info->VersionMajor = wireless_info.VersionMajor;
    local_info->VersionMinor = wireless_info.VersionMinor;
    local_info->VersionSub = wireless_info.VersionSub;
    local_info->VersionBranch = wireless_info.VersionBranch;
    local_info->VersionReleaseType = wireless_info.VersionReleaseType;

    local_info->MemorySizeSram2B = wireless_info.MemorySizeSram2B;
    local_info->MemorySizeSram2A = wireless_info.MemorySizeSram2A;
    local_info->MemorySizeSram1 = wireless_info.MemorySizeSram1;
    local_info->MemorySizeFlash = wireless_info.MemorySizeFlash;

    local_info->StackType = wireless_info.StackType;
    snprintf(
        local_info->StackTypeString,
        BL_IGLOO_MAX_VERSION_STRING_LEN,
        "%d.%d.%d:%s",
        local_info->VersionMajor,
        local_info->VersionMinor,
        local_info->VersionSub,
        bl_igloo_get_reltype_str(local_info->StackType));

    local_info->FusVersionMajor = wireless_info.FusVersionMajor;
    local_info->FusVersionMinor = wireless_info.FusVersionMinor;
    local_info->FusVersionSub = wireless_info.FusVersionSub;
    local_info->FusMemorySizeSram2B = wireless_info.FusMemorySizeSram2B;
    local_info->FusMemorySizeSram2A = wireless_info.FusMemorySizeSram2A;
    local_info->FusMemorySizeFlash = wireless_info.FusMemorySizeFlash;
}

static void bl_igloo_dump_stack_info() {
    const BlIglooC2Info* c2_info = &bl_igloo->c2_info;
    FURRY_LOG_I(
        TAG,
        "Core2: FUS: %d.%d.%d, mem %d/%d, flash %d pages",
        c2_info->FusVersionMajor,
        c2_info->FusVersionMinor,
        c2_info->FusVersionSub,
        c2_info->FusMemorySizeSram2B,
        c2_info->FusMemorySizeSram2A,
        c2_info->FusMemorySizeFlash);
    FURRY_LOG_I(
        TAG,
        "Core2: Stack: %d.%d.%d, branch %d, reltype %d, stacktype %d, flash %d pages",
        c2_info->VersionMajor,
        c2_info->VersionMinor,
        c2_info->VersionSub,
        c2_info->VersionBranch,
        c2_info->VersionReleaseType,
        c2_info->StackType,
        c2_info->MemorySizeFlash);
}

bool bl_igloo_wait_for_c2_start(int32_t timeout) {
    bool started = false;

    do {
        // TODO: use mutex?
        started = bl_igloo->status == BlIglooStatusC2Started;
        if(!started) {
            timeout--;
            furry_delay_tick(1);
        }
    } while(!started && (timeout > 0));

    if(started) {
        FURRY_LOG_I(
            TAG,
            "C2 boot completed, mode: %s",
            bl_igloo->c2_info.mode == BlIglooC2ModeFUS ? "FUS" : "Stack");
        bl_igloo_update_c2_fw_info();
        bl_igloo_dump_stack_info();
    } else {
        FURRY_LOG_E(TAG, "C2 startup failed");
        bl_igloo->status = BlIglooStatusBroken;
    }

    return started;
}

bool bl_igloo_start() {
    furry_assert(bl_igloo);

    if(bl_igloo->status != BlIglooStatusC2Started) {
        return false;
    }

    bool ret = false;
    if(ble_app_init()) {
        FURRY_LOG_I(TAG, "Radio stack started");
        bl_igloo->status = BlIglooStatusRadioStackRunning;
        ret = true;
        if(SHCI_C2_SetFlashActivityControl(FLASH_ACTIVITY_CONTROL_SEM7) == SHCI_Success) {
            FURRY_LOG_I(TAG, "Flash activity control switched to SEM7");
        } else {
            FURRY_LOG_E(TAG, "Failed to switch flash activity control to SEM7");
        }
    } else {
        FURRY_LOG_E(TAG, "Radio stack startup failed");
        bl_igloo->status = BlIglooStatusRadioStackMissing;
        ble_app_thread_stop();
    }

    return ret;
}

bool bl_igloo_is_alive() {
    if(!bl_igloo) {
        return false;
    }

    return bl_igloo->status >= BlIglooStatusC2Started;
}

bool bl_igloo_is_radio_stack_ready() {
    if(!bl_igloo) {
        return false;
    }

    return bl_igloo->status == BlIglooStatusRadioStackRunning;
}

BlIglooCommandResult bl_igloo_force_c2_mode(BlIglooC2Mode desired_mode) {
    furry_check(desired_mode > BlIglooC2ModeUnknown);

    if(desired_mode == bl_igloo->c2_info.mode) {
        return BlIglooCommandResultOK;
    }

    if((bl_igloo->c2_info.mode == BlIglooC2ModeFUS) && (desired_mode == BlIglooC2ModeStack)) {
        if((bl_igloo->c2_info.VersionMajor == 0) && (bl_igloo->c2_info.VersionMinor == 0)) {
            FURRY_LOG_W(TAG, "Stack isn't installed!");
            return BlIglooCommandResultError;
        }
        SHCI_CmdStatus_t status = SHCI_C2_FUS_StartWs();
        if(status) {
            FURRY_LOG_E(TAG, "Failed to start Radio Stack with status: %02X", status);
            return BlIglooCommandResultError;
        }
        return BlIglooCommandResultRestartPending;
    }
    if((bl_igloo->c2_info.mode == BlIglooC2ModeStack) && (desired_mode == BlIglooC2ModeFUS)) {
        SHCI_FUS_GetState_ErrorCode_t error_code = 0;
        uint8_t fus_state = SHCI_C2_FUS_GetState(&error_code);
        FURRY_LOG_D(TAG, "FUS state: %X, error = %x", fus_state, error_code);
        if(fus_state == SHCI_FUS_CMD_NOT_SUPPORTED) {
            // Second call to SHCI_C2_FUS_GetState() restarts whole MCU & boots FUS
            fus_state = SHCI_C2_FUS_GetState(&error_code);
            FURRY_LOG_D(TAG, "FUS state#2: %X, error = %x", fus_state, error_code);
            return BlIglooCommandResultRestartPending;
        }
        return BlIglooCommandResultOK;
    }

    return BlIglooCommandResultError;
}

static void bl_igloo_sys_status_not_callback(SHCI_TL_CmdStatus_t status) {
    switch(status) {
    case SHCI_TL_CmdBusy:
        furry_mutex_acquire(bl_igloo->shci_mtx, FurryWaitForever);
        break;
    case SHCI_TL_CmdAvailable:
        furry_mutex_release(bl_igloo->shci_mtx);
        break;
    default:
        break;
    }
}

/*
 * The type of the payload for a system user event is tSHCI_UserEvtRxParam
 * When the system event is both :
 *    - a ready event (subevtcode = SHCI_SUB_EVT_CODE_READY)
 *    - reported by the FUS (sysevt_ready_rsp == FUS_FW_RUNNING)
 * The buffer shall not be released
 * ( eg ((tSHCI_UserEvtRxParam*)pPayload)->status shall be set to SHCI_TL_UserEventFlow_Disable )
 * When the status is not filled, the buffer is released by default
 */
static void bl_igloo_sys_user_event_callback(void* pPayload) {
    UNUSED(pPayload);

#ifdef BL_IGLOO_DEBUG
    APPD_EnableCPU2();
#endif

    TL_AsynchEvt_t* p_sys_event =
        (TL_AsynchEvt_t*)(((tSHCI_UserEvtRxParam*)pPayload)->pckt->evtserial.evt.payload);

    if(p_sys_event->subevtcode == SHCI_SUB_EVT_CODE_READY) {
        FURRY_LOG_I(TAG, "Core2 started");
        SHCI_C2_Ready_Evt_t* p_c2_ready_evt = (SHCI_C2_Ready_Evt_t*)p_sys_event->payload;
        if(p_c2_ready_evt->sysevt_ready_rsp == WIRELESS_FW_RUNNING) {
            bl_igloo->c2_info.mode = BlIglooC2ModeStack;
        } else if(p_c2_ready_evt->sysevt_ready_rsp == FUS_FW_RUNNING) {
            bl_igloo->c2_info.mode = BlIglooC2ModeFUS;
        }

        bl_igloo->status = BlIglooStatusC2Started;
    } else if(p_sys_event->subevtcode == SHCI_SUB_EVT_ERROR_NOTIF) {
        FURRY_LOG_E(TAG, "Error during initialization");
    } else if(p_sys_event->subevtcode == SHCI_SUB_EVT_BLE_NVM_RAM_UPDATE) {
        SHCI_C2_BleNvmRamUpdate_Evt_t* p_sys_ble_nvm_ram_update_event =
            (SHCI_C2_BleNvmRamUpdate_Evt_t*)p_sys_event->payload;
        if(bl_igloo->callback) {
            bl_igloo->callback(
                (uint8_t*)p_sys_ble_nvm_ram_update_event->StartAddress,
                p_sys_ble_nvm_ram_update_event->Size,
                bl_igloo->context);
        }
    }
}

static void bl_igloo_clear_shared_memory() {
    memset(bl_igloo_event_pool, 0, sizeof(bl_igloo_event_pool));
    memset(&bl_igloo_system_cmd_buff, 0, sizeof(bl_igloo_system_cmd_buff));
    memset(bl_igloo_system_spare_event_buff, 0, sizeof(bl_igloo_system_spare_event_buff));
    memset(bl_igloo_ble_spare_event_buff, 0, sizeof(bl_igloo_ble_spare_event_buff));
}

void bl_igloo_thread_stop() {
    if(bl_igloo) {
        FurryThreadId thread_id = furry_thread_get_id(bl_igloo->thread);
        furry_assert(thread_id);
        furry_thread_flags_set(thread_id, BL_IGLOO_FLAG_KILL_THREAD);
        furry_thread_join(bl_igloo->thread);
        furry_thread_free(bl_igloo->thread);
        // Free resources
        furry_mutex_free(bl_igloo->shci_mtx);
        furry_semaphore_free(bl_igloo->shci_sem);
        bl_igloo_clear_shared_memory();
        free(bl_igloo);
        bl_igloo = NULL;
    }
}

// Wrap functions
static int32_t bl_igloo_shci_thread(void* context) {
    UNUSED(context);
    uint32_t flags = 0;

    while(true) {
        flags = furry_thread_flags_wait(BL_IGLOO_FLAG_ALL, FurryFlagWaitAny, FurryWaitForever);
        if(flags & BL_IGLOO_FLAG_SHCI_EVENT) {
            shci_user_evt_proc();
        }
        if(flags & BL_IGLOO_FLAG_KILL_THREAD) {
            break;
        }
    }

    return 0;
}

void shci_notify_asynch_evt(void* pdata) {
    UNUSED(pdata);
    if(bl_igloo) {
        FurryThreadId thread_id = furry_thread_get_id(bl_igloo->thread);
        furry_assert(thread_id);
        furry_thread_flags_set(thread_id, BL_IGLOO_FLAG_SHCI_EVENT);
    }
}

void shci_cmd_resp_release(uint32_t flag) {
    UNUSED(flag);
    if(bl_igloo) {
        furry_semaphore_release(bl_igloo->shci_sem);
    }
}

void shci_cmd_resp_wait(uint32_t timeout) {
    UNUSED(timeout);
    if(bl_igloo) {
        furry_hal_power_insomnia_enter();
        furry_semaphore_acquire(bl_igloo->shci_sem, FurryWaitForever);
        furry_hal_power_insomnia_exit();
    }
}

bool bl_igloo_reinit_c2() {
    return SHCI_C2_Reinit() == SHCI_Success;
}

BlIglooCommandResult bl_igloo_fus_stack_delete() {
    FURRY_LOG_I(TAG, "Erasing stack");
    SHCI_CmdStatus_t erase_stat = SHCI_C2_FUS_FwDelete();
    FURRY_LOG_I(TAG, "Cmd res = %x", erase_stat);
    if(erase_stat == SHCI_Success) {
        return BlIglooCommandResultOperationOngoing;
    }
    bl_igloo_fus_get_status();
    return BlIglooCommandResultError;
}

BlIglooCommandResult bl_igloo_fus_stack_install(uint32_t src_addr, uint32_t dst_addr) {
    FURRY_LOG_I(TAG, "Installing stack");
    SHCI_CmdStatus_t write_stat = SHCI_C2_FUS_FwUpgrade(src_addr, dst_addr);
    FURRY_LOG_I(TAG, "Cmd res = %x", write_stat);
    if(write_stat == SHCI_Success) {
        return BlIglooCommandResultOperationOngoing;
    }
    bl_igloo_fus_get_status();
    return BlIglooCommandResultError;
}

BlIglooCommandResult bl_igloo_fus_get_status() {
    furry_check(bl_igloo->c2_info.mode == BlIglooC2ModeFUS);
    SHCI_FUS_GetState_ErrorCode_t error_code = 0;
    uint8_t fus_state = SHCI_C2_FUS_GetState(&error_code);
    FURRY_LOG_I(TAG, "FUS state: %x, error: %x", fus_state, error_code);
    if((error_code != 0) || (fus_state == FUS_STATE_VALUE_ERROR)) {
        return BlIglooCommandResultError;
    } else if(
        (fus_state >= FUS_STATE_VALUE_FW_UPGRD_ONGOING) &&
        (fus_state <= FUS_STATE_VALUE_SERVICE_ONGOING_END)) {
        return BlIglooCommandResultOperationOngoing;
    }
    return BlIglooCommandResultOK;
}

BlIglooCommandResult bl_igloo_fus_wait_operation() {
    furry_check(bl_igloo->c2_info.mode == BlIglooC2ModeFUS);

    while(true) {
        BlIglooCommandResult fus_status = bl_igloo_fus_get_status();
        if(fus_status == BlIglooCommandResultOperationOngoing) {
            furry_delay_ms(20);
        } else if(fus_status == BlIglooCommandResultError) {
            return BlIglooCommandResultError;
        } else {
            return BlIglooCommandResultOK;
        }
    }
}
