#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furry_hal_i2c.h>

/** Channel types */
typedef enum {
    LP5562ChannelRed = (1 << 0),
    LP5562ChannelGreen = (1 << 1),
    LP5562ChannelBlue = (1 << 2),
    LP5562ChannelWhite = (1 << 3),
} LP5562Channel;

typedef enum {
    LP5562Direct = 0,
    LP5562Engine1 = 1,
    LP5562Engine2 = 2,
    LP5562Engine3 = 3,
} LP5562Engine;

/** Initialize Driver */
void lp5562_reset(FurryHalI2cBusHandle* handle);

/** Configure Driver */
void lp5562_configure(FurryHalI2cBusHandle* handle);

/** Enable Driver */
void lp5562_enable(FurryHalI2cBusHandle* handle);

/** Set channel current */
void lp5562_set_channel_current(FurryHalI2cBusHandle* handle, LP5562Channel channel, uint8_t value);

/** Set channel PWM value */
void lp5562_set_channel_value(FurryHalI2cBusHandle* handle, LP5562Channel channel, uint8_t value);

/** Get channel PWM value */
uint8_t lp5562_get_channel_value(FurryHalI2cBusHandle* handle, LP5562Channel channel);

/** Set channel source */
void lp5562_set_channel_src(FurryHalI2cBusHandle* handle, LP5562Channel channel, LP5562Engine src);

/** Execute program sequence */
void lp5562_execute_program(
    FurryHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint16_t* program);

/** Stop program sequence */
void lp5562_stop_program(FurryHalI2cBusHandle* handle, LP5562Engine eng);

/** Execute ramp program sequence */
void lp5562_execute_ramp(
    FurryHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint8_t val_start,
    uint8_t val_end,
    uint16_t time);

/** Start blink program sequence */
void lp5562_execute_blink(
    FurryHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint16_t on_time,
    uint16_t period,
    uint8_t brightness);
