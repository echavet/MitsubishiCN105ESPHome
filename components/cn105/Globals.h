#pragma once
#include "cn105_types.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#endif

#include <esphome.h>
#include "esphome/components/uart/uart.h"

#define CUSTOM_MILLIS esphome::millis()
#define CUSTOM_DELAY(x) esphome::delay(x)
