/**
 * @file furry_hal_pwm.h
 * PWM contol HAL
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    FurryHalPwmOutputIdTim1PA7,
    FurryHalPwmOutputIdLptim2PA4,
} FurryHalPwmOutputId;

/** Enable PWM channel and set parameters
 * 
 * @param[in]  channel  PWM channel (FurryHalPwmOutputId)
 * @param[in]  freq  Frequency in Hz
 * @param[in]  duty  Duty cycle value in %
*/
void furry_hal_pwm_start(FurryHalPwmOutputId channel, uint32_t freq, uint8_t duty);

/** Disable PWM channel
 * 
 * @param[in]  channel  PWM channel (FurryHalPwmOutputId)
*/
void furry_hal_pwm_stop(FurryHalPwmOutputId channel);

/** Set PWM channel parameters
 * 
 * @param[in]  channel  PWM channel (FurryHalPwmOutputId)
 * @param[in]  freq  Frequency in Hz
 * @param[in]  duty  Duty cycle value in %
*/
void furry_hal_pwm_set_params(FurryHalPwmOutputId channel, uint32_t freq, uint8_t duty);

#ifdef __cplusplus
}
#endif
