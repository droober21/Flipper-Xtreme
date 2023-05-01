/**
 * @file furry_hal_bt.h
 * BT/BLE HAL API
 */

#pragma once

#include <furry.h>
#include <stdbool.h>
#include <gap.h>
#include <serial_service.h>
#include <ble_glue.h>
#include <ble_app.h>

#include <furry_hal_bt_serial.h>

#define FURRY_HAL_BT_STACK_VERSION_MAJOR (1)
#define FURRY_HAL_BT_STACK_VERSION_MINOR (12)
#define FURRY_HAL_BT_C2_START_TIMEOUT 1000

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FurryHalBtStackUnknown,
    FurryHalBtStackLight,
    FurryHalBtStackFull,
} FurryHalBtStack;

typedef enum {
    FurryHalBtProfileSerial,
    FurryHalBtProfileHidKeyboard,

    // Keep last for Profiles number calculation
    FurryHalBtProfileNumber,
} FurryHalBtProfile;

/** Initialize
 */
void furry_hal_bt_init();

/** Lock core2 state transition */
void furry_hal_bt_lock_core2();

/** Lock core2 state transition */
void furry_hal_bt_unlock_core2();

/** Start radio stack
 *
 * @return  true on successfull radio stack start
 */
bool furry_hal_bt_start_radio_stack();

/** Get radio stack type
 *
 * @return  FurryHalBtStack instance
 */
FurryHalBtStack furry_hal_bt_get_radio_stack();

/** Check if radio stack supports BLE GAT/GAP
 *
 * @return  true if supported
 */
bool furry_hal_bt_is_ble_gatt_gap_supported();

/** Check if radio stack supports testing
 *
 * @return  true if supported
 */
bool furry_hal_bt_is_testing_supported();

/** Start BLE app
 *
 * @param profile   FurryHalBtProfile instance
 * @param event_cb  GapEventCallback instance
 * @param context   pointer to context
 *
 * @return          true on success
*/
bool furry_hal_bt_start_app(FurryHalBtProfile profile, GapEventCallback event_cb, void* context);

/** Reinitialize core2
 * 
 * Also can be used to prepare core2 for stop modes
 */
void furry_hal_bt_reinit();

/** Change BLE app
 * Restarts 2nd core
 *
 * @param profile   FurryHalBtProfile instance
 * @param event_cb  GapEventCallback instance
 * @param context   pointer to context
 *
 * @return          true on success
*/
bool furry_hal_bt_change_app(FurryHalBtProfile profile, GapEventCallback event_cb, void* context);

/** Update battery level
 *
 * @param battery_level battery level
 */
void furry_hal_bt_update_battery_level(uint8_t battery_level);

/** Update battery power state */
void furry_hal_bt_update_power_state();

/** Checks if BLE state is active
 *
 * @return          true if device is connected or advertising, false otherwise
 */
bool furry_hal_bt_is_active();

/** Start advertising
 */
void furry_hal_bt_start_advertising();

/** Stop advertising
 */
void furry_hal_bt_stop_advertising();

/** Get BT/BLE system component state
 *
 * @param[in]  buffer  FurryString* buffer to write to
 */
void furry_hal_bt_dump_state(FurryString* buffer);

/** Get BT/BLE system component state
 *
 * @return     true if core2 is alive
 */
bool furry_hal_bt_is_alive();

/** Get key storage buffer address and size
 *
 * @param      key_buff_addr  pointer to store buffer address
 * @param      key_buff_size  pointer to store buffer size
 */
void furry_hal_bt_get_key_storage_buff(uint8_t** key_buff_addr, uint16_t* key_buff_size);

/** Get SRAM2 hardware semaphore
 * @note Must be called before SRAM2 read/write operations
 */
void furry_hal_bt_nvm_sram_sem_acquire();

/** Release SRAM2 hardware semaphore
 * @note Must be called after SRAM2 read/write operations
 */
void furry_hal_bt_nvm_sram_sem_release();

/** Clear key storage
 *
 * @return      true on success
*/
bool furry_hal_bt_clear_white_list();

/** Set key storage change callback
 *
 * @param       callback    BleGlueKeyStorageChangedCallback instance
 * @param       context     pointer to context
 */
void furry_hal_bt_set_key_storage_change_callback(
    BleGlueKeyStorageChangedCallback callback,
    void* context);

/** Start ble tone tx at given channel and power
 *
 * @param[in]  channel  The channel
 * @param[in]  power    The power
 */
void furry_hal_bt_start_tone_tx(uint8_t channel, uint8_t power);

/** Stop ble tone tx
 */
void furry_hal_bt_stop_tone_tx();

/** Start sending ble packets at a given frequency and datarate
 *
 * @param[in]  channel   The channel
 * @param[in]  pattern   The pattern
 * @param[in]  datarate  The datarate
 */
void furry_hal_bt_start_packet_tx(uint8_t channel, uint8_t pattern, uint8_t datarate);

/** Stop sending ble packets
 *
 * @return     sent packet count
 */
uint16_t furry_hal_bt_stop_packet_test();

/** Start receiving packets
 *
 * @param[in]  channel   RX channel
 * @param[in]  datarate  Datarate
 */
void furry_hal_bt_start_packet_rx(uint8_t channel, uint8_t datarate);

/** Set up the RF to listen to a given RF channel
 *
 * @param[in]  channel  RX channel
 */
void furry_hal_bt_start_rx(uint8_t channel);

/** Stop RF listenning
 */
void furry_hal_bt_stop_rx();

/** Get RSSI
 *
 * @return     RSSI in dBm
 */
float furry_hal_bt_get_rssi();

/** Get number of transmitted packets
 *
 * @return     packet count
 */
uint32_t furry_hal_bt_get_transmitted_packets();

/** Check & switch C2 to given mode
 *
 * @param[in]  mode  mode to switch into
 */
bool furry_hal_bt_ensure_c2_mode(BleGlueC2Mode mode);

/** Modify profile advertisement name and restart bluetooth
 * @param[in] profile   profile type
 * @param[in] name      new adv name
*/
void furry_hal_bt_set_profile_adv_name(
    FurryHalBtProfile profile,
    const char name[FURRY_HAL_BT_ADV_NAME_LENGTH]);

const char* furry_hal_bt_get_profile_adv_name(FurryHalBtProfile profile);

/** Modify profile mac address and restart bluetooth
 * @param[in] profile   profile type
 * @param[in] mac       new mac address
*/
void furry_hal_bt_set_profile_mac_addr(
    FurryHalBtProfile profile,
    const uint8_t mac_addr[GAP_MAC_ADDR_SIZE]);

const uint8_t* furry_hal_bt_get_profile_mac_addr(FurryHalBtProfile profile);

uint32_t furry_hal_bt_get_conn_rssi(uint8_t* rssi);

void furry_hal_bt_set_profile_pairing_method(FurryHalBtProfile profile, GapPairing pairing_method);

GapPairing furry_hal_bt_get_profile_pairing_method(FurryHalBtProfile profile);

bool furry_hal_bt_is_connected(void);

#ifdef __cplusplus
}
#endif
