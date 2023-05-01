#pragma once
#include <furry/furry.h>

/*
Testing 10000 api calls

No lock
    Time diff: 445269.218750 us
    Time per call: 44.526920 us

furry_thread_flags
    Time diff: 430279.875000 us // lol wtf? smaller than no lock?
    Time per call: 43.027988 us // I tested it many times, it's always smaller

FurryEventFlag
    Time diff: 831523.625000 us
    Time per call: 83.152359 us

FurrySemaphore
    Time diff: 999807.125000 us
    Time per call: 99.980713 us

FurryMutex
    Time diff: 1071417.500000 us
    Time per call: 107.141747 us
*/

typedef FurryEventFlag* FurryApiLock;

#define API_LOCK_EVENT (1U << 0)

#define api_lock_alloc_locked() furry_event_flag_alloc()

#define api_lock_wait_unlock(_lock) \
    furry_event_flag_wait(_lock, API_LOCK_EVENT, FurryFlagWaitAny, FurryWaitForever)

#define api_lock_free(_lock) furry_event_flag_free(_lock)

#define api_lock_unlock(_lock) furry_event_flag_set(_lock, API_LOCK_EVENT)

#define api_lock_wait_unlock_and_free(_lock) \
    api_lock_wait_unlock(_lock);             \
    api_lock_free(_lock);
