#pragma once

#include <furry.h>
#include <furry_hal.h>
extern void latch_tx_handler();
extern void unlatch_tx_handler(bool persist);
extern FurryStreamBuffer* tx_stream;
extern FurryStreamBuffer* rx_stream;