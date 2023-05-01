#pragma once

#include "serial_service.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FURRY_HAL_BT_SERIAL_PACKET_SIZE_MAX SERIAL_SVC_DATA_LEN_MAX

typedef enum {
    FurryHalBtSerialRpcStatusNotActive,
    FurryHalBtSerialRpcStatusActive,
} FurryHalBtSerialRpcStatus;

/** Serial service callback type */
typedef SerialServiceEventCallback FurryHalBtSerialCallback;

/** Start Serial Profile
 */
void furry_hal_bt_serial_start();

/** Stop Serial Profile
 */
void furry_hal_bt_serial_stop();

/** Set Serial service events callback
 *
 * @param buffer_size   Applicaition buffer size
 * @param calback       FurryHalBtSerialCallback instance
 * @param context       pointer to context
 */
void furry_hal_bt_serial_set_event_callback(
    uint16_t buff_size,
    FurryHalBtSerialCallback callback,
    void* context);

/** Set BLE RPC status
 *
 * @param status        FurryHalBtSerialRpcStatus instance
 */
void furry_hal_bt_serial_set_rpc_status(FurryHalBtSerialRpcStatus status);

/** Notify that application buffer is empty
 */
void furry_hal_bt_serial_notify_buffer_is_empty();

/** Send data through BLE
 *
 * @param data  data buffer
 * @param size  data buffer size
 *
 * @return      true on success
 */
bool furry_hal_bt_serial_tx(uint8_t* data, uint16_t size);

#ifdef __cplusplus
}
#endif
