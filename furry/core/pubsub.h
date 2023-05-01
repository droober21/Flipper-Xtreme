/**
 * @file pubsub.h
 * FurryPubSub
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** FurryPubSub Callback type */
typedef void (*FurryPubSubCallback)(const void* message, void* context);

/** FurryPubSub type */
typedef struct FurryPubSub FurryPubSub;

/** FurryPubSubSubscription type */
typedef struct FurryPubSubSubscription FurryPubSubSubscription;

/** Allocate FurryPubSub
 *
 * Reentrable, Not threadsafe, one owner
 *
 * @return     pointer to FurryPubSub instance
 */
FurryPubSub* furry_pubsub_alloc();

/** Free FurryPubSub
 * 
 * @param      pubsub  FurryPubSub instance
 */
void furry_pubsub_free(FurryPubSub* pubsub);

/** Subscribe to FurryPubSub
 * 
 * Threadsafe, Reentrable
 * 
 * @param      pubsub            pointer to FurryPubSub instance
 * @param[in]  callback          The callback
 * @param      callback_context  The callback context
 *
 * @return     pointer to FurryPubSubSubscription instance
 */
FurryPubSubSubscription*
    furry_pubsub_subscribe(FurryPubSub* pubsub, FurryPubSubCallback callback, void* callback_context);

/** Unsubscribe from FurryPubSub
 * 
 * No use of `pubsub_subscription` allowed after call of this method
 * Threadsafe, Reentrable.
 *
 * @param      pubsub               pointer to FurryPubSub instance
 * @param      pubsub_subscription  pointer to FurryPubSubSubscription instance
 */
void furry_pubsub_unsubscribe(FurryPubSub* pubsub, FurryPubSubSubscription* pubsub_subscription);

/** Publish message to FurryPubSub
 *
 * Threadsafe, Reentrable.
 * 
 * @param      pubsub   pointer to FurryPubSub instance
 * @param      message  message pointer to publish
 */
void furry_pubsub_publish(FurryPubSub* pubsub, void* message);

#ifdef __cplusplus
}
#endif
