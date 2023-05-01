#pragma once
#include <furry.h>

#define err(...) FURRY_LOG_E("Heatshrink", "Error: %d-%s", __VA_ARGS__)