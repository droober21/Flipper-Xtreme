/**
 * @file event_flag.h
 * Furry Event Flag
 */
#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void FurryEventFlag;

/** Allocate FurryEventFlag
 *
 * @return     pointer to FurryEventFlag
 */
FurryEventFlag* furry_event_flag_alloc();

/** Deallocate FurryEventFlag
 *
 * @param      instance  pointer to FurryEventFlag
 */
void furry_event_flag_free(FurryEventFlag* instance);

/** Set flags
 *
 * @param      instance  pointer to FurryEventFlag
 * @param[in]  flags     The flags
 *
 * @return     Resulting flags or error (FurryStatus)
 */
uint32_t furry_event_flag_set(FurryEventFlag* instance, uint32_t flags);

/** Clear flags
 *
 * @param      instance  pointer to FurryEventFlag
 * @param[in]  flags     The flags
 *
 * @return     Resulting flags or error (FurryStatus)
 */
uint32_t furry_event_flag_clear(FurryEventFlag* instance, uint32_t flags);

/** Get flags
 *
 * @param      instance  pointer to FurryEventFlag
 *
 * @return     Resulting flags
 */
uint32_t furry_event_flag_get(FurryEventFlag* instance);

/** Wait flags
 *
 * @param      instance  pointer to FurryEventFlag
 * @param[in]  flags     The flags
 * @param[in]  options   The option flags
 * @param[in]  timeout   The timeout
 *
 * @return     Resulting flags or error (FurryStatus)
 */
uint32_t furry_event_flag_wait(
    FurryEventFlag* instance,
    uint32_t flags,
    uint32_t options,
    uint32_t timeout);

#ifdef __cplusplus
}
#endif
