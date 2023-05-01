/**
 * @file furry_hal_speaker.h
 * Speaker HAL
 */
#pragma once

#include <furry.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Init speaker */
void furry_hal_speaker_init();

/** Deinit speaker */
void furry_hal_speaker_deinit();

/** Acquire speaker ownership
 *
 * @warning    You must acquire speaker ownership before use
 *
 * @param      timeout  Timeout during which speaker ownership must be acquired
 *
 * @return     bool  returns true on success
 */
FURRY_WARN_UNUSED bool furry_hal_speaker_acquire(uint32_t timeout);

/** Release speaker ownership
 *
 * @warning    You must release speaker ownership after use
 */
void furry_hal_speaker_release();

/** Check current process speaker ownership
 *
 * @warning    always returns true if called from ISR
 *
 * @return     bool returns true if process owns speaker
 */
bool furry_hal_speaker_is_mine();

/** Play a note
 *
 * @warning    no ownership check if called from ISR
 *
 * @param      frequency  The frequency
 * @param      volume     The volume
 */
void furry_hal_speaker_start(float frequency, float volume);

/** Set volume
 *
 * @warning    no ownership check if called from ISR
 *
 * @param      volume  The volume
 */
void furry_hal_speaker_set_volume(float volume);

/** Stop playback
 *
 * @warning    no ownership check if called from ISR
 */
void furry_hal_speaker_stop();

#ifdef __cplusplus
}
#endif
