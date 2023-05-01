#include "pubsub.h"
#include "memmgr.h"
#include "check.h"
#include "mutex.h"

#include <m-list.h>

struct FurryPubSubSubscription {
    FurryPubSubCallback callback;
    void* callback_context;
};

LIST_DEF(FurryPubSubSubscriptionList, FurryPubSubSubscription, M_POD_OPLIST);

struct FurryPubSub {
    FurryPubSubSubscriptionList_t items;
    FurryMutex* mutex;
};

FurryPubSub* furry_pubsub_alloc() {
    FurryPubSub* pubsub = malloc(sizeof(FurryPubSub));

    pubsub->mutex = furry_mutex_alloc(FurryMutexTypeNormal);
    furry_assert(pubsub->mutex);

    FurryPubSubSubscriptionList_init(pubsub->items);

    return pubsub;
}

void furry_pubsub_free(FurryPubSub* pubsub) {
    furry_assert(pubsub);

    furry_check(FurryPubSubSubscriptionList_size(pubsub->items) == 0);

    FurryPubSubSubscriptionList_clear(pubsub->items);

    furry_mutex_free(pubsub->mutex);

    free(pubsub);
}

FurryPubSubSubscription*
    furry_pubsub_subscribe(FurryPubSub* pubsub, FurryPubSubCallback callback, void* callback_context) {
    furry_check(furry_mutex_acquire(pubsub->mutex, FurryWaitForever) == FurryStatusOk);
    // put uninitialized item to the list
    FurryPubSubSubscription* item = FurryPubSubSubscriptionList_push_raw(pubsub->items);

    // initialize item
    item->callback = callback;
    item->callback_context = callback_context;

    furry_check(furry_mutex_release(pubsub->mutex) == FurryStatusOk);

    return item;
}

void furry_pubsub_unsubscribe(FurryPubSub* pubsub, FurryPubSubSubscription* pubsub_subscription) {
    furry_assert(pubsub);
    furry_assert(pubsub_subscription);

    furry_check(furry_mutex_acquire(pubsub->mutex, FurryWaitForever) == FurryStatusOk);
    bool result = false;

    // iterate over items
    FurryPubSubSubscriptionList_it_t it;
    for(FurryPubSubSubscriptionList_it(it, pubsub->items); !FurryPubSubSubscriptionList_end_p(it);
        FurryPubSubSubscriptionList_next(it)) {
        const FurryPubSubSubscription* item = FurryPubSubSubscriptionList_cref(it);

        // if the iterator is equal to our element
        if(item == pubsub_subscription) {
            FurryPubSubSubscriptionList_remove(pubsub->items, it);
            result = true;
            break;
        }
    }

    furry_check(furry_mutex_release(pubsub->mutex) == FurryStatusOk);
    furry_check(result);
}

void furry_pubsub_publish(FurryPubSub* pubsub, void* message) {
    furry_check(furry_mutex_acquire(pubsub->mutex, FurryWaitForever) == FurryStatusOk);

    // iterate over subscribers
    FurryPubSubSubscriptionList_it_t it;
    for(FurryPubSubSubscriptionList_it(it, pubsub->items); !FurryPubSubSubscriptionList_end_p(it);
        FurryPubSubSubscriptionList_next(it)) {
        const FurryPubSubSubscription* item = FurryPubSubSubscriptionList_cref(it);
        item->callback(message, item->callback_context);
    }

    furry_check(furry_mutex_release(pubsub->mutex) == FurryStatusOk);
}
