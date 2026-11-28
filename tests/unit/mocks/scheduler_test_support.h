/// scheduler_test_support.h — Force-included when compiling request_scheduler.cpp for tests.
/// Supplies the log macros and a controllable clock in place of Globals.h / esphome.h.
#pragma once

#include <cstdint>
#include "esphome_stubs.h"

uint32_t scheduler_test_clock_ms();

#define CUSTOM_MILLIS (scheduler_test_clock_ms())
