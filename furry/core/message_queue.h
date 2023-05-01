/**
 * @file message_queue.h
 * FurryMessageQueue
 */
#pragma once

#include "core/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void FurryMessageQueue;

/** Allocate furry message queue
 *
 * @param[in]  msg_count  The message count
 * @param[in]  msg_size   The message size
 *
 * @return     pointer to FurryMessageQueue instance
 */
FurryMessageQueue* furry_message_queue_alloc(uint32_t msg_count, uint32_t msg_size);

/** Free queue
 *
 * @param      instance  pointer to FurryMessageQueue instance
 */
void furry_message_queue_free(FurryMessageQueue* instance);

/** Put message into queue
 *
 * @param      instance  pointer to FurryMessageQueue instance
 * @param[in]  msg_ptr   The message pointer
 * @param[in]  timeout   The timeout
 * @param[in]  msg_prio  The message prio
 *
 * @return     The furry status.
 */
FurryStatus
    furry_message_queue_put(FurryMessageQueue* instance, const void* msg_ptr, uint32_t timeout);

/** Get message from queue
 *
 * @param      instance  pointer to FurryMessageQueue instance
 * @param      msg_ptr   The message pointer
 * @param      msg_prio  The message prioority
 * @param[in]  timeout   The timeout
 *
 * @return     The furry status.
 */
FurryStatus furry_message_queue_get(FurryMessageQueue* instance, void* msg_ptr, uint32_t timeout);

/** Get queue capacity
 *
 * @param      instance  pointer to FurryMessageQueue instance
 *
 * @return     capacity in object count
 */
uint32_t furry_message_queue_get_capacity(FurryMessageQueue* instance);

/** Get message size
 *
 * @param      instance  pointer to FurryMessageQueue instance
 *
 * @return     Message size in bytes
 */
uint32_t furry_message_queue_get_message_size(FurryMessageQueue* instance);

/** Get message count in queue
 *
 * @param      instance  pointer to FurryMessageQueue instance
 *
 * @return     Message count
 */
uint32_t furry_message_queue_get_count(FurryMessageQueue* instance);

/** Get queue available space
 *
 * @param      instance  pointer to FurryMessageQueue instance
 *
 * @return     Message count
 */
uint32_t furry_message_queue_get_space(FurryMessageQueue* instance);

/** Reset queue
 *
 * @param      instance  pointer to FurryMessageQueue instance
 *
 * @return     The furry status.
 */
FurryStatus furry_message_queue_reset(FurryMessageQueue* instance);

#ifdef __cplusplus
}
#endif
